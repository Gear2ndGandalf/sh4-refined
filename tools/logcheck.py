r"""logcheck.py: a short, actionable report of an SH4 Refined log (v3.33 LOG333; standard library only, read only).

    python logcheck.py [game folder | log files ...] [--full]

With no argument it reads SH4Refined.log and SH4Refined.prev.log in the GOG game folder. Each log file is one game
session (the mod rewrites SH4Refined.log at every start and keeps the previous one as .prev.log). The report per session:
the version / exe / Playable Eileen / dgVoodoo2 / log level line, the settings that are not default, how the session
ended (a missing end line = a crash or a killed process; a crash dump of the same time is named), the rooms visited,
the ANOMALY lines grouped and counted, faults and failed installs, the worst frames, and per feature "acted in N of M
rooms it was on" with the features that never acted while on. Older logs (v3.32 and before, no session / room lines)
still give the version, settings, rooms (from the features' own room lines), faults and the noisiest repeated lines.
--full lists every group instead of the first few.

v3.40 LG340: the game folder's logs\ keeps the last 8 sessions' logs ("SH4Refined 2026-11-22 0724 v3.40.0.log"). With a
game folder, the report covers SH4Refined.log and the session before it, then a comparison table of every session found
(SH4Refined.log, logs\*, SH4Refined.prev.log unless logs\ holds the same file): start, version, renderer (the Renderer
row's dgVoodoo conf at the start and the Direct3D runtime the display chain loaded), length, rooms, play time, frame
mean / p99 / worst in play, spikes, stalls, the perf overlay's 5-minute summaries, ANOMALY lines and how it ended, and
the same per renderer, so "which renderer felt best" can be read off. Several log files given: a report each and the
table. --table: the table only; --all: a report for every session.
"""
import datetime
import hashlib
import os
import re
import sys
from collections import Counter, OrderedDict

GAME_DIR = os.environ.get('SH4_GAME_DIR', r'C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4')
TS = re.compile(r'^(\d\d):(\d\d):(\d\d)\.(\d\d\d) ')
LEVEL = re.compile(r'^\[(v|t)\] ')
ROOM_LINE = re.compile(r'^room: ([0-9A-F]+):([0-9A-F]+) left after ([\d.]+) s \(play ([\d.]+), cutscene ([\d.]+), paused ([\d.]+)\);'
                       r' (?:(\d+) frames: mean ([\d.]+) ms, p99 ([\d.]+), worst ([\d.]+) at \+([\d.]+) s \(([^)]*)\), spikes (\d+)'
                       r' \(over (\d+) ms\), stalls (\d+)|no play frames; stalls (\d+))')
LOAD_LINE = re.compile(r'^load: game time (-?\d+) .*?stage ([0-9A-F]+) room ([0-9A-F]+).*?ledger ([0-9A-F ]+) \(this world visit '
                       r'(\d+)(?: of (\d+)|, no budget: drops fade)\), pickup records free (\d+) of (\d+)')  # v3.35: no budget while drops fade
FEAT_LINE = re.compile(r'^room: ([0-9A-F]+):([0-9A-F]+) features: (.*)$')
FEAT_ITEM = re.compile(r'^(?P<name>[A-Za-z ]+)=(?P<val>.+?) acted (?P<n>-?\d+) ')
# v3.35 FX335: the effect budget (budget.inc) and the warm-up (echo.inc RfWarmFrame)
BUDGET_ROOM = re.compile(r'^budget: room ([0-9A-F]+):([0-9A-F]+): level (\d+) now \(([^)]*)\), at most (\d+) this room; (\d+) step\(s\) down, '
                         r'(\d+) up; (\d+) % of its play frames thinned; the effects ([\d.]+) ms a frame on average, ([\d.]+) at most '
                         r'\(target ([\d.]+)\); ([\d.]+) % of the frames late')
BUDGET_STEP = re.compile(r'^budget: level (\d+) -> (\d+) ')
BUDGET_IN = re.compile(r'^budget: (in|off) ')
# v3.56 PW356: the effect budget's high-rate fallback and After Shock's over-the-shoulder fade
BUDGET_HR_IN = re.compile(r'^budget: high rate fallback (in|off) ')
BUDGET_HR_RULES = re.compile(r'^budget: (\d+) fps: (the high-rate rules|v3\.55\'s rules)')
BUDGET_BLIND = re.compile(r'^budget: level (\d+) -> (\d+) \(frames run over, no CPU evidence: a blind try;')
BUDGET_BACK = re.compile(r'^budget: level (\d+) -> (\d+) \(a blind try taken back: the frames (-?[\d.]+) % shorter')
BUDGET_KEPT = re.compile(r'^budget: the blind step to level (\d+) is kept: the frames (-?[\d.]+) % shorter')
BUDGET_YIELD = re.compile(r'^budget: nothing left to shed at (\d+) fps \(level (\d+)')
BUDGET_ROOM_HR = re.compile(r'; high rate: (\d+) frame\(s\), (\d+) blind step\(s\), (\d+) kept, (\d+) taken back; the sparks at (\d+) %')
# v3.58 GX358: the quick rules (a start step when powers start together, the refill, quick steps) and the room line's part
BUDGET_QUICK_IN = re.compile(r'^budget: quick rules (on|off)')
BUDGET_PRE = re.compile(r'^budget: level (\d+) -> (\d+) \(powers start together at (\d+) fps: (.*?): a start step')
BUDGET_REFILL = re.compile(r'^budget: level (\d+) -> (\d+) \((the powers fill back in|late again as they filled in)')
BUDGET_QUICK = re.compile(r'^budget: level (\d+) -> (\d+) \(frames run far over: a quick step;')
BUDGET_ROOM_GX = re.compile(r'; (\d+) start step\(s\), (\d+) quick step\(s\)')
AI_OTS_CAM = re.compile(r'^after shock: over-the-shoulder camera (on|off): ')
AI_OTS_FADE = re.compile(r'^after shock: over-the-shoulder fade: a copy right over her keeps (\d+) % of its light; her box (\d+) x (\d+) u')
AI_OTS_INI = re.compile(r'^after shock: \[Gameplay\] AfterImageOtsFade=(-?\d+)')
AI_OTS_ROOM = re.compile(r'^after shock: over-the-shoulder fade in stage ([0-9A-F]+) room ([0-9A-F]+): on (\d+) frame\(s\), (\d+) copy draw\(s\) '
                         r'dimmed, the least light (\d+) %')
WARM_LINE = re.compile(r'^warm-up: stage ([0-9A-F]+) room ([0-9A-F]+): (.*); ([\d.]+) ms in all')
WARM_PART = re.compile(r'(screen effects|death presence|echo|after shock) ([\d.]+) ms')
# v3.41 DP341: Death Presence's draw gate (deathpresence_draw.inc DpGateFrame): hidden during views of another place, a load,
# a room change settling
DPG_HIDE = re.compile(r'^death presence: stage ([0-9A-F]+) room ([0-9A-F]+): hidden: (.+)$')
DPG_SHOWN = re.compile(r'^death presence: stage ([0-9A-F]+) room ([0-9A-F]+): shown again after ([\d.]+) s hidden \((.+?); '
                       r'still for (\d+) frames\)')
DPG_LONG = re.compile(r'^death presence: stage ([0-9A-F]+) room ([0-9A-F]+): still hidden after 20 s: (.+?) \(her model drawn')
DPG_WARP = re.compile(r'^death presence: stage [0-9A-F]+ room [0-9A-F]+: a move of the player stayed pending')
# v3.42 SM342: Death Presence off in Room 302 (the install line, one line per entry); Echo Doubles / After Shock hidden in
# views (deathpresence_draw.inc DpViewFrame); the shadows' Darkness; Curse Meter Off / On
SM_R302_IN = re.compile(r'^death presence: Room 302 \(stages 1 and 0A\): (on|off)\b')
SM_R302 = re.compile(r'^death presence: Room 302 \(stage ([0-9A-F]+) room ([0-9A-F]+)\): (on|off here)\b')
SM_VIEW_HIDE = re.compile(r'^(echo|after shock): hidden in a view: (.+?) \(stage ([0-9A-F]+) room ([0-9A-F]+)\)$')
SM_VIEW_SHOWN = re.compile(r'^(echo|after shock): shown again after ([\d.]+) s hidden \((.+)\)$')
SH_DARK = re.compile(r'^shadows: Darkness (\d+) % \(of the game')
SH_DARK_USE = re.compile(r'^shadows: Darkness (\d+) % on (Stock|Soft): darkness ([0-9A-F]{8}),.*?: inside ([\d.]+) of the game.s ([\d.]+)')
# v3.43 SH343: the c1.a the soft composite drew with, read back from the device (in play / the Shadows page's right half)
SH_C1 = re.compile(r'^shadows: (in play the composite|the Shadows page\'s right half) drew darkness ([0-9A-F]{8}) at Darkness (\d+) %'
                   r'(?::| with) c1\.a ([\d.]+) (read back|\(not read back).*?lets (\d+) % of the light through')
HB_CM42 = re.compile(r'^hud bars: Curse Meter Off / On \(v3\.42\)')
HB_CM42_OLD = re.compile(r'^hud bars: \[Gameplay\] CurseMeter=(\S+) \(v3\.41 or older\) reads as (On|Off)')
# v3.36 HE336: the hit-shape pool's and the enemy records' room lines (hitpool.inc HpTick, recevict.inc EvTick)
HIT_ROOM = re.compile(r'^hit shapes: room (\d+):(\d+) left: lowest free (-?\d+) of (\d+) \((-?\d+) in use at most\), '
                      r'(\d+) refusal\(s\) here \((\d+) this session\), stomp guard (\d+) here')
REC_ROOM = re.compile(r'^enemy records: room (\d+):(\d+) left: (\d+) of 64 in use now \((\d+) copies\'\), (\d+) at most at '
                      r'its exit\'s (\d+) write\(s\); (\d+) copies\' record\(s\) purged; evictions: (\d+) copies\' record\(s\), '
                      r'(\d+) real \((\d+) of them for a copy\'s write\)')
# v3.36 TP336: the task / IK pools (pools.inc): the install line and the per-room use line
POOL_TASKS = re.compile(r'^task pool: (\d+) slots \(was 300\)|^task pool: the game\'s 300 slots')
POOL_IK = re.compile(r'^IK pool: (\d+) handles \(was 160|^IK pool: the game\'s 160 handles')
POOLS_ROOM = re.compile(r'^pools: room ([0-9A-F]+):([0-9A-F]+) left: tasks (\d+) of (\d+) in use at most \(fewest free (\d+); '
                        r'slots 300\+ held (\d+) at most\), (?:IK handles (\d+) in use at most \(fewest free (\d+) of (\d+)\)|'
                        r'IK list unreadable[^,]*), actor records fewest free (-?\d+) of 300, heap (\d+) of 65536 units at most')
# v3.37 MN337: Supply Drops' given / faded / made-way / refused lines (drops.inc), for the 64 / 10 caps
DROP_GIVEN = re.compile(r'^supply drops: ([A-Z][A-Za-z ]+?) from (?:a copy of |type [0-9A-F]{2}/|ghost |Cynthia|Jasper|Andrew|Richard|Walter)')
DROP_FADED = re.compile(r'^supply drops: .+ faded after \d+ s')
DROP_WAY = re.compile(r'^supply drops: the cap of (\d+) lying drops: .+ is taken away')
DROP_NO = re.compile(r"^supply drops: no drop: .+? won but (the room's cap of drops lying here|the cap of drops lying|too few free pickup records)"
                     r".*?; (\d+) lying, cap (\d+);")
DROP_CAPS = re.compile(r'^supply drops: v[\d.]+: .*\(lying caps (\d+) anywhere / (\d+) a room, (\d+) records kept free')
DROP_RENUM = re.compile(r'^supply drops: drop numbers renumbered 1\.\.(\d+)')
# v3.38 DR338: the level (install line, a menu change, an ini re-read, v3.37's Generous read as Abundant), the four levels'
# chances and the equal shares (drops.inc DropsInstall), the drops given per item group
DROP_LEVEL_IN = re.compile(r'^supply drops: in \(.*\); level (\w+)$')
DROP_LEVEL_SET = re.compile(r'^gameplay: (?:\[Gameplay\] )?SupplyDrops (?:read again: )?(\w+)$')
DROP_LEVEL_ALIAS = re.compile(r'^gameplay: SupplyDrops Generous \(v3\.29 - v3\.37\) reads as Abundant')
# v3.38 DR338c: the final design's levels line (the level factors, the Normal class table) and the shares line
DROP_LEVELS_LINE = re.compile(r"^supply drops: levels Off / Rare / Normal / Abundant \(Normal's chances x([\d.]+) / x([\d.]+) / "
                              r"x([\d.]+)\): one roll a kill or full down, common / uncommon / rare on Normal: (.*?) \(any item ([\d.]+) % ")
DROP_CLASS_ITEM = re.compile(r'(weak|strong|ghost|named ghost|Walter) ([\d.]+) / ([\d.]+) / ([\d.]+) %')
DROP_SHARES = re.compile(r'^supply drops: shares in each group: common [A-Za-z ]+? (\d+) / [A-Za-z ]+? (\d+) / [A-Za-z ]+? (\d+), '
                         r'uncommon [A-Za-z ]+? (\d+) / [A-Za-z ]+? (\d+), rare [A-Za-z ]+? (\d+) / [A-Za-z ]+? (\d+) / '
                         r'[A-Za-z ]+? (\d+) / [A-Za-z ]+? (\d+);')
DROP_GROUPS = (('common', (('Nutrition Drink', 'Drink'), ('Pistol Bullets', 'Pistol'), ('Revolver Bullets', 'Revolver'))),
               ('uncommon', (('Portable Medical Kit', 'Kit'), ('Holy Candle', 'Candle'))),  # v3.38 DR338b: groups by rarity (DR338c: Kit 2 : Candle 1)
               ('rare', (('Saint Medallion', 'Medallion'), ('Ampoule', 'Ampoule'), ('Silver Bullets', 'Silver'),
                         ('Sword of Obedience', 'Sword'))))
DROP_LEVEL_NAMES = ('Rare', 'Normal', 'Abundant')
# v3.37 CP337: the performance overlay's summary lines, the Direct3D chain (perfhud.inc), the pause Camera page (extramenu.inc)
PERF_SUM = re.compile(r'^perf: (the last 5 minutes|the overlay went off): ([\d.]+) s, (\d+) frames, ([\d.]+) fps on average, the lowest '
                      r'1% low ([\d.]+) fps \(now ([\d.]+), p99 ([\d.]+) ms\), worst frame ([\d.]+) ms, Refined ([\d.]+) ms a frame '
                      r'\((\d+) ledger frames\), address space (\d+) of (\d+) MB \(most (\d+)\)')
PERF_BACKEND = re.compile(r'^perf: (Direct3D: .*?) \(Option 4')
# v3.39 PR339: the Renderer row (presentation.inc / renderer.inc): the start line, a replace, a refusal; the Opening skip
REND_START = re.compile(r'^renderer: (.*); row (greyed|open) \(([^)]*)\); manifest ')
REND_SWITCH = re.compile(r'^renderer: (.+?) \(([^,()]+), ([0-9A-F]{32})\) -> (.+?) \(([^,()]+), ([0-9A-F]{32})\): dgVoodoo\.conf replaced')
REND_FAIL = re.compile(r'^renderer: (.+?) \(([^,()]+)\): not replaced: (.*); dgVoodoo\.conf left as it was')
OPEN_IN = re.compile(r'^opening: (Full|Skip Logos|Fast) \(\[Window\] Opening\)')
OPEN_SKIP = re.compile(r'^opening: the boot screens skipped .*?, (\d+) ms after the start')
OPEN_SAVED = re.compile(r'^opening: (Full|Skip Logos|Fast) saved \(\[Window\] Opening\)')
# v3.38 HB338: the curse meter under Hide Gauge (hudbars.inc), the Performance readout's place (perfhud.inc)
HB_HIDE = re.compile(r'^hud bars: Hide Gauge (on|off): ')
HB_HIDE_RING = re.compile(r'^hud bars: Hide Gauge: (the curse meter|companion Eileen\'s small ring) (shown|faded out): (.*?) \(gauge fade')
# v3.41 CM341: who draws the player's curse ring (hudbars.inc HbPeOwnCheck: Playable Eileen v1.23.1+ or Refined)
# v3.52 HB352: the enemy bars refused for an enemy the game does not draw yet / with no place (hudbars.inc HbRefuseNote,
# HbRefRoomEnd) and the bosses' first bar (HbBossFrame)
HB_REF = re.compile(r'^hud bars: no bar for ([0-9A-F]{8}) \(type ([0-9A-F]{2})\) in room (\d+:[0-9A-F]+): (not in the room yet|not at a place in the room)')
HB_REF_ROOM = re.compile(r'^hud bars: room (\d+:[0-9A-F]+): no bar for (\d+) enemy\(ies\) not in the room yet .*?, (\d+) not at a place in the '
                         r'room; this session (\d+) / (\d+)')
# v3.58 EN358: a refusal for a body nothing draws yet (the event dogs before flag 130), Walter's downs
HB_UNDRAWN = re.compile(r'^hud bars: no bar for [0-9A-F]{8} .*?\((nothing draws it: no model in its actor record|an event dog '
                        r'before story flag 130: its task returns before its draw)\)')
HB_WALTER = re.compile(r'^hud bars: (the final|the stalker) Walter [0-9A-F]{8} (standing|rising|stumble|down|pinned|gone) '
                       r'\(class [0-9A-F]+, AI [0-9A-F]+ step \d+, hp (-?\d+) / (\d+), down timer ([\d.]+) s\)')
HB_WALTER_SITE = re.compile(r'^hud bars: Walter downed keeps his bar .*?: the stalker\'s \(004A4A12\) (read|differs[^,]*), the '
                            r'final Walter\'s \(004A8760\) (read|differs.*)$')
HB_BOSS = re.compile(r'^hud bars: boss (Walter|the Wall boss) ([0-9A-F]{8}) \(type ([0-9A-F]{2})\) in room (\d+:[0-9A-F]+): '
                     r'(?:its bar is up ([\d.]+) s after it became active|active 5 s and no bar yet: (.*?) \(state)')
# v3.59 FT359: the enemy bars' looks under the special filters (hudbars.inc HbInit / HbThemeFrame / HbThemeHides)
FT_SET = re.compile(r'^hud bars: filter themes (on|off) \(\[ScreenEffects\] FilterThemes=(-?\d+), hidden\)')
FT_LOOK = re.compile(r'^hud bars: the enemy bars wear the (Thermal Vision|Spirit Vision|Night Vision|Night Mono|stock) look '
                     r'\((?:Filter Strength (\w+)|Filter ([^;)]+))')
FT_HID1 = re.compile(r'^hud bars: Thermal Vision: no bar for the victims .*?the first: type ([0-9A-F]{2}) in room (\d+:[0-9A-F]+)')
FT_HIDR = re.compile(r'^hud bars: room (\d+:[0-9A-F]+): Thermal Vision drew no bar for (\d+) victim\(s\) .*?; (\d+) this session\)')
HB_PE_RING = re.compile(r"^hud bars: (?:Playable Eileen draws the player's curse ring \(SH4PE_DrawsPlayerCurse\)|"
                        r"SH4PE_DrawsPlayerCurse (is not exported|\(\) returned -?\d+))")
PERF_PLACE = re.compile(r'^perf: readout (at the top-left corner, clear of|under|right of|at the top-left corner, over) (.+?) \((\d+)x(\d+), S')
CAM_PAGE = re.compile(r'^extra menu: Camera page (opened|closed)(?:: (.*))?')
# v3.51 MU351: the sound in the Extra Menu's Storage Chest / Save Game / Load Game (extramenu.inc MuLogSound)
MU_SET = re.compile(r'^extra menu: sound in the Storage Chest / Save Game / Load Game: (paused as in the pause menu|plays quietly as '
                    r'in v3\.50) \(\[ExtraMenu\] PauseSound=([01])\)$')
MU_LINE = re.compile(r'^extra menu: sound in the (load list|save menu|storage chest): (opened from the pause|opened from play|closed, '
                     r'back in the pause|closed, back to play|closed, a save loads[^:]*): pause mode (-?\d+), master ([\d.]+), fade '
                     r'(-?\d+): the level music (stays paused|PLAYS quietly|comes back|PLAYS)( \(PauseSound=0: v3\.50\))?$')
MU_LISTS = ('load list', 'save menu', 'storage chest')
# v3.52 QL352: Quick Load inside the pause (extramenu.inc QlPauseStart / ql_go / QlSoundTick, quicksave.inc QsLoad)
QL_SET = re.compile(r'^extra menu: Quick Load (runs inside the pause(?: \([^)]*\))?|closes the pause first \(v3\.51\)) \(\[ExtraMenu\] '
                    r'QuickLoadInPause=([01])(?:: (.*))?\)$')
QL_LOAD = re.compile(r'^quick load: slot (\d) \(sequence \d+\), stage (\d+) room (\d+), game time \d+(?:; path: (.*?))?'
                     r'(?: \(black in (\d+) frames(, NOT black: loaded anyway)?; pause mode (-?\d+), master ([\d.]+), fade (-?\d+): '
                     r'the level music (stays paused|PLAYS quietly|comes back|PLAYS)\))?$')
QL_BACK = re.compile(r'^quick load: the loaded room runs after (\d+) ms \(stage \d+ room \d+\): the level music fades in '
                     r'\(0x56E960\(-1\) from pause mode -?\d+\)(: NOT seen running)?$')
QL_DROP = re.compile(r'^quick load: the loaded room opened a menu or a cut-scene first ')
QL_REFUSED = re.compile(r'^quick load: refused at black: (.*?); back in the pause')
# v3.56 PV356: the Effects page's preview (preview.inc): the settings, each pause's look layers, the page's backdrop / layer
PV_SET = re.compile(r"^screen effects: the Effects page's preview: with no paused game (SH4 Refined's own scene|PreviewImage, else "
                    r"SH4 Refined's own scene) \(PreviewScene=(\w+)\); a pause keeps .*?: (on|off) \(PreviewLayers=(\d)\)$")
PV_KEPT = re.compile(r"^screen effects: the pause's look layers kept for the Effects page: heat (on|off) \((\d+) bod(?:y|ies), (\d+) "
                     r"footprint\(s\)\), spirit (on|off) \((\d+) ghost\(s\), (\d+) soul fire\(s\), (\d+) aura sprite\(s\)\); "
                     r"(\d+)x(\d+), (targets made|targets kept from an earlier pause) \(the preview's ([\d.]+) MB\), ([\d.]+) ms$")
PV_NOT = re.compile(r"^screen effects: the pause's look layers not kept: (.+?) \(Thermal / Spirit Vision show ")
PV_FAIL = re.compile(r"^screen effects: the pause's look layers (faulted|could not be copied)")
# v3.59 TV359: Thermal Vision: walls hide heat (the light grid draws without a depth write while it is on), the room lines,
# the first kept frame, the player's footprint shape (Henry's boots / Eileen's heels)
TV_IN = re.compile(r'^screen effects: Thermal Vision: walls hide heat \(the light grid 004EFBDC ')
TV_OFF = re.compile(r"^screen effects: Thermal Vision: (\[ScreenEffects\] ThermalTrueDepth=0|the light grid's call 004EFBDC is not "
                    r"stock|004EFBDC could not be written)")
TV_ROOM = re.compile(r'^screen effects: Thermal Vision: stage (\d+) room ([0-9A-F]+) left: (\d+) heat frame\(s\); the light grid '
                     r'drew in (\d+), keeping the depth in (\d+) \((.+?)\); footprints depth-tested in (\d+)$')
TV_FIRST = re.compile(r"^screen effects: Thermal Vision: first frame with the scene's depth kept under the light grid \(stage (\d+) "
                      r"room ([0-9A-F]+)\)")
TV_SHAPE = re.compile(r"^screen effects: Thermal Vision: the player's footprints are (Henry's boots|Eileen's heels|the old oval) "
                      r"\((.+)\)$")
PV_PAGE = re.compile(r"^screen effects: the Effects page's preview: (.+?) over (the paused scene \(the pause menu\)|the paused scene|"
                     r"SH4 Refined's preview scene|the preview picture \(PreviewImage\)|the stock hallway|"
                     r"the stock picture \(SH4Refined_preview_stock\.dds\))(?: with (.+?))?"  # v3.58 PV358: + the stock picture
                     r"( \(the layer could not be drawn\))?(?: \(Filter Strength (Low|Medium|Ultra)\))?$")  # v3.58 PV358: + the strength
# v3.58 PV358: Preview Off / Stock / Live, Filter Strength (the install line, the migrations, the stock file, the copy)
PV_SET358 = re.compile(r"^screen effects: the Effects page's preview: Preview=(Off|Stock|Live) \(.*?\); the stock picture (.*?): "
                       r"(present|missing)[^;]*; a pause keeps .*?: (on|off) \(PreviewLayers=(\d)\); Filter Strength (Low|Medium|High|Ultra)$")
PV_OFF = re.compile(r"^screen effects: the Effects page's preview: off \(Preview Off\) over the game's own menu background \(page (\d+)\)$")
PV_MIG = re.compile(r"^screen effects: \[ScreenEffects\] PreviewScene=(.*?) \(v3\.56(?:, hidden)?\) (?:is now Preview=(\w+)|is not read any more)")
PV_FSLEG = re.compile(r"^screen effects: \[ScreenEffects\] FilterStrength=(.*?) is v3\.15's")
PV_STOCKBAD = re.compile(r"^screen effects: the stock picture (.*?) is (missing|not usable)")
PV_FSFAIL = re.compile(r"^screen effects: Filter Strength (\w+): the plain picture could not be copied")
PV_LAID = re.compile(r"^screen effects: the preview scene laid out for (\d+)x(\d+): ")
# v3.59 PV359: F7's look layers beside its picture, the picture layers the page loads, the Shadows page with Preview Stock
PV_F7 = re.compile(r"^screen effects: F7's look layers for (.+?): heat (saved|none) (.*?); spirit (saved|none) (.*?)"
                   r"(; the older layer file\(s\) of that name removed)?; ([\d.]+) ms$")
PV_F7LATE = re.compile(r"^screen effects: F7: no picture taken: the game left play")
PV_F7FAIL = re.compile(r"^screen effects: F7's look layers (faulted|could not be copied)")
PV_LAY = re.compile(r"^screen effects: the picture's (heat|spirit) layer (.*?): (used|no file|not used: (.+?)) \((?:\d|Thermal|Spirit)")
PV_SHSTOCK = re.compile(r"^screen effects: the Shadows page's preview: Preview (Stock|Live): the still picture alone: (.+?) "
                        r"\(the split shadows only over a paused game")
PV_STOCKLAY = re.compile(r": present \((with its heat and spirit layer files|with its heat layer file|with its spirit layer file|"
                         r"no layer files)\);")
MU_EVENTS = (('opened from the pause', 'from the pause'), ('opened from play', 'from play'),
             ('closed, back in the pause', 'back in the pause'), ('closed, back to play', 'back to play'),
             ('closed, a save loads', 'a save loads'))
# v3.52 SND352: Sound (sound.inc): the install line, the first frame's start line, the row / page changes, the first start's
# migration, the Sound byte / engine fixes, the minute lines (level 2)
SND_IN = re.compile(r'^sound: in: pan law (Smooth|Stock|NOT IN)[^,]*, volume hooks (in|NOT IN) \(Night Mode\), Room Reverb (in|NOT IN)'
                    r'[^,]*, Mono fold (in|NOT IN|off)(?: \(MonoFold=0\))? \(streams\) / (in|NOT IN|off) \(music\)')
SND_START = re.compile(r"^sound: Mode (\w+) \(the game's Sound (\d+), engine (-?\d+)\), pan law (.+?), Stereo Width (\d+) %, "
                       r"Headphones floor -(\d+) dB, Room Reverb (On|Off) \(([^)]*)\), Night Mode (\w+), Voice volume not in this "
                       r"build, Mono fold (on|off); speakers (.+)$")
SND_FIRST = re.compile(r"^sound: first start: \[Sound\] Mode=(\w+), from the save's Option 1 Sound (-?\d+)")
SND_BADMODE = re.compile(r'^sound: \[Sound\] Mode="(.*)" is not a mode .*?: (\w+), from the save')
SND_DIFF = re.compile(r"^sound: the save's Sound (-?\d+) .*? differs from \[Sound\] Mode=(\w+)")
SND_CHANGE = re.compile(r'^sound: (Mode|Stereo Width|Room Reverb|Night Mode|Background) (\S+) -> (\S+?)(?: %)? \(([^)]*)\)')  # v3.56 SD356: + Background
SND_BYTEFIX = re.compile(r"^sound: the Options block's Sound came in as (-?\d+)")
SND_ENGFIX = re.compile(r'^sound: the engine was told Sound (-?\d+) but the Options say (\d+)')
SND_DISC = re.compile(r'^sound: Options discarded: the engine back to Sound (\d+) \(it kept (-?\d+)')
SND_MIN = re.compile(r'^sound: minute: volumes 2D (\d+), 3D (\d+), streams (\d+) \(Night Mode (\w+): (\d+) changed\); pans 2D (\d+) '
                     r'\(one ear only (\d+); the stock law (\d+)\), streams (\d+) \(one ear only (\d+); the stock law (\d+)\); Mono: '
                     r'(\d+) stream block\(s\) folded, (\d+) music fold\(s\)')
SND_NOTIN = re.compile(r'^sound: (.+?): ([0-9A-F]{8}) is not the GOG v7 code')
SND_OFF = re.compile(r'^sound: \[Sound\] Enabled=0')
# v3.56 SD356: Background, the keep-awake signal, focus changes, the device watch (sound.inc)
SD_BG = re.compile(r'^sound: background: \[Sound\] Background (Mute|Play) \(the buffer sites [0-9A-F /]+ (in|NOT IN)[^)]*\); '
                   r'keep-awake (on|off)[^;]*; device watch (on|off)')
SD_KEEP_ON = re.compile(r'^sound: keep-awake: on: a 45 Hz tone at (-?\d+\.\d) dBFS \(level (\d+) of 32767')
SD_KEEP_NONE = re.compile(r'^sound: keep-awake: no sound device: none$')
SD_KEEP_RE = re.compile(r'^sound: keep-awake: (?:it had stopped \(status ([0-9A-F]{8})(, the buffer was lost)?\)|GetStatus hr ([0-9A-F]{8}))')
SD_KEEP_FAIL = re.compile(r'^sound: keep-awake: DirectSound would not make or play its buffer')  # an ANOMALY line's body
SD_FOCUS_AWAY = re.compile(r'^sound: focus: another window is in front \(the game was in front (\d+\.\d) s\): Background (Mute|Play): ')
SD_FOCUS_BACK = re.compile(r'^sound: focus: the game is in front again after (\d+\.\d) s behind other windows')
SD_FOCUS_CAP = re.compile(r'^sound: focus: 40 changes logged this session')
SD_DEV_GAME = re.compile(r'^sound: device: the game plays on "([^"]*)"(?: \((.*?)\); a change|; the device watch could not register \(hr ([0-9A-F]{8})\))')
SD_DEV_NEW = re.compile(r'^sound: device: the default sound device changed to "([^"]*)"; restart the game to use it')
SD_DEV_BACK = re.compile(r'^sound: device: the default sound device is the game\'s "([^"]*)" again')
SD_DEV_STATE = re.compile(r'^sound: device: the game\'s sound device "([^"]*)" is (.+?)(: no game sound| again \()')
SD_DEV_OFF = re.compile(r'^sound: device watch: (.+)$')
SD_CFG = re.compile(r'^sound: \[Sound\] (Background|KeepAwakeLevel)=')
# v3.58 SD358: Volume 0 = silent; Background Mute by the sound session (sound.inc)
SD_ZERO_IN = re.compile(r'^sound: volume 0: (silent|off|NOT IN)(?:: .*?2D sounds and the music (in|NOT IN)[^,]*, streams (in|NOT IN)'
                        r'.*?\(the fading stream 0x2774 (by its set level|NOT IN))?')
SD_ZERO_MIN = re.compile(r'^sound: minute: volume 0: (\d+) 2D, (\d+) 3D, (\d+) stream volume\(s\) set to silence')
SD_SESS = re.compile(r'^sound: background: Mute by the sound session: (\d+) session\(s\) of the game on (\d+) output device')
SD_SESS_NONE = re.compile(r'^sound: background: no sound session of the game to mute \(([^)]*)\)')
SD_SESS_MUTED = re.compile(r'^sound: background: muted (\d+)( more)? sound session\(s\) of the game \((\d+) output device\(s\); (\d+) ms; '
                           r'(\d+) already muted')
SD_SESS_UNMUTED = re.compile(r'^sound: background: unmuted (\d+) sound session\(s\)')
SD_SESS_FIXED = re.compile(r'^sound: background: (\d+) sound session\(s\) of the game were still muted by SH4 Refined')
SD_SESS_USER = re.compile(r'^sound: background: (\d+) sound session\(s\) of the game are muted in Windows')
SD_SESS_BACK = re.compile(r'^sound: background: the game\'s sound session can be muted again')
# v3.58 SD358-001: SH4Refined.muted by session identity; the user's change of a held session in the Volume Mixer
SD_SESS_KEPT = re.compile(r'^sound: background: SH4Refined\.muted names (\d+) session\(s\) not found at this start')
SD_SESS_DROP = re.compile(r'^sound: background: (\d+) entr(?:y|ies) of SH4Refined\.muted older than 30 days dropped')
SD_MARK_FAIL = re.compile(r'^sound: background: SH4Refined\.muted could not be written \((.+?) failed, error (\d+)\)')  # v3.58 SD358-002
# v3.58 SD358-003: pending entries at the start (a mute not confirmed when the game ended): left as they are, dropped (not an
# anomaly: the user's mute is kept; when Refined's own, the line tells the user to unmute it in the Volume Mixer)
SD_SESS_PEND = re.compile(r'^sound: background: SH4Refined\.muted named (\d+) sound session\(s\) of the game as pending .*?: left as they '
                          r'are, (\d+) of them muted now')
SD_SESS_TOUCH = re.compile(r'^sound: background: a sound session SH4 Refined had muted was (unmuted|unmuted and muted again) in the '
                           r'Windows Volume Mixer')
SD_SITES = re.compile(r'^sound: background: the buffer sites (now (Play|Mute)|could not be set)')
SD_STALE = re.compile(r'^sound: background: (\d+) of the game\'s sounds carry the global focus')
SD_EXIT = re.compile(r'^sound: background: the window closes behind other windows: the game\'s sound sessions unmuted first \((done|no answer)')
SD_KEEP_HOLD = re.compile(r'^sound: keep-awake: (stopped while another window is in front|playing again)')
# v3.59 SD359: the lost-device guard (sound.inc, snddummy_sim.h): its install line, the first frame's Play2D / Play3D decision,
# the first stand-in buffer, the count, a second try that made it, the guard off
SD_GUARD_IN = re.compile(r'^sound: lost-device guard: PlayMusic 0040A1DE (in|NOT IN), the streams 0040A797 (in|NOT IN); (.+?): a '
                         r'CreateSoundBuffer that fails')
SD_GUARD_FIRST = re.compile(r'^sound: lost-device guard: at the first frame: (.+)$')
SD_GUARD_OFF = re.compile(r'^sound: lost-device guard: off \(\[Sound\] LostDeviceGuard=0\)')
SD_GONE_FIRST = re.compile(r'^sound: the device is gone: CreateSoundBuffer (failed|faulted) in (.+?) \(hr ([0-9A-F]{8})')
SD_GONE_N = re.compile(r'^sound: the device is gone: (\d+) sound\(s\) made silent \(2D (\d+), 3D (\d+), music (\d+), streams (\d+);')
SD_RETRY = re.compile(r'^sound: CreateSoundBuffer failed in (.+?) \(hr ([0-9A-F]{8})\): the second try made it')
# v3.59 SD359: the fault watch (loghealth.inc, faultwhere_sim.h): each fault's code, place and module; the Direct3D stop
FW_LINE = re.compile(r'^fault watch: ([0-9A-F]{8}) (.+?)(?: \((?:read|write|execute|access) of [0-9A-F]{8}\))? at (\S+?)\+([0-9A-F]{8}) '
                     r'\([0-9A-Fa-fx]+\), (the game thread|thread \d+), \d+ ms ago: (inside [^;]+?)(?:; Refined\'s (.+) guard caught it)?$')
FW_CAP = re.compile(r'^fault watch: 24 faults logged this session')
FW_IN = re.compile(r'^session: fault watch (in|NOT IN|off)')
FW_END = re.compile(r'^session: faults seen (\d+) \(inside SH4 Refined (\d+), the game (\d+), Direct3D / the driver (\d+), other (\d+), '
                    r'none (\d+); (\d+) not read\): (.*?); Refined\'s Direct3D work (stopped|kept)')
FW_STOP = re.compile(r'^direct3d: two faults within a second .*?\(the last ([0-9A-F]{8} at \S+)\)')
SD_SESS_LOST = re.compile(r'^sound: background: (no sound session of the game could be muted|\d+ sound session\(s\) of the game could not be '
                          r'(?:un)?muted)')  # ANOMALY bodies
# v3.53 OL2: the Options look (look.inc): the install line, the glyph line of each page (level 2), its ANOMALY, a key on a
# grey row (level 2)
LKL_IN = re.compile(r'^options look: (in|off) ')
LKL_GLYPH = re.compile(r'^options: page (\d+) \((.*?)\): about (\d+) menu glyphs \(title (\d+), page strip (\d+), rows (\d+), '
                      r"description (\d+), each twice with its shadow; the cursor's flash (\d+)\); measured (-?\d+) of the 800")
LKL_OVER = re.compile(r'^options: page (\d+) \((.*?)\): (\d+) menu glyphs \(measured (-?\d+)\): over the 800')
LKL_INERT = re.compile(r'^options look: (.+?) is grey \((.*?)\): the key did nothing')
# v3.54 OL34: the Options layout (rounds R3 + R4): the install line, a row left out, the line at every Options opening
# v3.55 OL56: + R5's line (the layout: stock moves on / off and why)
LYL_IN = re.compile(r'^options layout: (?:R3 \+ R4 \(OL34\)|R5 \+ R6 \(OL56\), stock moves (on|off)(?: \((.*?)\))?): the cycle (.+?); '
                    r'(\d+) pages \(folders: .*?\); (\d+) rows placed, (\d+) folder rows, (\d+) left out(?:: (.*?))?; (\d+) of (\d+) texts'
                    r'( \(OVER A LIMIT\))?$')
LYL_VB_IN = re.compile(r'^options layout: the volume bars by record \(0x525672')  # v3.55 OL56: the one exe patch
LYL_VB_BYTES = re.compile(r'^options layout: 0x525672 is (.*?), not the stock')
LYL_VB_FAIL = re.compile(r'^options: layout: writing 0x525672 failed')  # (the body of its ANOMALY line)
LYL_OFF = re.compile(r'^options layout: no Options pages')
LYL_MISS = re.compile(r'^options layout: page (\d+) \((.*?)\): no (.+?) row: (.*)$')
LYL_OPEN = re.compile(r'^options layout: open: the cycle (.+?); pages: (.*)$')
# v3.56 MN356: every value its own line (vlines.inc), a click opens a folder row (menuclick.inc), the Extra Menu's text
MN_VL_IN = re.compile(r'^options: value lines: (\d+) row\(s\) carry the plan\'s (\d+) value lines \((\d+) in the plan\)(.*)$')
MN_VL_ANS = re.compile(r'^options: value lines: (.+?) / (.+?): "(.*)" \(its row code answered the row\'s line\)')
MN_VL_MISS = re.compile(r'^options: value lines: (.+?) / (.+?) value (-?\d+) "(.*?)" has no line in the plan')
MN_FC_SET = re.compile(r'^options: folder rows open on a mouse click (anywhere on the row|only as in v3\.55).*?\(\[Options\] FolderClick=(\d)(.*)\)$')
MN_FC_OPEN = re.compile(r'^options: a click on page (\d+) \((.*?)\) row (\d+) \((.*?)\): its input runs')
MN_FC_REFUSE = re.compile(r'^options: a click on page (-?\d+) row (-?\d+) not taken: (.*) \(click again\)')
MN_EM_TEXT = re.compile(r'^extra menu: text drawn (once|with the stock pause\'s second shadow string).*\(\[ExtraMenu\] StockShadow=(\d)\)')
# v3.37 INV337: Extended Toolbelt / Stackable Items (inventory.inc): the install line, the switches, the counters
INV_INSTALL = re.compile(r'^inventory: Extended Toolbelt (on|off|UNAVAILABLE) .*?, Stackable Items (on|off|UNAVAILABLE) .*?'
                         r'stack guards (in|OFF)')
INV_SWITCH = re.compile(r'^(extended toolbelt|stackable items): (on|off|stays on)\b')
# v3.43 GT343: Ghost Teleport (teleport.inc): the install line, the warps, the refusals, the cooldown waits
GT_INSTALL = re.compile(r'^ghost teleport: (in|OFF) \(no exe patch[^;]*; .*?\[Gameplay\] GhostTeleport (On|Off);')  # v3.58 GT358: [^;]*
GT_WARP = re.compile(r'^ghost teleport: behind type ([0-9A-F]{2}) (\d+) u away')
GT_NO = re.compile(r'^ghost teleport: not now: (.+?)(?: \(.*)?$')
GT_COOL = re.compile(r'^ghost teleport: (?:Run held, waiting for the cooldown|Run \+ Action during the cooldown)')  # v3.44 GT344: +
# v3.44 GT344: the warp ahead (no enemy near), the chord (which press came last), the fade's end, a fade called off
GT_AHEAD = re.compile(r'^ghost teleport: ahead (\d+) u the way she faces')
GT_CHORD = re.compile(r'^ghost teleport: Run \+ Action \((Action last: its own use taken back|Action last: NOT taken back|'
                      r'Action first: its press reached the game|held: the next warp)(?:[;,][^)]*)?\)')  # v3.49 GT349: + held; the
# "; a later call-off keeps it" tail of v3.44's later lines (the old pattern missed every live chord line)
GT_HOLD = re.compile(r'^ghost teleport: Run \+ Action (.+?) after (\d+) warp\(s\) in a row \((\d+) try')  # v3.49 GT349: a hold's end
GT_ZONES = re.compile(r'^ghost teleport: room (\d+):(\d+): (\d+) walk-in event zone\(s\) the game would still fire')  # v3.49 GT349
GT_ZONE_NO = "an event's trigger zone on the way"  # v3.49 GT349: in a refusal's spot list
GT_ZONE_IN = "inside an event's trigger zone"  # v3.49 GT349-001: a refusal while she stands in a zone not fired yet
GT_FADE = re.compile(r"^ghost teleport: Richard's fade ended \(drawn see-through on (\d+) body and (\d+) weapon draw")
# v3.58 GT358: the fade line's draw path (late frames, of them with the pre-pass, in place by shader 4 / 5, missed, the filter,
# the heat body / soul fire faded, her shadow hidden) and the start's draw mode
GT_FADE_PATH = re.compile(r'; the draw: late (\d+) frame\(s\) \((\d+) with the depth pre-pass\), in place (\d+) \(shader 4\) / '
                          r'(\d+) \(shader 5\), (\d+) missed; filter (.+?): heat body faded (\d+), soul fire faded (\d+); '
                          r'her shadow hidden (\d+)$')
GT_FADE_DRAW = re.compile(r"^ghost teleport: the fade's draw: (Clean|One pass|Stock)")
GT_FADE_NOTIN = re.compile(r"^ghost teleport: the clean fade's draw is not in \((.+)\): one pass in place$")
GT_OFF = re.compile(r'^ghost teleport: called off during the fade: (.+?)(?: \(.*)?$')
# v3.50 TP350: the warp through a wall, the faced enemy, a room visit's counts, the room's floor map
GT_THRU = re.compile(r'^ghost teleport: through (\d+) u the way she faces')
GT_FACED = 'the enemy she faces'  # in a GT_WARP line: behind the enemy she faces (v3.50), not the nearest (v3.43 .. v3.49)
GT_ROOM = re.compile(r'^ghost teleport: room (\d+):(\d+) left: (\d+) warp\(s\) \((\d+) through walls, (\d+) behind the enemy she '
                     r'faces, (\d+) ahead\), (\d+) refused(?: \((.*?)\))?; (\d+) Action / Check press\(es\) taken back in '
                     r'fade-outs(?:; (\d+) enemy\(ies\) in the cone skipped behind collision in (\d+) try\(ies\))?'
                     r'(?:; (\d+) time\(s\) freed from a wall)?(?:; (\d+) up a level, (\d+) down a level)?; floor map (.*)$')
# v3.59 GT359: + the warps up / down a level in the room visit (optional: a v3.58 room line has none)
# v3.59 GT359: a warp up / down a level (its heights, the candidates), a room where none was proven (the flat warp)
GT_LEVEL = re.compile(r'^ghost teleport: (up|down) a level: height \(y\) from (-?\d+) to (-?\d+) \((\d+) u (?:up|down)\), '
                      r'(\d+) u off her place; candidates: (\d+) floor cell\(s\)')
GT_LV_FLAT = re.compile(r'^ghost teleport: room (\d+):(\d+): no landing on another level is proven here \((up|down) a level')
# v3.51 TP351: + the enemies in the cone skipped behind collision (optional: a v3.50 room line has none)
# v3.52 TP352: + the times she was freed from a wall in the room visit (optional: a v3.51 room line has none)
# v3.52 TP352: freed from a wall (after a warp, or at a warp from inside one) / left inside collision (not moved)
GT_FREE = re.compile(r'^ghost teleport: freed from a wall: (after warp|at the warp)')
GT_FREE_NO = re.compile(r'^ghost teleport: inside collision after warp \d+ at \(-?\d+, -?\d+\): (?:no proven floor cell|not moved)')
GT_MAP = re.compile(r'^ghost teleport: room (\d+):(\d+): the floor map is finished: (\d+) cell\(s\) a route of the room reaches, '
                    r'from (\d+) seed\(s\) \((\d+) without a floor\); ([\d.]+) ms over (\d+) run')
# v3.46 OM346: Stick Movement (omni.inc): the install line, the mode changes, the bindings, the self-check, the counts
# v3.48 OM348: Omni-Directional Movement, Off / On (On = v3.47's Analog); v3.46 / v3.47 logs (Direction / Analog) still parse
OM_MODE = r'(Off|On|Direction|Analog)'
OM_IN = re.compile(r'^omni: (in|OFF) \(005543C0 .*?\[Input\] OmniMove ' + OM_MODE + r', OmniDeadZone ([\d.]+)')
OM_NOT = re.compile(r'^omni: (.+?) is not the stock code')
OM_SET = re.compile(r'^omni: OmniMove ' + OM_MODE + r' \(was ' + OM_MODE + r'; (.+?)\)')
OM_REREAD = re.compile(r'^omni: \[Input\] OmniMove read again: ' + OM_MODE + r' \(was ' + OM_MODE + r'\)')
# v3.48 OM348: an old value (v3.46 / v3.47's Direction / Analog / 2): written back at the start, or read as On while running
OM_OLD = re.compile(r'^omni: \[Input\] OmniMove=(.*?) \(v3\.46 / v3\.47\) is (On now .*?: (written as On|not written|could not be '
                    r'written)|read as On)')
OM_DZ = re.compile(r'^omni: \[Input\] OmniDeadZone read again: ([\d.]+)')
OM_BIND = re.compile(r'^omni: bindings: TurnRight ([0-9A-F]{4}), TurnLeft ([0-9A-F]{4}), MoveForward ([0-9A-F]{4}), '
                     r'MoveBackward ([0-9A-F]{4}) = (the left stick \(stock\)|the right stick.s axes \(swapped sticks\)|not a stick)')
OM_CHECK = re.compile(r'^omni: self-check \(check (\d+)\): GetDeviceState (\d+) of (\d+) agree, call sites (\d+) of (\d+) agree: '
                      r'source (GetDeviceState|call sites|none)')
OM_COUNTS = re.compile(r'^omni: ' + OM_MODE + r': since the last change (\d+) frame\(s\) written \((\d+) of them from the '
                       r'call sites\), (\d+) stopped')
OM_GATE = re.compile(r'^omni: the stick is pushed in game state (-?\d+) / (-?\d+), overlay (-?\d+), phase (-?\d+)')
OM_SWAP = re.compile(r'^omni: the swapped-stick bindings need GetDeviceState')
OM_RECHECK = re.compile(r'^omni: (GetDeviceState|call sites) agreed with the game.s direction bits in only (\d+) of (\d+) frames')
# v3.47 CT347: the row on Option 2, Playable Eileen's answer, GetDeviceState failures while the game window is not in front
OM_ROW = re.compile(r'^omni: the (?:Stick|Omni-Directional) Movement row is on Option 2 \(Control\), row (\d+) of (\d+)')  # v3.48 OM348: + the new name
OM_NOROW = re.compile(r'^omni: (no Options pages|Option 2.s table is not the stock one)[^:]*: no (?:Stick|Omni-Directional) Movement row')
OM_PE = re.compile(r'^omni: Playable Eileen (follows (?:Analog|the stick).s push|reads SH4R_StickAmount but has it off|'  # v3.48 OM348: "the stick's push"
                   r'does not read SH4R_StickAmount)')
OM_BG = re.compile(r'^omni: GetDeviceState on the game.s pad failed \(hr ([0-9A-F]{8})\) (while the game window is not in front|'
                   r'for (\d+) frame\(s\) around a focus loss)')
# v3.49 LK349: the analog look ([Input] OmniLook): its start line, a read again, an unknown value, the look bindings, the
# look self-check, and the per-place count line (verbose)
OL_IN = re.compile(r'^omni: look: \[Input\] OmniLook (Off|Room302|All) \(')
OL_REREAD = re.compile(r'^omni: \[Input\] OmniLook read again: (Off|Room302|All) \(was (Off|Room302|All)\)')
OL_BAD = re.compile(r'^omni: \[Input\] OmniLook=(.*?) is not Off, Room302 or All')
OL_BIND = re.compile(r'^omni: look bindings: LookRight ([0-9A-F]{4}), LookLeft ([0-9A-F]{4}), LookUp ([0-9A-F]{4}), '
                     r'LookDown ([0-9A-F]{4}) = (the right stick \(stock\)|the left stick.s axes \(swapped sticks\)|not a stick)')
OL_CHECK = re.compile(r'^omni: look self-check: GetDeviceState (\d+) of (\d+) agree with the game.s look bits: (the analog look is on|'
                      r'the game.s own 8-way look stays)')
OL_COUNT = re.compile(r'^omni: look: (Room 302|stage -?\d+), (\d+) s \((.+?)\): the look stick pushed (\d+) frame\(s\): (\d+) analog '
                      r'\(OmniLook (?:Off|Room302|All)\), the game.s own (\d+) \(OmniLook not here (\d+), a look key (\d+), no pad '
                      r'read (\d+), look self-check \w[\w ]*? (\d+), look not on a stick (\d+)\); the game.s reversal latch (\d+) '
                      r'frame\(s\) \(x (\d+), y (\d+)\); a push the game.s 8-way look missed (\d+) frame\(s\); the right stick.s '
                      r'turn kept from a small sideways push (\d+) frame\(s\)')
OM_AGAIN = re.compile(r'^omni: GetDeviceState reads the game.s pad again after (\d+) failed frame\(s\)( \(the game window was not in front\))?')
# v3.59 CT359: the control-direction latch (omnilatch.inc L1) and Lock-On Walk (L2): install, episodes, room summaries
LT_IN = re.compile(r'^omni: latch: (in|OFF) \(00502794: the control-direction latch 00502040\); \[Input\] OmniLatch ([01])')
LT_EP = re.compile(r'^omni: latch: (.+?) held the control frame ([\d.]+) s \((the Shoulder camera.s view|the game.s camera), Omni '
                   r'(On|Off)(, Follow on)?(, a warp of the player in the 2 s before)?, rule (Refined.s|the game.s)\): let go by (.+?); '
                   r'the stick (\d+) deg / ([\d.]+) from the latch; the control frame up to (\d+) deg from the camera, (\d+) '
                   r'frame\(s\) blended')
LT_ROOM = re.compile(r'^omni: latch: stage (-?\d+) room (-?\d+): (\d+) episode\(s\) .*?; (\d+) rig look-rate trigger\(s\) ignored '
                     r'under the Shoulder camera; (\d+) line\(s\) over the room.s quota')
LT_REREAD = re.compile(r'^omni: \[Input\] OmniLatch read again: ([01])')
LT_BAD = re.compile(r'^omni: \[Input\] OmniLatch=(.*?) is not 0 or 1')
LT_FAULT = re.compile(r'^omni: (latch|lock-on walk): (?:its log )?faulted')
LT_NOT = re.compile(r'^omni: (latch|lock-on walk): (\S+) \(call (\S+)\) is not the stock code')
LW_IN = re.compile(r'^omni: lock-on walk: (in|OFF) \(00534C23: the ready-stance step 00534620\); \[Gameplay\] LockOnWalk (Off|On)')
LW_FIRST = re.compile(r'^omni: lock-on walk: the first ready-stance step below a full push this session \(weapon 0x([0-9A-F]{2}), '
                      r'(locked on|no target)')
LW_ROOM = re.compile(r'^omni: lock-on walk: stage (-?\d+) room (-?\d+): (\d+) frame\(s\) stepped below a full push \((\d+) of them '
                     r'locked on\), weapons (.+)$')
LW_SET = re.compile(r'^omni: LockOnWalk (Off|On) \(was (Off|On); (.+?)\)')
LW_REREAD = re.compile(r'^omni: \[Gameplay\] LockOnWalk read again: (Off|On)')
LW_BAD = re.compile(r'^omni: \[Gameplay\] LockOnWalk=(.*?) is not Off or On')
# v3.47 RG347: Refined's own Regeneration (regen.inc): the install line, Playable Eileen's exports, who heals, the value
# changes, the healing runs (start / stop), Life Steal's heal route, the first start's seed from Playable Eileen's ini
RG_IN = re.compile(r'^regeneration: (in|OFF) \(no exe patch; .*?\); \[Gameplay\] Regen (Off|x1|x1\.25|x2) \(([\d.]+) % of max '
                   r'health a minute\), RegenDelay (\d+) s; who heals now: (.+)$')
RG_PE = re.compile(r'^regeneration: Playable Eileen loaded (with|without) SH4PE_RegenActive')
RG_WHO = re.compile(r"^regeneration: who heals: (.+) \(Refined's Regen (Off|x1|x1\.25|x2), RegenDelay (\d+) s\)$")
RG_SET = re.compile(r'^regeneration: Regen (Off|x1|x1\.25|x2) \(was (Off|x1|x1\.25|x2); (.+?)\)')
RG_REREAD = re.compile(r'^regeneration: \[Gameplay\] Regen read again: (Off|x1|x1\.25|x2) \(was (Off|x1|x1\.25|x2)\)')
RG_DELAY = re.compile(r'^regeneration: \[Gameplay\] RegenDelay read again: (\d+) s')
RG_START = re.compile(r'^regeneration: heals (Henry|Eileen) from ([\d.]+) / (\d+) HP \((x1|x1\.25|x2): ([\d.]+) HP a second\)')
RG_STOP = re.compile(r'^regeneration: stops: (.+?) \(([\d.]+) / (\d+) HP, \+([\d.]+) HP in ([\d.]+) s\)$')
RG_HURT = re.compile(r'^regeneration: stops: hurt \(-([\d.]+) HP; ([\d.]+) / (\d+) HP, \+([\d.]+) HP in ([\d.]+) s\): no healing '
                     r'for (\d+) s')
RG_NOT = re.compile(r'^regeneration: (0052D1D0 \(PlayerHealAmount\) is not the stock code|no 0x4EF982 tick dispatcher)')
# v3.50 R302: Room 302's look and walk (room302.inc)
R3_OWN = re.compile(r'^room 302: owners \(([^)]*)\): look (nobody|SH4 Refined|Playable Eileen|waiting for Playable Eileen), walk (nobody|SH4 Refined|Playable Eileen|waiting for Playable Eileen); Playable Eileen (.*?); '
                    r'\[Room302\] LookDownLimit (\S+), LookUpLimit (\S+), LookFeel (\S+), WalkSpeed ([^;\s]+)'
                    r'(; decided again on the first game tick)?$')
R3_HOOK = re.compile(r'^room 302: look hooks in \(([^)]*)\)')
R3_FAIL = re.compile(r'^room 302 look: [0-9A-F]{8} could not be written')
R3_VISIT = re.compile(r'^room 302: visit (\d+) s \(([^)]*)\): look (\d+) tick\(s\) \(eased (\d+), clamped (\d+): up (\d+), '
                      r'down (\d+); view rebuilt (\d+)\), walk x([\d.]+) (\d+) tick\(s\)$')
R3_SET = re.compile(r'^room 302: \[Room302\] (LookDownLimit|LookUpLimit|LookFeel|WalkSpeed) (\S+) -> (\S+) \(([^)]*)\)$')
R3_SEED = re.compile(r'^settings: \[Room302\] (LookDownLimit|LookUpLimit|LookFeel|WalkSpeed) was missing, added as (\S+)'
                     r'(?: \((.*)\))?$')
R3_BAD = re.compile(r'^room 302: \[Room302\] (\w+)=(.*?) is not (?:a number|one of the row.s values): (\S+) used$')
R3_NOPAGE = re.compile(r'^room 302: (no sub-screen fade|Option 2.s Button Config record is not the stock one): no Room 302 page')
R3_REBUILT = re.compile(r'^room 302: look: the view was rolled or past a limit')

# v3.56 FR356: the rate guard (rateguard.inc) and the final room's per-tick sites (true60_final.inc)
FRG_ON = re.compile(r'^rate guard: (on|off|not used) \(')
FRG_TOP = re.compile(r'\(top now (\d+)\)$')
FRG_DOWN = re.compile(r'^rate guard: (\d+) -> (\d+): the play frames took ([\d.]+) ms on average against ([\d.]+) \(([\d.]+) x\) '
                      r'(?:in (\d+) windows of (\d+) s in a row|in the last (\d+) s, the effect budget with nothing left to shed) '  # v3.58 GX358
                      r'\(stage ([0-9A-F]+) room ([0-9A-F]+); effect budget level (\d+)\)(; stepped down from it '
                      r'twice: not tried again this run)?; \[Window\] FrameRate stays (\d+)')
FRG_UP = re.compile(r'^rate guard: (\d+) -> (\d+) at the room change to ([0-9A-F]+):([0-9A-F]+) ')
FRG_RETRY = re.compile(r'^rate guard: (\d+) minutes clean at (\d+) .*: (\d+) is tried again at the next room change')
FRG_MISS = re.compile(r'^rate guard: a (\d+) s window at (\d+) missed: ([\d.]+) ms on average against ([\d.]+) \(([\d.]+) x\)(; held: the effect '
                      r'budget is still shedding)?')
FRG_RESET = re.compile(r'^rate guard: the Frame Rate row chose (\d+): the guard starts again \(it held the game at (\d+)\)')
FRG_VSYNC = re.compile(r'^rate guard: the V-Sync cap is now (\d+): the guard.s (\d+) is lifted')
T60F_IN = re.compile(r'^true 60: (\d+)/(\d+) final-room sites follow the rate')
T60F_BAD = re.compile(r'^true 60: ([0-9A-F]{8}) (?:\(.*\) is not stock: none of the \d+ final-room sites is used|could not be written: the '
                      r'final-room sites are put back)')
# v3.59 FR359: cut-scenes and Room 302's views at the gameplay rate (ratephase.inc)
CR_IN = re.compile(r'^frame rate: cut-scenes (at the gameplay rate|at 30); Room 302.s window view (at the gameplay rate|at 30), its '
                   r'peephole / door-scope / hole views (at the gameplay rate|at 30) \(hidden .*?; (\d+) window view sites and (\d+) caves\)')
CR_OFF = re.compile(r'^frame rate: cut-scenes and Room 302.s views at 30 \(hidden \[Window\] CutsceneRate=0, ViewRate=0\)')
CR_START = re.compile(r'^frame rate: (\d+) in a (cut-scene|Room 302 window view|Room 302 peephole / hole view) \(scene ([0-9A-F]+);')
CR_END = re.compile(r'^frame rate: the (cut-scene|Room 302 window view|Room 302 peephole / hole view) at (\d+) ended after ([\d.]+) s: '
                    r'(\d+) camera cut\(s\) and (\d+) actor jump\(s\) shown at their stock moment; the stream ([+-]?\d+) ms against real time')
CR_CUT = re.compile(r'^frame rate: (cut-scene|Room 302 window view|Room 302 peephole / hole view) frame (-?\d+) -> (-?\d+): '
                    r'(a camera cut|an actor put elsewhere)')
CR_DRIFT_MS = 100  # a stretch whose stream ended further from real time than this is named in the summary
RG_SEED = re.compile(r'^settings: \[Gameplay\] (Regen|RegenDelay) was missing, added as "([^"]*)" \((.*)\)$')
# v3.48 RZ348: the Randomizer (randomizer.inc): the start, the seed, the plans per world visit, the room loads, the changes
# v3.56 RI356: + the new keys and values (Specials, Room302 Off / Shuffle / Random, Reroll By Room / By World); the key names
# are groups 1 and 4 (parse_rando keeps the v3.48 tuple and notes the form)
RZ_IN = re.compile(r'^randomizer: \[Randomizer\] Items (Off|Shuffle|Random), (KeepSpecials|Specials) (On|Off), Room302 '
                   r'(In|Out|Off|Shuffle|Random), (PerVisit|Reroll) (On|Off|By Room|By World), Seed (\d+); self-check: (-?\d+) '
                   r'plain pickup\(s\) and (\d+) weapon spot\(s\) in (\d+) script\(s\) '
                   r'\(stock (\d+) / (\d+)\): (.+?); (\d+) weapons may move(.*?); applied at each room load')
RZ_ROLL = re.compile(r'^randomizer: \[Randomizer\] Seed was "([^"]*)"(?: \([^)]*\))?: rolled (\d+) (and written|for this session \(not written\))')
RZ_BADKEY = re.compile(r'^randomizer: \[Randomizer\] (Items|KeepSpecials|Specials|Room302|PerVisit|Reroll) "([^"]*)" '
                       r'(?:\(v3\.55\) )?is not .+?: (?:Specials |Reroll )?(By Room|By World|\w+) \(the default\)$')  # v3.56 RI356: + the new keys
RZ_FIRST = re.compile(r'^randomizer: the first room load through 004F0AF8 \(stage (\d+) room (\d+)\): Items (\S+)$')
# v3.58 MN358: a stage without both pickup model files (a story interlude: stage 10 / 11): its pickups are left alone by
# design. v3.48 .. v3.57 wrote it as an ANOMALY (once per stage and half), v3.58 as a plain line (once per stage)
RZ_NOMODELS = re.compile(r"^randomizer: stage (\d+): item_model\.bin / wp_model\.bin are not both in the world's file list")
# v3.58 MN358: the Options value box (valbox.inc): the install line, a page's box (level 2), a value wider than its page's
# measure, and the switch off / not on
VBX_ON = re.compile(r'^options: value box on: .*?\(0052570E, 00525802, 0052587C, 00525CE3(; the volume bars\' arrows '
                    r'0x5256B6 / 0x5256E2)?\)(.*)$')
VBX_OFF = re.compile(r'^options: value box (off \(\[Options\] ValueBox=0\)|NOT on: (.+?) not stock)')
VBX_PAGE = re.compile(r'^options: value box: page (\d+) \(([^)]*)\): (\d+) value row\(s\), (\d+) value\(s\), the widest (\d+) px, '
                      r'the widest label (\d+) px: the arrows at x (-?\d+) and (-?\d+) on every row \(([^)]*)\)(.*)$')
VBX_GROW = re.compile(r'^options: value box: page (\d+) row (\d+): a (\d+) px value is wider than the page\'s measure: the arrows at x '
                      r'(-?\d+) and (-?\d+) from now on')
RZ_PLAN = re.compile(r'^randomizer: stage (\d+) visit (\d): (Shuffle|Random), (\d+) of (\d+) pickup\(s\) in the pool \((\d+) weapon '
                     r'spot\(s\)\), (\d+) changed \(seed (\d+), (?:Keep Specials|Specials) (On|Off), Room 302(?: Items)? '
                     r'(In|Out|Off|Shuffle|Random), (?:Change Each Visit|Reroll) (On|Off|By Room|By World), '  # v3.56 RI356: + the new words
                     r'world visit (\d+)\); Holy Candle (\d+) \(stock (\d+)\), Saint Medallion (\d+) \(stock (\d+)\), Sword of Obedience '
                     r'(\d+) \(stock (\d+)\), weapons (\d+) \(stock (\d+)\)(; fewer Swords of Obedience[^;]*)?(; SOME ITEMS COULD NOT BE PLACED)?')
RZ_ROOM = re.compile(r'^randomizer: room (\d+):(\d+) \(visit (\d), #(\d+)\): (\d+) of (\d+) pickup\(s\) changed, (\d+) weapon\(s\) '
                     r'\(Items (\w+)\)(; some words)?')
RZ_LIST = re.compile(r'^randomizer: room (\d+):(\d+): (bit .+)$')
RZ_SET = re.compile(r'^randomizer: (Items|KeepSpecials|Specials|Room302|PerVisit|Reroll) (By Room|By World|\w+) \(was '
                    r'(By Room|By World|\w+); applies at the next room load\)$')  # v3.56 RI356: + Specials, Reroll
RZ_SEEDSET = re.compile(r'^randomizer: (New Seed|Seed back to) (\d+) \(was (\d+); (saved|NOT saved); applies at the next room load\)$')
RZ_REREAD = re.compile(r'^randomizer: \[Randomizer\] (Items|KeepSpecials|Specials|Room302|PerVisit|Reroll|Seed) read again: '
                       r'(By Room|By World|\S+)')  # v3.56 RI356: + Specials, Reroll
RZ_SESS = re.compile(r'^randomizer: session: Items (\w+) \(seed (\d+), (?:Keep Specials|Specials) (On|Off), Room 302(?: Items)? '
                     r'(In|Out|Off|Shuffle|Random), (?:Change Each Visit|Reroll) (On|Off|By Room|By World)\): (\d+) room load\(s\), (\d+) with pickups in the pool, (\d+) pickup\(s\) changed, (\d+) weapon\(s\) '
                     r'placed, (\d+) plan\(s\), (\d+) word\(s\) left as found(.*)$')
# v3.56 RI356: v3.55's keys migrated once, the seed line under the page's description, the world-visit rule (worldvisit.inc)
RZ_MIG = re.compile(r'^randomizer: \[Randomizer\] (KeepSpecials|Room302|PerVisit)=(.*?) \(v3\.55\) is (Specials|Room302|Reroll)='
                    r'(By Room|By World|\w+) now \((.+?)\): (migrated once|NOT written|for this session)')
RZ_SEEDLINE = re.compile(r"^randomizer: the seed (\d+) is drawn under the page's description \(x (-?\d+), y (-?\d+), (\d+) "
                         r"glyph\(s\), (the smaller size|full size: .*)\)$")
WV_WORLD = re.compile(r'^visits: world visit ([0-9A-F]{8}) begins: stage (\d+) entered \(room (\d+); world entry (\d+) of this '
                      r'session(, after a New Game / title load)?\)$')
WV_LOAD = re.compile(r"^visits: room (\d+):(\d+) after a load \((.+?)\): no room or world entry; world visit ([0-9A-F]{8}) "
                     r"(resumed|= the seed's own|goes on).*\(room entries (\d+)\)$")
WV_SESS = re.compile(r"^visits: session: (\d+) room entr(?:y|ies), (\d+) world visit\(s\), (\d+) load\(s\) \((\d+) resumed a "
                     r"stage's visit, (\d+) the seed's own\), (\d+) world reset\(s\), (\d+) room load\(s\) ignored \(room 0\)$")
# v3.49 RZ349: the Randomizer's Monsters (rzmon.inc)
RZM_VAL = r'(Off|This Room|This World|Same World|All Worlds)'  # v3.50 RM350: + All Worlds  # v3.56 RE356: + Same World
RZM_IN = re.compile(r'^randomizer: \[Randomizer\] (?:Monsters|Enemies) ' + RZM_VAL + r'; self-check: (-?\d+) source placement\(s\) of (\d+) enemy '
                    r'placement\(s\) in (\d+) script\(s\) \(stock (\d+) / (\d+)\): (.+?); targets (.+?) \(Gum Heads only with a crash '
                    r"guard(?:; victims' ghosts by the story rule)?\); applied at each room load through 004F0AF8 \(patch-and-restore of "
                    r'the type / mode / (?:y|position) words\); save filter 00465468 (.+)$')  # v3.56 RE356: Enemies, the story rule, position
RZM_BADKEY = re.compile(r'^randomizer: \[Randomizer\] (?:Monsters|Enemies) "([^"]*)" is not .+?: ' + RZM_VAL + r' \(the default\)$')
RZM_SET = re.compile(r'^randomizer: (?:Monsters|Enemies) ' + RZM_VAL + r' \(was ' + RZM_VAL + r'; applies at the next room load\)$')
RZM_REREAD = re.compile(r'^randomizer: \[Randomizer\] (?:Monsters|Enemies) read again: ' + RZM_VAL)  # v3.56 RE356: + Enemies
# v3.56 RE356: the renamed keys read once (Monsters -> Enemies, Ghosts -> SpawnSite), the story rule, Spawn Site's monster sites
RZ_KEYMIG = re.compile(r'^randomizer: \[Randomizer\] (Monsters|Ghosts) "([^"]*)" is now (Enemies|SpawnSite) (.+?) \(v3\.56: the row was '
                    r'renamed(; not one of its values: the default)?; (written, the old key removed|NOT written: the old key is read '
                    r'again at the next start)\)$')
RZM_STORY = re.compile(r'^randomizer: monsters: story rule in stage (\d+): (.+) \(their create sets that flag; in the pool once it is set\)$')
SS_ROOM = re.compile(r'^randomizer: spawn site: room (\d+):(\d+) \(#(\d+)\): (\d+) of (\d+) monster\(s\) at a new site, (\d+) record\(s\) '
                     r'moved with them; floor map (\d+) cell\(s\) \(([\d.]+) ms\)(?: \(kept or refused: (.*)\))?$')
SS_NOMAP = re.compile(r'^randomizer: spawn site: no floor map in room (\d+):(\d+) \((.*)\): its monsters keep their spots \(said once\)$')
SS_FN = re.compile(r"^randomizer: spawn site: ([0-9A-F]{8}) is not the game's code \(another mod\?\): Spawn Site is off for this session$")
SS_SESS = re.compile(r'^randomizer: spawn site session: (Off|On): (\d+) room load\(s\) with monsters, (\d+) monster\(s\) at a new site, '
                     r'(\d+) kept, (\d+) record\(s\) moved; (\d+) floor map\(s\) \((\d+) cell\(s\), ([\d.]+) ms in all\), (\d+) room '
                     r"load\(s\) without a map; (\d+) ghost\(s\) at a site in the room's box(.*)$")
RZM_GUM = re.compile(r"^randomizer: monsters: Gum Heads (may be rolled|are never rolled) \(Playable Eileen's crash guard of the Gum "
                     r"Head's group call: 0049B70A (in|not in), 0045C3C0 (in|not in)\)$")
RZM_CT = re.compile(r"^randomizer: monsters: the cheat table's Enemy Randomizer \(entry 7700: 0052CAA9 / 0052CB2B\) is (on|off again): "
                    r"Refined's Monsters ")
RZM_HIT = re.compile(r'^randomizer: monsters: the hit-shape pool is the stock 100')
RZM_FIRST = re.compile(r'^randomizer: monsters: the first room with source placements \(stage (\d+) room (\d+)\): (?:Monsters|Enemies) ' + RZM_VAL + '$')
RZM_OFFDROP = re.compile(r'^randomizer: monsters: room (\d+):(\d+): (\d+) record\(s\) of monsters made as another type before are deleted')
RZM_LEFT = re.compile(r'^randomizer: monsters: room (\d+):(\d+) left as placed \((.+)\)$')
RZM_FILES = re.compile(r"^randomizer: monsters: room (\d+):(\d+): (.+?)'s files could not be loaded \((.*)\): its placements stay as placed$")
RZM_SILENT = re.compile(r"^randomizer: monsters: (.+?) is no monster of stage (\d+)'s scripts: its sounds may be missing there \(allowed\)$")
RZM_ROOM = re.compile(r"^randomizer: monsters: room (\d+):(\d+) \(visit (\d), #(\d+)\): (\d+) of (\d+) placement\(s\) changed \((This Room|"
                      r"This World|Same World|All Worlds)\): (.*?); (\d+) kept their record's type, (\d+) record\(s\) deleted; kept as placed: (\d+) for the "
                      r"bone budget, (\d+) for files, (\d+) without a floor(; some words[^;]*)?; files loaded for (\d+) type\(s\) \((\d+) KB\)"
                      # v3.50 RM350: + the game's own records adopted and why the stock type was kept (v3.49 lines end before it)
                      r"(?:; the game's own records: (\d+) adopted \((\d+) made as another type\); stock type kept: (\d+) rolled it, (\d+) "
                      r"by a stock record, (\d+) recorded elsewhere, (\d+) for another writer's record, (\d+) with no target)?"
                      # v3.51 SN351: + the second roll and the event dogs (only when there are any)
                      r"(?:; rolled again for their own type: (\d+))?(?:; event dogs: (\d+), story flag 130 (set|clear) \((?:rolled as "
                      r"the others|kept as placed for their event)\))?"
                      # v3.58 RZ358: + the dead kept dead (only when there are any)
                      r"(?:; the dead stay dead: (\d+) bod(?:y|ies) as (?:it|they) died \((\d+) empty here\), (\d+) by a kill bit"
                      r"(, SOME BACK: no kill index)?)?$")
# v3.58 RZ358: the dead stay dead (rzmon.inc), Extra Enemies' copies drawn (rzmon.inc RmCopiesSay), the copies' record guard
RZM_DEADV = re.compile(r'^randomizer: monsters: room (\d+):(\d+): n(\d+) lies dead here as another type \((.*)\): its body record stays '
                       r"and its placement stays empty for this room's build \(kill index (\d+)\)$")
RZM_DEADLOST = re.compile(r'^randomizer: monsters: room (\d+):(\d+): n(\d+) lies dead here as another type \((.*)\) and has no kill '
                          r'index: its body record is deleted \(it stands again\)$')
RZM_SAVEDEAD = re.compile(r'^randomizer: monsters: a save: (\d+) randomized monster\(s\) lying dead are kept dead in it as kill bits '
                          r'\(.*\)(, SOME without a kill index come back after a load)?$')
RZM_COPIES = re.compile(r"^randomizer: monsters: copies x(\d): room (\d+):(\d+): (\d+) made, types (.+?); (\d+) drawn by the seed \((\d+) "
                        r"at a site of their own\), (\d+) kept their original's type \(files\), (\d+) not made \(the copies' line says why\)$")
RZM_CPFILES = re.compile(r"^randomizer: monsters: copies: room (\d+):(\d+): (.+?)'s files could not be loaded \((.*)\): those copies "
                         r"keep their original's type$")
RZM_SESS5 = re.compile(r'^randomizer: monsters session: the dead stay dead: (\d+) bod(?:y|ies) kept as they died, (\d+) kept by a kill '
                       r"bit, (\d+) kept with their placement empty for a room's build \(type not makeable / Enemies Off\), (\d+) lost "
                       r'\(no kill index\); saves: (\d+) randomized bod(?:y|ies) kept dead as kill bits, (\d+) lost$')
RZM_SESS6 = re.compile(r'^randomizer: monsters session: copies: (\d+) room load\(s\) with copies drawn: (\d+) drawn by the seed \((\d+) '
                       r"at a site of their own\), (\d+) made, (\d+) kept their original's type \(files\); (\d+) descriptor\(s\) made "
                       r'as a drawn type \(in their room or away\)$')
ENM_CPRECS = re.compile(r'^enemies: room (\d+):(\d+) copies: (\d+) not made: the enemy records would be full at this room\'s exit')
ENM_CPRECV = re.compile(r"^enemies: copies: the enemy records are full for this room's exit \((\d+) free, (\d+) of other worlds, (\d+) "
                        r'real enemies to write\): no copy is made')
# v3.51 SN351: the event dogs and the second roll at the start, and the session's counts
RZM_EVIN = re.compile(r"^randomizer: monsters: (\d+) of the source placements are event Sniffer Dogs \(spawn mode 6 / 7, stock: Subway "
                      r"room 2:2's (\d+)\): rolled once story flag 130 is set \(their cut-scene is over\), else as placed; a roll of the "
                      r"placement's own type rolls once more: (On|Off) \(\[Randomizer\] MonsterRollAgain\)$")
RZM_SESS4 = re.compile(r"^randomizer: monsters session: (\d+) placement\(s\) rolled again for their own type \(MonsterRollAgain "
                       r"(On|Off)\); event dogs: (\d+) planned with story flag 130 set, (\d+) kept as placed for their event \(flag "
                       r"clear\)$")
RZM_SESS3 = re.compile(r"^randomizer: monsters session: the game's own records: (\d+) adopted \((\d+) made as another type\); stock type "
                       r"kept: (\d+) rolled it, (\d+) by a stock record, (\d+) recorded elsewhere, (\d+) with no target$")  # v3.50 RM350
RZM_LIST = re.compile(r'^randomizer: monsters: room (\d+):(\d+): (n\d+ .+)$')
RZM_SAVE = re.compile(r'^randomizer: monsters: a save: (\d+) record\(s\) of monsters made as another type are left out of it')
RZM_SAVEV = re.compile(r'^randomizer: monsters: a save: [0-9A-F]{8} \(made as ')
# v3.49 fix2 RZ349-SAVE-001: records Refined did not make, left alone
RZM_OTHER = re.compile(r"^randomizer: monsters: room (\d+):(\d+): (\d+) record\(s\) of another type that Refined did not make "
                       r"\(another writer\): those placements stay as placed, the records are kept$")
RZM_SESS2 = re.compile(r"^randomizer: monsters session: (\d+) placement\(s\) left as placed for a record Refined did not make "
                       r"\(another writer\); (\d+) placement\(s\) of Refined's \((\d+) full\)$")
RZM_SESS = re.compile(r"^randomizer: monsters session: " + RZM_VAL + r" \(seed (\d+), Change Each Visit (On|Off)\): (\d+) room load\(s\), "
                      r"(\d+) with source placements randomized, (\d+) placement\(s\) changed, (\d+) kept their record's type, (\d+) "
                      r"record\(s\) deleted \(\+(\d+) with Monsters off\); kept as placed: (\d+) for the bone budget, (\d+) for files, "
                      r"(\d+) without a floor; files loaded (\d+) time\(s\) \((\d+) KB\); (\d+) room load\(s\) with the cheat table's "
                      r"Enemy Randomizer on; (\d+) save\(s\), (\d+) record\(s\) left out; (\d+) word\(s\) left as found(.*)$")
# v3.49 RZ349L: the Randomizer's looks (rzlooks.inc): Eileen Outfit Off / Random, Ghosts Off / Spots
RZL_IN = re.compile(r'^randomizer: looks: \[Randomizer\] EileenOutfit (Off|Random), (?:Ghosts|SpawnSite) (Off|Spots|On) ')  # v3.56 RE356
RZL_BADKEY = re.compile(r'^randomizer: \[Randomizer\] (EileenOutfit|Ghosts|SpawnSite) "([^"]*)" is not .+?: (\w+) \(the default\)$')
RZL_FIRST = re.compile(r'^randomizer: looks: the first room load \(stage (\d+) room (\d+)\): Eileen Outfit (Off|Random), ([^;]*); '
                       r'(?:Ghosts|Spawn Site) (Off|Spots|On), (.*)$')
RZL_DRAW = re.compile(r'^randomizer: outfit: world visit (\d+) \(stage (\d+), half (\d), at ([^)]*)\): (Dress|Nurse) \(seed (\d+), '
                      r'Change Each Visit (On|Off); (?:unlocked|every outfit): Dress, Nurse; the set outfit (\w+)\)$')  # v3.50 RL350: + every outfit
RZL_SINGLE = re.compile(r'^randomizer: outfit: world visit (\d+) \(stage (\d+), half (\d), at ([^)]*)\): only the Dress is unlocked '
                        r'in this save: her outfit stays (\w+)$')
RZL_BACK = re.compile(r'^randomizer: outfit: back to the set outfit (\w+) \((Eileen Outfit Off|a world without her)\)$')
RZL_TAKE = re.compile(r'^randomizer: outfit: (\w+) chosen in the Extra Menu: kept until the next world visit$')
RZL_GROOM = re.compile(r'^randomizer: ghosts: room (\d+):(\d+) \(#(\d+)\): (\d+) of (\d+) victim\(s\) moved \(seed (\d+), Change '
                       r'Each Visit (On|Off)\): (.*)$')
RZL_CT = re.compile(r"^randomizer: ghosts: the cheat table's Enemy Randomizer \(0052CAA9 / 0052CB2B\) is on")
RZL_SET = re.compile(r'^randomizer: (EileenOutfit|Ghosts|SpawnSite) (\w+) \(was (\w+); applies at the next room load\)$')
RZL_REREAD = re.compile(r'^randomizer: \[Randomizer\] (EileenOutfit|Ghosts|SpawnSite) read again: (\w+)')
# v3.50 RL350: the room draws (Change Each Visit On), the final fight's hold, the ghosts' distance / record per victim, the
# v3.50 session line
RZL_RDRAW = re.compile(r'^randomizer: outfit: room (\d+):(\d+) load \(world visit (\d+), room draw (\d+)\): (\w+) -> (Dress|Nurse) '
                       r'\(seed (\d+)\)$')
RZL_HELD = re.compile(r'^randomizer: outfit: room (\d+):(\d+) load: no room draw in the final fight \(stage (\d+)\): her outfit '
                      r'stays (\w+) for this visit$')
RZL_GMOVE = re.compile(r' -> (-?\d+) (-?\d+) (-?\d+) \(d (\d+)( near: the room allows no farther)?, (\d+) of (\d+) fit, record '
                       r'([a-z ]+)\)')
RZL_GKEPT = re.compile(r' kept \(its record: (pinned here|down here)\)')
RZL_SESS2 = re.compile(r'^randomizer: looks session: Eileen Outfit (\w+): (\d+) world visit\(s\), (\d+) world draw\(s\) \(all draws: '
                       r'Dress (\d+), Nurse (\d+)\), (\d+) room draw\(s\) \((\d+) changed her outfit\), (\d+) back to the set outfit, '
                       r'(\d+) chosen in the Extra Menu(.*?); (?:Ghosts|Spawn Site) (\w+): (\d+) room load\(s\) with victims, (\d+) victim\(s\), (\d+) '
                       r'moved \((\d+) of them 800 or more, (\d+) record\(s\) moved\), (\d+) kept \((\d+) by a pinned or down record\), '
                       r'(\d+) left as found(.*)$')
RZL_SESS = re.compile(r'^randomizer: looks session: Eileen Outfit (\w+): (\d+) world visit\(s\), (\d+) draw\(s\) \(Dress (\d+), Nurse '
                      r'(\d+)\), (\d+) with only the Dress unlocked, (\d+) back to the set outfit, (\d+) chosen in the Extra Menu(.*?); '
                      r'Ghosts (\w+): (\d+) room load\(s\) with victims, (\d+) victim\(s\), (\d+) moved, (\d+) kept, (\d+) left as '
                      r'found(.*)$')
LS_ROUTE = re.compile(r"^life steal: (heals through the game's heal|heals go to 0052D1D0 directly)")
INV_COUNTS = re.compile(r'^inventory: toolbelt flips (\d+), overflows to page 2 (\d+), page-2 story removals (\d+), re-equips from '
                        r'page 2 (\d+), rounds fired from page 2 (\d+), cross-page reloads (\d+), broken page-2 medallions (\d+); '
                        r'stacks: merges (\d+), chest take-outs (\d+), stacked breaks (\d+), broken ones dropped \(no room\) (\d+), '
                        r'full-belt pickups (\d+)')
# v3.38 EN338: Enemies Follow (enemies.inc): the door lines, arrivals, the spotted marks, waits, stops, story refusals
FOL_DOOR = re.compile(r'^enemies: through door \d+ from room \d+ to \d+: (\d+) follow \(.*?; (\d+) wait for a later door .*?, '
                      r'(\d+) stopped,')
# v3.58 EN358: the ghosts of each door (the door line's ghost count), what became of them in that room, the first sight
FOL_DOOR_G = re.compile(r'^enemies: through door \d+ from room \d+ to \d+: \d+ follow \(\d+ monster\(s\), \d+ of them copies, '
                        r'(\d+) ghost\(s\);')
FOL_GSUM = re.compile(r'^enemies: ghosts that followed you into room \d+: (\d+) of (\d+) came in \(the real ghost in the room; '
                      r'nearest (-?\d+) units from you\), (\d+) still arriving through the walls, (\d+) never in the room')
FOL_GIN = re.compile(r'^enemies: ghost [0-9A-F]{8} \(type [0-9A-F]{2}\) that followed you is in room \d+ now \(class [0-9A-F]+, '
                     r'(-?[\d.]+) s after the door.*?\)( \(a victim the randomizer drew\))?$')
FOL_CAME = re.compile(r'^enemies: (?:copy|follower) [0-9A-F]{8} \(type [0-9A-F]{2}\) came through the door into room')
FOL_SPOT = re.compile(r'^enemies: (?:monster|copy) [0-9A-F]{8} \(type [0-9A-F]{2}\) spotted you in room')
FOL_STOP = re.compile(r'^enemies: .*? stops? pursuing you(?! at the door)')
FOL_WAIT = re.compile(r'^enemies: .*? (?:waits in front of room|waits outside room) \d+: (.+?) \(it comes')
FOL_STORY = re.compile(r'^enemies: .*? does not come from room -?\d+ into room \d+: (.+?) \(it keeps pursuing you\)')
FOL_WHEEL = re.compile(r'^enemies: Wheelchair [0-9A-F]{8} \(home room \d+\) was recorded in room')
# v3.49 EF349: Enemies Follow's value (Off / Monsters / Victims Only / Monsters + Victims): at the start (the enemies install
# line), each change (the menu, the ini read again), what it takes (enemies.inc EnmFollowSay), the monsters sent home
FOL_VALUES = r'(Off|Monsters|Victims Only|Victims|Monsters \+ Victims)'  # v3.50 EF350: + Victims (v3.49's Victims Only)
FOL_MODE_IN = re.compile(r'^enemies: in \(.*?\); EnemyCopies [^,]+, EnemyFollow ' + FOL_VALUES + ', ')
FOL_MODE_SET = re.compile(r'^gameplay: (?:\[Gameplay\] )?EnemyFollow (?:read again: )?' + FOL_VALUES + r'(?: applies now|$)')
FOL_MODE_SAY = re.compile(r'^enemies: Enemies Follow ' + FOL_VALUES + ': ')
FOL_HOME = re.compile(r'^enemies: Enemies Follow is (off|Victims Only|Victims): (\d+) monster\(s\) left in other rooms went home')
FOL_HOME_CW = re.compile(r'^enemies: Enemies Follow is (?:off: (\d+) cop(?:y|ies) that|Victims(?: Only)?: (\d+) monster cop(?:y|ies) that)'
                         r' followed you (?:are made in their own rooms|is made in its own room) again')
# v3.50 EF350: the followers across saves (enmfollow.inc: kept at a save, found at a load, back at its room load) and Enemy
# Pursuit's lost-track lines (enemies.inc EfDoorLostSay) with its value (the install line, the menu / ini changes)
EF_SAVE = re.compile(r'^enemies: follow save: kept for this save \(stage \d+ room \d+, game time \d+, key [0-9A-F]{8}\): (\d+) '
                     r'pursuer\(s\) \((\d+) here, (\d+) away\), (\d+) on their way here, (\d+) ghost\(s\) let into this room, (\d+) '
                     r'cop\(ies\) that followed you; side file \d+ entr\(ies\)( \(not written)?')
EF_LOAD = re.compile(r'^enemies: follow load: (this save\'s followers are kept|no followers kept for this save)')
EF_REST = re.compile(r'^enemies: follow restore: room \d+:\d+ after a load: (\d+) pursuer\(s\) back \((\d+) here with you, (\d+) '
                     r'away\), (\d+) on their way here, (\d+) ghost\(s\) let in, (\d+) cop\(ies\) in the ledger; left out: (\d+) '
                     r'without their record \(the save filter, or gone\), (\d+) of a kind')
EF_REST_NO = re.compile(r'^enemies: follow restore: (?:the load went on to room|Enemies Follow is off)')
# v3.59 EN359: one line a load (followers restored N of M, the randomized followers' records put back from the side file),
# the kept records at a save, a pinned ghost let in by the gate (00465123), Walter's long down, the bars' anchors
EF_RESTORED = re.compile(r'^enemies: follow restore: followers restored (\d+) of (\d+)(?: \((\d+) of (\d+) record\(s\) of randomized '
                         r'followers put back)?')
EF_KEPTREC = re.compile(r'^enemies: follow save: (\d+) record\(s\) of followers the Randomizer made as another type kept')
EN_PINGATE = re.compile(r'^enemies: ghost ([0-9A-F]{8}) \(type ([0-9A-F]{2}) v\d+\) lies pinned by its Sword in room (\d+:\d+): it is '
                        r'made pinned there again')
EN_PINGATE_IN = re.compile(r'^enemies: (?:pinned ghosts stay \(00465123\)|pinned ghosts: the gate site is off|0046511D differs)')
HB_WLONG = re.compile(r'^hud bars: Walter\'s long down \(v3\.59\): .*?: (his bars stay|the heal\'s code differs)')
HB_ANCHOR = re.compile(r'^hud bars: bar anchor for type ([0-9A-F]{2}) v(\d+): (its top bone|its bones are not this frame\'s)')
HB_EDGE = re.compile(r'^hud bars: type ([0-9A-F]{2}) v(\d+): its head\'s top is above the 3D view')
# v3.56 ER356: Enemies Return Off / By Room / By World (enemies.inc ErAtRoomLoad, ErWorldPass, EnmReviveRoom, the Tremer site
# 0x48ACE5) and the world visit (worldvisit.inc: the world entries, the room loads after a save load); v3.30 - v3.55's On
ER_VALUES = r'(Off|On|By Room|By World)'
ER_MODE_IN = re.compile(r'^enemies: in \((.*?)\); .*?EnemyRevive ' + ER_VALUES + ', ')
ER_MODE_SET = re.compile(r'^gameplay: (?:\[Gameplay\] )?EnemyRevive (?:read again: )?' + ER_VALUES + r'(?: applies now|$)')
ER_MIGR = re.compile(r'^gameplay: EnemyRevive On \(v3\.30 - v3\.55\) is (By Room) now \(Enemies Return: Off, By Room, By World\): '
                     r'(written to SH4Refined\.ini|read only|could not be written)')
ER_WORLD = re.compile(r'^enemies: return: entered world (\d+) \(By World\): (\d+) room\(s\) walked, (\d+) killed monster\(s\) stand '
                      r'again \((\d+) bod(?:y|ies) taken away, (\d+) kill bit\(s\) cleared\), (\d+) released event dog\(s\) awake at '
                      r"their room's next load, (\d+) recorded alive left as they are, (\d+) story room\(s\) left as they are")
ER_LOADNONE = re.compile(r'^enemies: return: room (\d+):(\d+) after a load: nothing returns \((By Room|By World): ')
ER_TREMER = re.compile(r'^enemies: return: (\d+) killed Tremer\(s\) kept their body at a room exit this session')
ER_TSITE = re.compile(r'^enemies: return: (?:the Tremer site is off|0048ACE0 or 004215D0 differs)')
ER_ROOM = re.compile(r"^enemies: revive: room (\d+):(\d+): killed monsters stand again \((\d+) bod(?:y|ies) taken away, (\d+) kill "
                     r"bit\(s\) cleared, (\d+) bod(?:y|ies) of other rooms' monsters: they stand at home\), (\d+) killed cop(?:y|ies) "
                     r"come back")
WV_ENTRY = re.compile(r'^world visit: entered world (\d+) at room (\d+) \(from stage (-?\d+)\): world entry (\d+) of this session$')
WV_LOADL = re.compile(r'^world visit: room (\d+):(\d+) after a save load: no room change, no world entry')
EF_LOST = re.compile(r'^enemies: through door \d+ from room \d+ to \d+: Enemy Pursuit (\w+): (\d+) lost track of you \((\d+) out '
                     r'of their senses, (\d+) down when you left, (\d+) did not catch up\); (\d+) more had spotted you but forgot')
EF_VALUES = r'(Relentless|Normal|Forgetful)'
EF_IN = re.compile(r'^enemies: followers across saves (in|off|NOT in)\b.*?; Enemy Pursuit ' + EF_VALUES + ' ')
EF_SET = re.compile(r'^gameplay: (?:\[Gameplay\] )?EnemyPursuit (?:read again: )?' + EF_VALUES + r'(?: applies now|$)')
# v3.51 EP351: Enemy Pursuit inside the room (pursuit_room.inc): the install line, one verbose line per give-up, the room's line
EP_IN = re.compile(r'^enemies: Enemy Pursuit in the room (in|off|NOT in)\b')
EP_GIVE = re.compile(r'^enemies: (monster|copy|ghost|ghost copy) [0-9A-F]{8} \((?:([A-Za-z ]+), )?type ([0-9A-F]{2}) v\d+\) gave up the '
                     r'chase in room \d+:\d+: (it had not sensed you|it was down when you walked off|farther than|it chased you|'
                     r'you were out of its sight)')  # v3.52 EP352: + out of its sight
EP_ROOM = re.compile(r'^enemies: room \d+:\d+ \(Enemy Pursuit ' + EF_VALUES + r'\): (\d+) gave up the chase in the room \((\d+) '
                     r'monster\(s\), (\d+) ghost\(s\); (\d+) of them again, (\d+) down when you walked off\); (\d+) set back during '
                     r"their rest; (\d+) left to the game's own rules")
EP_WHY = {'it had not sensed you': 'unsensed', 'it was down when you walked off': 'downed', 'farther than': 'far',
          'it chased you': 'untouched', 'you were out of its sight': 'sight'}  # v3.52 EP352: + sight
# v3.52 EP352: the ghosts' calm (Normal, Forgetful): the room line's calm part (only when there is something to say)
EP_ROOM_CALM = re.compile(r"; ghosts' calm: (\d+) ghost dive\(s\) into a wall stopped, (\d+) ghost\(s\) sensed you again$")
# v3.41 EN341: a ghost copy made in another room without its record (the way the game's own ghosts arrive)
FOL_FRESH = re.compile(r"^enemies: ghost copy [0-9A-F]{8} \(type [0-9A-F]{2} v\d+\) (?:came into|is back in) room \d+ as the "
                       r"game's own ghosts arrive")
# v3.43 DF343: Depth of Field (dof.inc)
DOF_IN = re.compile(r'^depth of field: hooks in place .*?; DepthOfField (Off|Cut-scenes|Always), Strength (Low|Medium|High|Ultra);')  # v3.45 QS345: + Medium  # v3.59 DF359: + Ultra
DOF_NOT = re.compile(r'^depth of field: (.*?) is not stock .*?: no depth of field|^depth of field: not available \((.*?)\)')
DOF_SET = re.compile(r'^depth of field: DepthOfField (Off|Cut-scenes|Always) \(Strength (Low|Medium|High|Ultra)\)')  # v3.59 DF359: + Ultra
DOF_STR = re.compile(r'^depth of field: Strength (Low|Medium|High|Ultra) \(DepthOfField (Off|Cut-scenes|Always)\)')  # v3.59 DF359: + Ultra
# v3.45 QS345: the one-time migration of a v3.43 / v3.44 DofStrength (their Low = v3.45's Medium)
DOF_MIG = re.compile(r'^depth of field: \[ScreenEffects\] DofStrength=(.*?) \(v3\.43 / v3\.44\) is v3\.45\'s (Low|Medium|High) .*?: (written|could not)')
DOF_ROOM = re.compile(r'^depth of field: stage ([0-9A-F]+) room ([0-9A-F]+): drawn in (\d+) of (\d+) frames .*?; ours ([\d.]+) ms a frame '
                      r'on average, ([\d.]+) at most;')
DOF_PROBE = re.compile(r'^depth of field: stage ([0-9A-F]+) room ([0-9A-F]+): depth probe .*?: (the scene.s depth: on|flat|her figure|too few|'
                       r'not probed: no read back|not probed)')  # v3.45 RB345: + no read back
DOF_NOREAD = re.compile(r'; probe: not probed: no read back')  # v3.45 RB345: the room line, once per room
# v3.45 RB345: the GPU -> CPU read back's path (post.inc RbBegin): the one that worked, a switch, or none on this backend
RB_OK = re.compile(r'^read back: GPU -> CPU through (.+?) on this backend \(CopyRects, then LockRect; first used by (.+?), (\d+)x(\d+)\)')
RB_SW = re.compile(r'^read back: now through (.+?) \(')
RB_LOST = re.compile(r'^read back: (.+?) failed this time ')
RB_OFF = re.compile(r'^read back: no GPU -> CPU read back on this backend \((.*?); last by ')
DOF_OFF = re.compile(r'^depth of field: off for this session: (.*?) \(hr ')
DOF_KEPT = re.compile(r'^depth of field: the paused scene kept for the (?:Depth of Field page|Screen Effects preview)')  # v3.44 DF344: + preview
# v3.44 DF344: the room line's reasons ("not drawn: <reason> <n>, ..."; "skipped by design: ...")
DOF_WHY = re.compile(r'\(not drawn: (.*?); the cut-scene.s own focus \d+\); skipped by design: (.*?); focus ')
# v3.47 DF347: the blur eases in (quarter-level ramps) or, without the blend shader, steps by whole levels
DOF_RAMPS = re.compile(r'^depth of field: the blur eases in: Low (\d+) far \+ (\d+) near slices, Medium (\d+) \+ (\d+), High (\d+) \+ (\d+)'
                       r'(?:, Ultra (\d+) \+ (\d+))?')  # v3.59 DF359: + Ultra
DOF_FIRST = re.compile(r'^depth of field: first frame drawn \((.*?), (Low|Medium|High|Ultra)\): .*?; render targets [\d.]+ MB; (eased in|whole levels)')
# v3.59 DF359: a v3.45 .. v3.58 Medium / High kept (now a bit gentler) and noted once; Spirit Vision kept sharp (once a
# session); the hidden DofSpirit=1 (v3.58's order)
DOF_NOTE59 = re.compile(r"^depth of field: \[ScreenEffects\] DofStrength=(.*?): kept; v3\.59's (Medium|High) is a bit gentler")
DOF_SPIRIT = re.compile(r'^depth of field: Spirit Vision draws: depth of field keeps out of it')
DOF_SPIRITKEEP = re.compile(r'^depth of field: \[ScreenEffects\] DofSpirit=1: ')
DOF_NOBLEND = re.compile(r'^depth of field: the blend shader \(bloom.s up-step\) could not be made')
# v3.52 FX352: the player close to the camera (a focus, the far side only; v3.51 took it for a first-person view)
DOF_CLOSE = re.compile(r'; focus [\d.]+ \.\. [\d.]+ \(the player close to the camera in (\d+) frame\(s\)\)')
DOF_CLOSE1 = re.compile(r'^depth of field: stage ([0-9A-F]+) room ([0-9A-F]+): the player is close to the camera \(view depth ([-\d.]+)\)')
# v3.40 SH340: Shadow Edge (shadows.inc)
SH_IN = re.compile(r'^shadows: hooks in place .*?; Edge (Stock|Soft|Softer);')
SH_NOT = re.compile(r'^shadows: (.*?) is not stock .*?: no Shadow Edge')
SH_EDGE = re.compile(r'^shadows: Edge (Stock|Soft|Softer)\b')
SH_PLAN = re.compile(r'^shadows: (Soft|Softer) at (\d+)x(\d+): (\d) pass\(es\) down to (\d+)x(\d+), composite taps ([\d.]+) texels: a ([\d.]+) px edge')
SH_ROOM = re.compile(r'^shadows: room ([0-9A-F]+):([0-9A-F]+): (\d+) composites over (\d+) frames \(at most (\d+) in a frame\), (\d+) soft; '
                     r'ours ([\d.]+) ms a composite on average, ([\d.]+) at most')
SH_OFF = re.compile(r'^shadows: off for this session: (.*?) \(hr ([0-9A-F]+), (.*?)\)')
# v3.58 GX358: the stock Realtime Shadow switch (Shadows), the composites the game drew with it Off, the preview's Off
SH_SW = re.compile(r'^shadows: Shadows (On|Off) \(the game\'s Realtime Shadow, cfg\+0x19')
SH_SW_HERE = re.compile(r'; here \(stage ([0-9A-F]+) room ([0-9A-F]+)\): (kept|none);')
SH_ROOM_OFF = re.compile(r'; (\d+) with Shadows Off \(the game keeps this room\'s\)')
SH_PV_OFF = re.compile(r'^shadows: the Shadows page\'s preview: (Shadows Off: no real-time shadow here|the split shadows again)')
# v3.52 SH352: the room line's tail (why composites were not soft: hard with their reasons, none with no darkness) and the
# once-a-room reason line
SH_ROOM_NS = re.compile(r'; not soft: (\d+) hard(?: \((.*?)\))?, (\d+) with no darkness')
SH_NS = re.compile(r'^shadows: room ([0-9A-F]+):([0-9A-F]+): a composite not soft: (.*?) \(darkness ([0-9A-F]{8}), edge (\S+), '
                   r'Darkness (\d+) %')
# v3.49 SH349: what the edge and the Darkness cover (every real-time shadow) and the Tremers' wall shadows at the Darkness
SH_COVER = re.compile(r'^shadows: the edge and the Darkness cover every shadow the game draws: .*?; the Tremers\' wall shadows: '
                      r'(the Darkness|the game\'s own)')
SH_RIB = re.compile(r'^shadows: a Tremer\'s wall shadow drawn at Darkness (\d+) %: its alpha ([0-9A-F]{2}) -> ([0-9A-F]{2}) \((\d+) % of')
SH_RIB_ROOM = re.compile(r'^shadows: room ([0-9A-F]+):([0-9A-F]+): (\d+) Tremer wall shadow draw\(s\), (\d+) at the Darkness \((\d+) %')
SH_RIB_NOT = re.compile(r'^shadows: (\[Shadows\] TremerShadows=0|the Tremer\'s wall shadow draw \S+ is not stock|0045F4CA could not be written)')
SH_KEPT = re.compile(r'^shadows: the paused scene kept for the Shadows page: (\d+)x(\d+) picture \+ mask \([\d.]+ MB'
                     r'(?:; (targets made|targets kept from an earlier pause:) ([\d.]+) ms, copied ([\d.]+) ms)?')  # v3.41 SH341: timed
# v3.41 SH341: the new Soft's plan, v3.40's Softer read as Soft, the pause's kept targets, the Screen Effects page's
# pause picture on the GPU, and any frame still read back from the GPU (PreviewKey, the first-pause file, a fallback)
SH_PLAN41 = re.compile(r'^shadows: (Soft) at (\d+)x(\d+): down to (\d+)x(\d+) \((\d) level\(s\)\), (\d) blur pass\(es\).*?: a ([\d.]+) px '
                       r'edge \(asked ([\d.]+).*?; (\d+) draws, at most a ([\d.]+)x stretch')
SH_OLDINI = re.compile(r'^shadows: \[Shadows\] Edge=(\S+) \(v3\.40\) reads as Soft')
SH_KEEP = re.compile(r'^shadows: the pause ended: its full-size targets \(([\d.]+) MB')
SH_LOWVA = re.compile(r'^shadows: the preview\'s copies freed \(the address space has (\d+) MB left')
FX_GPU = re.compile(r'^screen effects: the pause picture kept on the GPU for the Screen Effects page: (\d+)x(\d+) \([\d.]+ MB; '
                    r'(target made|target kept from an earlier pause:) ([\d.]+) ms, copied ([\d.]+) ms')
FX_READ = re.compile(r'^screen effects: a frame read back from the GPU \((\d+)x(\d+), route (\d)\): copy ([\d.]+) ms, read back ([\d.]+) ms, '
                     r'shrink ([\d.]+) ms')
# v3.43 PP343: Bloom, Colour Grading, Otherworld Lens (post.inc)
PP_IN = re.compile(r'^post effects: in place \(.*?device reset 00402DF4 (hooked|NOT hooked)\); Bloom (Very High|\S+), Colour Grading (\S+), '
                   r'Otherworld Lens (\S+)')  # v3.44 CS344: + Very High
PP_INI_OFF = re.compile(r'^post effects: off \(\[ScreenEffects\] PostEffects=0\)')
PP_SET = re.compile(r'^screen effects: (Bloom|Grading|Lens) (Very High|\S+)')  # v3.44 CS344: + Very High
PP_FAIL = re.compile(r'^post effects: (bloom|colour grading|the Otherworld lens) off for this session: (.*?) \(hr ([0-9A-F]+)\)')
PP_LENS = re.compile(r'^post effects: the Otherworld lens fades (in|out) \(stage ([0-9A-F]+) room ([0-9A-F]+); haunting strength '
                     r'([-\d.]+), nearness ([-\d.]+): (\d+) %\)')
PP_CHAIN = re.compile(r'^post effects: bloom chain at (\d+)x(\d+): .*?: (\d+) draws, ([\d.]+) MB of render targets')
PP_NOTF = re.compile(r'^post effects: (bloom|colour grading|the Otherworld lens) (Very High|\S+) not drawn under the filter (.+?) \(')
PP_DRAWN = re.compile(r'^post effects: (bloom|colour grading|the Otherworld lens) (Very High|\S+) drawn over (.+?) \(before the filter')
# v3.52 FX352: Bloom Off / Low / Medium / High / Ultra: a v3.43 .. v3.51 value migrated once; the lens' room line
PP_MIG = re.compile(r'^post effects: \[ScreenEffects\] Bloom=(.*?) \(v3\.43 \.\. v3\.51\) is v3\.52\'s (Off|Low|Medium|High|Ultra) '
                    r'.*?: (written|could not)')
PP_LENS_ROOM = re.compile(r'^post effects: the Otherworld lens in stage ([0-9A-F]+) room ([0-9A-F]+): shown (\d+) of (\d+) s \((\d+) %\), '
                          r'at most (\d+) % strength \(haunting strength up to ([-\d.]+), nearness up to ([-\d.]+)')
# v3.44 CS344: the pixel checks (a fixed test draw through the real shader, read back): the grade and the shadow composite
PP_PX = re.compile(r'^post effects: colour grading (\S+): pixel check (ok|FAILED): .*?(\d+) level\(s\) off its sim at most, '
                   r'([\d.]+) levels changed on average \(the sim ([\d.]+)\): (.*)$')
PP_PX_NOT = re.compile(r'^post effects: colour grading (\S+): pixel check not run \((.*)\)$')
SH_PX = re.compile(r'^shadows: pixel check \((in play|the Shadows page), darkness ([0-9A-F]{8}) at Darkness (\d+) %\) (ok|FAILED): '
                   r'.*?full cover alpha ([\d.]+), asked ([\d.]+) .*? none [\d.]+: (.*)$')
SH_PX_NOT = re.compile(r'^shadows: pixel check \((in play|the Shadows page), darkness ([0-9A-F]{8}) at Darkness (\d+) %\) not run \((.*)\)$')
PS_LINT = re.compile(r'^post effects: every screen shader in use uses proven ps\.1\.1 forms only')
# v3.47 PX347: the pixel checks' pattern: the test draw's sampling offset (the cells' edges) and the source as read back
PX_ALIGN = re.compile(r'^pixel check alignment \((.+?)\): the test draw samples ([\d.]+) texel\(s\) (left|right) of the texel centres '
                      r'\(from (\d+) cell edge')
PX_ALIGN_NO = re.compile(r'^pixel check alignment \((.+?)\): not measurable')
PX_SRC = re.compile(r'; (the source read back as asked|the source held other values in (\d+) cell\(s\)|the source not read back)')
SH_PREV = re.compile(r'^shadows: the Shadows page\'s preview: (the paused scene|a figure\'s shadow|none)')
# v3.43 FN343: the HD menu font ([Textures] MenuFont, font.inc)
FN_SET = re.compile(r'^settings: \[Textures\] MenuFont (.+?): fonts\\(\S+)\.sh4font \((.*?), 4x; fonts 0 / 1: (\d+) / (\d+) glyphs\)')
FN_LIVE = re.compile(r'^font: (.*?) \(fonts\\(\S+)\.sh4font\): the glyph page is now 2048 x 2048')
FN_MATCH = re.compile(r'^font: font (\d) \(struct \S+\) matches ')
FN_NOMATCH = re.compile(r'^font: font (\d) \(struct .*?\) is not the font ')
FN_STOCK = re.compile(r'^font: (.*?): the stock font')
# v3.44 FN344: the Menu Font row (Options > Option 1) and its live switches
FN_ROW = re.compile(r'^font: Options > Option 1 has a Menu Font row')
FN_SWITCH = re.compile(r'^font: Menu Font -> (.+?) \(Options, live')
# v3.47 SB347: the save / load list's Bank row (savebanks.inc): its first draw and the mouse's bank clicks
SB_ROW = re.compile(r'^save banks: the Bank row draws "(.+?)" in the list\'s own font \(Menu Font (.+?)\), (-?\d+) units wide')
SB_CLICK = re.compile(r'^save banks: mouse click on the (left arrow|right arrow|Bank row): bank (\d+) -> (\d+)$')
# v3.48 SB348: the per-step line (normal in v3.47 and older, verbose from a save / load list now), the settled change
SB_STEP = re.compile(r'^save banks: bank (\d+)$')
SB_SETTLE = re.compile(r'^save banks: bank (\d+) -> (\d+) in the (save|load|save / load) list \((\d+) steps?: keys (\d+), '
                       r'pad (\d+), mouse (\d+); by 10: (\d+)\)$')
# v3.49 SB349: the lists' highlight bar at the start (savebanks.inc SbListBarInstall)
SB_BAR = re.compile(r'^save banks: list highlight bar: (blue|yellow) \((.+)\)$')
SH_RT = re.compile(r'^shadows: render target made: .*?ours now ([\d.]+) MB')
# v3.41 VW341: Room 302's window views at 4:3 and the Borderless edge-cover check (sh4windowmod.c)
VW_VIEW = re.compile(r"^window views: Room 302's window view: black sides (\d+) \| (\d+) px beside the 4:3 view \((\d+)x(\d+)\)")
EDGE_COVER = re.compile(r'^window: borderless edges: (\d+) window\(s\) above the game cover its edges: (.*); the game is (in front|not in front)')
# v3.42 EG342: the 2D edge decision (sh4windowmod.c SetConstants2D): canvases on pixel edges, the black sides' widths
EDGE_2D = re.compile(r'^2D edges: (\d+)x(\d+): (the 2D canvases sit on pixel edges|\[Window\] PixelAlign2D=0)')
EDGE_2D_SIDES = re.compile(r'^2D edges: (\d+)x(\d+): black sides (\d+) \| (\d+) px')
# v3.40 LG340: the session's start date (logrotate.inc), the session end line's numbers (loghealth.inc LhSessionEndC),
# a rotated log's file name (logrotate_sim.h LrMakeName), the Renderer row's start state (renderer.inc RdLoad's why)
STARTED = re.compile(r'^log: started (\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)')
ROTATED = re.compile(r'^SH4Refined (\d{4})-(\d\d)-(\d\d) (\d\d)(\d\d) (v[\w.+-]+|unknown)( #[2-9])?\.log$', re.I)
END_AFTER = re.compile(r'^end \(([^)]*)\) after (\d+):(\d\d):(\d\d): (\d+) room visit\(s\)(?: \((\d+) rooms\))?')
END_FRAMES = re.compile(r'frames (\d+): mean ([\d.]+) ms, p99 ([\d.]+), worst ([\d.]+)')
END_SPIKES = re.compile(r'spikes (\d+), stalls (\d+)')
END_VA = re.compile(r'VA peak (\d+) of (\d+) MB')
END_ANOM = re.compile(r'(\d+) ANOMALY line\(s\)')
REND_CONF = re.compile(r'^dgVoodoo\.conf is (.+) \(([\w.+-]+), [0-9A-F]{32}\)$')
REND_KNOWN = re.compile(r'^dgVoodoo\.conf is ([\w.+-]+) \([0-9A-F]{32}, recognised only')
OLD_ROOM = [
    re.compile(r'stage ([0-9A-F]+) room ([0-9A-F]+)(?:[:)]| \()'),   # echo / after shock / depth fix room lines
    re.compile(r'enemies: room ([0-9A-F]+):([0-9A-F]+) copies'),
]
NUM = re.compile(r'\b(?:0x)?[0-9A-Fa-f]*\d[0-9A-Fa-f]*(?:\.\d+)?\b')
SIZE = re.compile(r'\b\d+x\d+\b')
DUMPS = ('SH4EileenWeapons.crash.dmp', 'SH4Refined.crash.dmp')


def norm(msg, width=110):
    """A line's kind: numbers and addresses as #, cut to width."""
    m = NUM.sub('#', SIZE.sub('#x#', msg))
    m = re.sub(r'"[^"]*"', '"..."', m)
    return m[:width]


def secs(ts):
    h, m, s, ms = (int(x) for x in ts.groups())
    return h * 3600 + m * 60 + s + ms / 1000.0


def hms(t):
    t = int(t)
    return '%d:%02d:%02d' % (t // 3600, t // 60 % 60, t % 60)


# v3.50 FI350: Fade In (fadein.inc)
FI_MODE = r'(Off|Fade \+ Portal|Fade)'  # (the longer name first)
# v3.51 PT351: "no code patch" (the step list's entry 8 is data) and the tar portal's part (optional: v3.50 logs read too)
FI_IN = re.compile(r'^fade in: in \(no (?:exe|code) patch: .*?; the portal (from the scene pass 004EFBFD|NOT available[^)]*\))'
                   r'(?:; the tar portal (.*?))?\); '
                   r'\[Gameplay\] FadeIn ' + FI_MODE + r', FadeInSeconds ([\d.]+)(?:, FadeInPortalDraw (\w+), FadeInHold (\d))?'
                   r'(?:, FadeInStrandSeconds ([\d.]+), FadeInPortalPieces (\d+), FadeInStrands (\d+))?')  # v3.58 FI358
FI_OFF = re.compile(r'^fade in: OFF \((.+?)\); enemies appear as the game makes them')
FI_ROOM = re.compile(r'^fade in: room (\d+):(\d+): (\d+) fade\(s\) \(room load (\d+), follower (\d+), wanderer (\d+), copy (\d+), '
                     r'revive (\d+), ghost (\d+), monster (\d+)\), (\d+) portal\(s\); (\d+) not faded \(stage (\d+), cut-scene (\d+), '
                     r'type (\d+), boss (\d+), event (\d+), dead (\d+), own draw (\d+), own portal (\d+), own fade (\d+)\)')
# v3.51 PT351: the room line's portal part "P portal(s) (the game's own draw a, Refined's b, vortex c; h held; portal draw x ms a
# frame, max y)" (taken out before FI_ROOM reads the rest, so v3.50's form reads as before)
FI_ROOM_TAR = re.compile(r" \(the game's own draw (\d+), Refined's (\d+), vortex (\d+); (\d+) held; portal draw ([\d.]+) ms a frame, "
                         r"max ([\d.]+)\)(?=; \d+ not faded)")
FI_SRC = ('room load', 'follower', 'wanderer', 'copy', 'revive', 'ghost', 'monster')
FI_SKIP = ('stage', 'cut-scene', 'type', 'boss', 'event', 'dead', 'own draw', 'own portal', 'own fade')
FI_SET = re.compile(r'^fade in: FadeIn ' + FI_MODE + r'( applies now but could not be saved.*)?$')
FI_REREAD = re.compile(r'^fade in: \[Gameplay\] FadeIn read again: ' + FI_MODE + '$')
FI_BAD = re.compile(r'^fade in: \[Gameplay\] FadeIn "([^"]*)" is not Off / Fade / Fade \+ Portal: ' + FI_MODE + r' \(the default\)')
FI_FAULT = re.compile(r'^fade in: a fault in (.+?) \(code ([0-9A-F]{8})')
FI_START = re.compile(r'^fade in: (room load|follower|wanderer|copy|revive|ghost|monster) ([0-9A-F]{8}) \(type ([0-9A-F]{2})\) '
                      r'fades in over ([\d.]+) s( from a blood portal| from a tar portal \((the game\'s own draw|Refined\'s draw), '
                      r'(on a wall|in the air) (\d+) u behind; ([^)]*)\))?')
FI_DRAWN = re.compile(r"^fade in: a (?:blood|tar) portal drawn \((?:(the game's own draw|Refined's draw), )?viewport (3D|saved) ")
# v3.51 PT351: the hold's lines
FI_LETGO = re.compile(r"^fade in: ([0-9A-F]{8}) \(type ([0-9A-F]{2})\) let go by its tar portal \((the game's own draw|Refined's draw)\) "
                      r"after ([\d.]+) s \(([^)]*)\): strands: (none|the ghost's own|Refined's) (\d+); moved (\d+) of (\d+) u; no attack "
                      r"for ([\d.]+) s; portal draw ([\d.]+) ms a frame \(max ([\d.]+), (\d+) frames\)")
FI_REAL = re.compile(r'^fade in: ([0-9A-F]{8}) \(type ([0-9A-F]{2})\) is real now, ([\d.]+) s after its tar portal opened: its hold (?:starts|goes on)')
FI_NEVER = re.compile(r'^fade in: ([0-9A-F]{8}) \(type ([0-9A-F]{2})\): its tar portal closed after ([\d.]+) s: it never came out')
FI_GFAULT = re.compile(r"^fade in: a fault in the game's own portal draw \(code ([0-9A-F]{8}) at ([0-9A-Fa-fx]+)\)")
FI_NOTSTOCK = re.compile(r'^fade in: ([0-9A-F]{8}) \((.+?)\) is not the stock code \(another mod\?\): the tar portal does without it')
FI_TDM_BAD = re.compile(r'^fade in: \[Gameplay\] FadeInPortalDraw "([^"]*)" is not Auto / Refined / Vortex')
FI_OWN = re.compile(r'^fade in: type ([0-9A-F]{2}) is drawn with a draw state of its own \(pre-draw ([0-9A-F]{8})\)')
# v3.58 FI358: a tar portal's close line (fadein_sim.h FiCloseText) and a bad FadeInStrandSeconds
FI_CLOSE = re.compile(r"^fade in: ([0-9A-F]{8}) \(type ([0-9A-F]{2})\): its tar portal closed at ([\d.]+) s: (\d+) piece\(s\) \(grids (\d+) "
                      r"of (\d+), blot sets (\d+), drips (\d+)\), (\d+) x (\d+) u for a body (\d+) u tall \(([\d.]+) x, (its bones|its "
                      r"type's box|not seen)\); strands: the ghost's own (\d+), Refined's (\d+) \((its bones|its box)\), held ([\d.]+) s "
                      r"after it was in \(to ([\d.]+) s; (\d+) snapped, (\d+) stretched loose\); portal draw ([\d.]+) ms a frame "
                      r"\(max ([\d.]+), (\d+) frames\)")
FI_STICK_BAD = re.compile(r'^fade in: \[Gameplay\] FadeInStrandSeconds "([^"]*)" is not 0 \.\. 6')
# v3.59 FI359: the arrive line (fadein_sim.h FiArriveText), the wall-exit wait cap, the Spirit Vision look, the start line's part
FI_ARRIVE = re.compile(r'^fade in: ([0-9A-F]{8}) \(type ([0-9A-F]{2})\) arrives: (?:its tar portal on the floor \(foot (-?\d+) u above '
                       r'it, raised (-?\d+) u, (full size|a low ceiling): (\d+) %\)|its tar portal: no floor found under it \(as laid '
                       r'out\))(?:; it came out (-?\d+) u above the floor \(its own height (\d+) u\): (?:lifted (\d+) u|left as it was))?'
                       r'; AI state ([0-9A-F]+) step (\d+), animation (-?\d+)(?:; unseen ([\d.]+) s before)?')
FI_WAITCAP = re.compile(r'^fade in: ([0-9A-F]{8}) \(type ([0-9A-F]{2})\) still comes out of a wall by itself after ([\d.]+) s')
FI_SPIRIT = re.compile(r"^fade in: Spirit Vision: (the game's own wall portal|Reveal Method's tar portal)")
FI_IN359 = re.compile(r'; v3\.59: (on the floor|NOT on the floor)[^;]*; .*?FadeInSpiritLook (\d)')


class Session:
    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path)
        self.lines = 0
        self.size = os.path.getsize(path)
        self.t0 = self.t1 = None
        self.span = 0.0
        self.version = None
        self.header = []          # session: lines (v3.33)
        self.display = None
        self.not_default = []
        self.old_settings = []
        self.level = None
        self.end = None
        self.end_feats = None
        self.anomalies = OrderedDict()   # kind -> [count, first ts, last ts, example]
        self.faults = OrderedDict()
        self.warnings = OrderedDict()
        self.rooms = []           # (stage, room, dict)
        self.loads = []           # (ts, game time, stage:room, ledger, visit, budget, free, of)
        self.old_rooms = OrderedDict()
        self.feats = OrderedDict()  # name -> [rooms on, rooms acted, total, values seen]
        self.kinds = Counter()
        self.levels = Counter()
        self.budget = None        # v3.35 FX335: 'in' / 'off'
        self.budget_rooms = []    # (stage, room, dict)
        self.budget_steps = [0, 0, 0]  # down, up, deepest level
        self.bhr = None           # v3.56 PW356: the high-rate fallback 'in' / 'off'
        self.bhr_rules = []       # (ts, fps, 'high' / 'v3.55')
        self.bhr_blind = 0        # blind steps, taken back, kept, the go-ahead lines
        self.bhr_back = []        # (ts, gain %)
        self.bhr_kept = []        # (ts, level, gain %)
        self.bhr_yield = []       # (ts, fps, level)
        self.bhr_rooms = []       # (stage:room, frames, blind, kept, back, sparks %)
        self.bq = None            # v3.58 GX358: the quick rules 'on' / 'off'
        self.bq_pre = []          # (ts, from, to, fps, powers)
        self.bq_refill = [0, 0]   # refill steps up, take-backs
        self.bq_quick = 0         # quick steps
        self.bq_rooms = []        # (stage:room, start steps, quick steps)
        self.ots_cam = Counter()  # v3.56 PW356: After Shock's over-the-shoulder camera on / off lines
        self.ots_fade = None      # (keep %, box w, box h) the last fade line
        self.ots_ini = None
        self.ots_rooms = []       # (stage:room, frames, dimmed, least %)
        self.warms = []           # (ts, stage:room, total ms, {part: ms})
        self.dpg = Counter()      # v3.41 DP341: Death Presence hidden, by reason
        self.dpg_shown = [0, 0.0, '', '']  # shown again: count, the longest hold (s), its room, its ts
        self.dpg_long = []        # (ts, stage:room, reason) of the holds past 20 s
        self.dpg_warp = 0         # pending moves no longer waited for
        self.r302 = None          # v3.42 SM342: 'on' / 'off' (Death Presence in Room 302, the install line)
        self.r302_n = 0           # v3.42 SM342: entries into Room 302 with Death Presence on (its line)
        self.vg = Counter()       # v3.42 SM342: 'echo' / 'after shock' hidden in a view
        self.vg_why = Counter()   # v3.42 SM342: their reasons
        self.vg_shown = [0, 0.0, '']  # v3.42 SM342: shown again: count, the longest hide (s), its ts
        self.sh_dark = []         # v3.42 SM342: (ts, pct) at the start and each change
        self.sh_dark_use = OrderedDict()  # v3.42 SM342: 'Stock 32000000 85' -> (inside, the game's)
        self.hb_cm42 = None       # v3.42 SM342: the Curse Meter Off / On line's ts
        self.sh_c1 = OrderedDict()  # v3.43 SH343: ('play' | 'page', 'darkness pct') -> (c1.a, light %, read back, ts)
        self.sh_px = []           # v3.44 CS344: (ts, 'page' | 'play', 'darkness pct', 'ok' | 'FAILED' | 'not run', measured, asked, why)
        self.pp_px = []           # v3.44 CS344: (ts, preset, 'ok' | 'FAILED' | 'not run', levels off, changed, the sim's change, why)
        self.rb = None            # v3.45 RB345: (ts, 'ok' | 'none', the path or the tries, first user)
        self.rb_sw = []           # v3.45 RB345: (ts, 'switch' | 'lost', path)
        self.dof_noread = 0       # v3.45 RB345: room lines with "probe: not probed: no read back"
        self.ps_lint = None       # v3.44 CS344: True = every screen shader proven (the start line)
        self.px_align = []        # v3.47 PX347: (ts, who, offset in texels (- = left) or None, edges)
        self.px_src = Counter()   # v3.47 PX347: 'as asked' / 'lost' (a Clear that did not land) / 'not read back'
        self.px_lost = []         # v3.47 PX347: (ts, the check's line head, cells)
        self.hb_cm42_old = None   # v3.42 SM342: (old value, read as)
        self.hits = []            # v3.36 HE336: (stage:room, lowest free, of, refusals here, stomps here, ts)
        self.recs = []            # v3.36 HE336: (stage:room, most at the exit, copies', real, for copies, purged, ts)
        self.inv = None           # v3.37 INV337: (toolbelt, stackable, guards) at the start
        self.inv_sw = Counter()   # v3.37 INV337: 'extended toolbelt on' / '... off' / '... stays on' / 'stackable items on' ...
        self.inv_counts = None    # v3.37 INV337: the last counters line (12 numbers)
        self.gt = None            # v3.43 GT343: (in / OFF, On / Off) from the install line
        self.gt_warps = Counter()  # v3.43 GT343: warps by enemy type
        self.gt_no = Counter()    # v3.43 GT343: refusals by reason (the text before its details)
        self.gt_cool = 0          # v3.43 GT343: presses that waited for the cooldown
        self.gt_chord = Counter()  # v3.44 GT344: chords by which press came last ('taken back', 'NOT taken back', 'Action first')
        self.gt_fades = [0, 0]    # v3.44 GT344: fades ended, of them never drawn see-through on her body
        self.gt_fdraw = None      # v3.58 GT358: the start's fade draw (Clean / One pass / Stock), why Clean is not in
        self.gt_fnotin = None
        self.gt_fpath = {}        # v3.58 GT358: filter -> [fades, late, pre-pass, shader 4, shader 5, missed, heat, soul, shadow]
        self.gt_off = Counter()   # v3.44 GT344: fades called off, by reason
        self.gt_holds = []        # v3.49 GT349: (warps, refused) of every hold that tried more than once
        self.gt_zones = {}        # v3.49 GT349: 'stage:room' -> walk-in event zones the warp kept out of
        self.gt_zone_no = 0       # v3.49 GT349: refusals with spots refused for an event's zone
        self.gt_faced = 0         # v3.50 TP350: warps behind the enemy she faces
        self.gt_rooms = []        # v3.50 TP350: room visits' lines: (room, warps, through, faced, ahead, refused, taken back)
        self.gt_room_no = Counter()  # v3.50 TP350: refusals by kind, from the room visits' lines
        self.gt_maps = []         # v3.50 TP350: finished floor maps: (room, cells, seeds, no floor, ms)
        self.gt_map_open = 0      # v3.50 TP350: room visits left with the floor map not finished (or overflowed)
        self.gt_skip = 0          # v3.51 TP351: enemies in the cone skipped behind collision (the room visits' lines)
        self.gt_skip_tries = 0    # v3.51 TP351: the tries with such a skip
        self.gt_free = Counter()  # v3.52 TP352: freed from a wall: 'after warp' / 'at the warp' (the lines)
        self.gt_free_no = 0       # v3.52 TP352: left inside collision (no proven floor cell, or not moved)
        self.gt_room_freed = 0    # v3.52 TP352: freed, from the room visits' lines
        self.gt_lv = []           # v3.59 GT359: warps up / down a level: (dir, height gained, candidates)
        self.gt_lv_flat = {}      # v3.59 GT359: 'stage:room' -> 'up' / 'down': no landing on another level proven there
        self.gt_room_lv = [0, 0]  # v3.59 GT359: up / down a level, from the room visits' lines
        self.fi = None            # v3.50 FI350: Fade In: ('in', portal, mode, seconds) or ('OFF', why) from the install line
        self.fi_rooms = 0         # v3.50 FI350: room visits with a Fade In line
        self.fi_src = Counter()   # v3.50 FI350: fades by source
        self.fi_skip = Counter()  # v3.50 FI350: kept from fading, by reason
        self.fi_portals = 0       # v3.50 FI350: blood portals started
        self.fi_drawn = None      # v3.50 FI350: (ts, viewport) of the first portal drawn
        self.fi_sets = []         # v3.50 FI350: (ts, value, how) the row's / ini's changes
        self.fi_bad = None        # v3.50 FI350: (text, value) an unknown [Gameplay] FadeIn
        self.fi_fault = None      # v3.50 FI350: (ts, where, code)
        self.fi_starts = Counter()  # v3.50 FI350: the verbose per-fade lines by source
        self.fi_own = set()       # v3.50 FI350: 'type 0A (pre-draw 0045C840)' drawn with a state of its own
        self.fi_tar = None        # v3.51 PT351: (tar part of the install line, FadeInPortalDraw, FadeInHold) or None (v3.50)
        self.fi_by = Counter()    # v3.51 PT351: portals by draw ('game', 'Refined', 'vortex'), from the room lines
        self.fi_held = 0          # v3.51 PT351: ghosts held, from the room lines
        self.fi_cost = [0, 0.0, 0.0]  # v3.51 PT351: room lines with a portal draw, the worst per-frame average, the worst frame (ms)
        self.fi_letgo = []        # v3.51 PT351: (id, draw, s, why, strands, n, moved, unheld, no attack s, avg ms, max ms, frames)
        self.fi_real = 0          # v3.51 PT351: placeholders that turned real (their hold goes on in the real ghost)
        self.fi_never = 0         # v3.51 PT351: tar portals closed with nobody coming out
        self.fi_close = []        # v3.58 FI358: (id, closed at, grids, of, sets, w, h, body, ratio, body from, game, own, own from,
                                  #   stick s, to s, snapped, loose, avg ms, max ms)
        self.fi_big = None        # v3.58 FI358: (FadeInStrandSeconds, FadeInPortalPieces, FadeInStrands) of the start line
        self.fi_stick_bad = None  # v3.58 FI358: a FadeInStrandSeconds that is not a number of seconds
        self.fi_arrive = []       # v3.59 FI359: (id, type, foot or None, raised, k %, above, rest, lift, ai, step, anim, unseen s)
        self.fi_waitcap = 0       # v3.59 FI359: victims still coming out of their wall at the wait's cap
        self.fi_spirit = set()    # v3.59 FI359: what Spirit Vision drew as spirit tar
        self.fi59 = None          # v3.59 FI359: (floor, FadeInSpiritLook) of the start line
        self.fi_tar_starts = Counter()  # v3.51 PT351: verbose starts by (draw, wall / air)
        self.fi_gfault = None     # v3.51 PT351: (ts, code) a fault in the game's own portal draw
        self.fi_notstock = []     # v3.51 PT351: game code the tar portal found changed
        self.fi_tdm_bad = None    # v3.51 PT351: an unknown FadeInPortalDraw text
        self.fi_tar_drawn = None  # v3.51 PT351: (ts, draw) of the first tar portal drawn
        self.om = None            # v3.46 OM346: (in / OFF, mode, dead zone) from the install line
        self.om_not = None        # v3.46 OM346: the site that was not stock
        self.om_sets = []         # v3.46 OM346: (ts, new, old, from)
        self.om_dz = []           # v3.46 OM346: (ts, dead zone) read again
        self.om_bind = []         # v3.46 OM346: (ts, verdict, 'TR TL MF MB')
        self.om_checks = []       # v3.46 OM346: (ts, check, dev ok, dev n, calls ok, calls n, source)
        self.om_counts = [0, 0, 0]  # v3.46 OM346: frames written, of them from the call sites, stopped (the count lines)
        self.om_gate = []         # v3.46 OM346: 'mode / sub, overlay, phase' states where the stick was pushed outside gameplay
        self.om_swap = False      # v3.46 OM346: swapped sticks without GetDeviceState
        self.om_rechecks = []     # v3.46 OM346: (ts, source, ok, n)
        self.om_anom = 0          # v3.46 OM346: its ANOMALY lines
        self.om_row = None        # v3.47 CT347: 'Option 2 row r of n' or why there is none
        self.om_pe = None         # v3.47 CT347: Playable Eileen's answer (the last)
        self.om_bg = [0, 0]       # v3.47 CT347: focus-loss episodes, their failed frames (the lines that give a count)
        self.om_again = [0, 0]    # v3.47 CT347: failures in front that ended: episodes, failed frames
        self.om_old = []          # v3.48 OM348: (ts, the old OmniMove text, written as On / read as On / not written)
        self.ol = None            # v3.49 LK349: [Input] OmniLook at the start
        self.ol_sets = []         # v3.49 LK349: (ts, new, old) read again
        self.ol_bad = []          # v3.49 LK349: unknown OmniLook texts
        self.ol_bind = None       # v3.49 LK349: (ts, verdict, codes) the last look bindings line
        self.ol_check = None      # v3.49 LK349: (ts, ok, n, 'on' / 'none') the last look self-check
        self.ol_places = 0        # v3.49 LK349: count lines
        # v3.49 LK349: summed counts: pushed, analog, own, not here, key, no read, check, bind, latch, latch x, latch y, band, turn kept
        self.ol_n = [0] * 13
        self.ol_room = [0, 0]     # v3.49 LK349: Room 302 count lines, their latch frames
        self.lt = None            # v3.59 CT359: (in / OFF, OmniLatch) at the start
        self.lt_not = []          # v3.59 CT359: the sites that were not the stock code (latch / lock-on walk)
        self.lt_eps = []          # v3.59 CT359: (ts, trigger, seconds, ots, omni, follow, warp, rule, why, deg, dist, off, blend)
        self.lt_rooms = 0         # v3.59 CT359: room summaries
        self.lt_supp = 0          # v3.59 CT359: rig look-rate triggers ignored under the Shoulder camera (room summaries)
        self.lt_over = 0          # v3.59 CT359: episode lines over a room's quota
        self.lt_sets = []         # v3.59 CT359: (ts, OmniLatch) read again
        self.lt_bad = []          # v3.59 CT359: unknown OmniLatch / LockOnWalk texts
        self.lt_fault = []        # v3.59 CT359: the hooks that faulted
        self.lw = None            # v3.59 CT359: (in / OFF, LockOnWalk) at the start
        self.lw_first = None      # v3.59 CT359: (ts, weapon, locked on / no target)
        self.lw_rooms = []        # v3.59 CT359: (frames, locked, weapons)
        self.lw_sets = []         # v3.59 CT359: (ts, new, from)
        self.rz = None            # v3.48 RZ348: (Items, KeepSpecials, Room302, seed, found, scripts, stock, verdict) at the start
        self.rz_roll = None       # v3.48 RZ348: (was, seed, written?) the first start's roll
        self.rz_bad = []          # v3.48 RZ348: (key, text, default) unknown ini values
        self.rz_first = None      # v3.48 RZ348: (ts, stage, room, Items) the first room load
        self.rz_plans = []        # v3.48 RZ348: (ts, stage, visit, mode, pooled, n, changed, seed, candles, medallions, swords, swords stock)
        self.rz_rooms = Counter()  # v3.48 RZ348: 'loads', 'pooled', 'changed', 'other'
        self.rz_lists = 0         # v3.48 RZ348: verbose slot lists
        self.rz_sets = []         # v3.48 RZ348: (ts, key, new, old / from)
        self.rz_sess = None       # v3.48 RZ348: the session line's groups
        self.rz_anom = 0          # v3.48 RZ348: its ANOMALY lines
        self.rz_nomodels = []     # v3.58 MN358: the stages left alone by design (no pickup model files), in order
        self.rz_nomodels_anom = 0  # v3.58 MN358: ... of them written as ANOMALY lines (v3.57 and before: not counted)
        self.vbx = None           # v3.58 MN358: the value box: ('on', bars) / ('off', why)
        self.vbx_pages = OrderedDict()  # v3.58 MN358: page -> (title, rows, values, widest, label, left, right, how, rest)
        self.vbx_grow = []        # v3.58 MN358: (ts, page, row, w, left, right)
        self.rz_v356 = False      # v3.56 RI356: the start line has v3.56's keys (Specials, Reroll)
        self.rz_migs = []         # v3.56 RI356: (ts, old key, old text, new key, new text, how) v3.55's keys migrated
        self.rz_seedline = None   # v3.56 RI356: (seed, x, y, glyphs, size) the seed line under the page's description
        self.wv_worlds = []       # v3.56 RI356: (ts, visit, stage, room, entry, after a reset?) world entries
        self.wv_loads = []        # v3.56 RI356: (ts, stage, room, signal, visit, how, room entries) the loads' rooms
        self.wv_sess = None       # v3.56 RI356: the visits' session line (ints)
        self.rzm = None           # v3.49 RZ349: the Monsters' start line (mode, found, kind-2, scripts, stock, stock kind-2, verdict, targets, save filter)
        self.rzm_bad = None       # v3.49 RZ349: (text, default) an unknown [Randomizer] Monsters value
        self.rzm_gum = None       # v3.49 RZ349: 'may be rolled' / 'are never rolled'
        self.rzm_ct = []          # v3.49 RZ349: (ts, on / off again) the cheat table's Enemy Randomizer
        self.rzm_first = None     # v3.49 RZ349: (ts, stage, room, mode)
        self.rzm_rooms = Counter()  # v3.49 RZ349: 'rooms', 'changed', 'slots', 'sticky', 'drops', 'bones', 'files', 'floor', 'types', 'kb', 'other', 'revisits'; v3.50 RM350: + 'why_lines', 'adopted', 'adopt_changed', 'roll_stock', 'same', 'away', 'other_rec', 'none'
        self.rzm_by = Counter()   # v3.49 RZ349: placements by the type they became
        self.rzm_lists = 0        # v3.49 RZ349: verbose placement lists
        self.rzm_misc = Counter()  # v3.49 RZ349: 'hit', 'offdrop', 'left', 'files', 'save', 'savev'
        self.rzm_silent = []      # v3.49 RZ349: (type, stage) types that may be silent
        self.rzm_sets = []        # v3.49 RZ349: (ts, new, old / from)
        self.rzm_sess = None      # v3.49 RZ349: the session line's groups
        self.rzm_sess3 = None     # v3.50 RM350: the session's adopted records and reasons (adopted, changed, rolled, by record, elsewhere, none)
        self.rzm_anom = 0         # v3.49 RZ349: its ANOMALY lines (not counted in rz_anom)
        self.rz_mig = []          # v3.56 RE356: (old key, its text, new key, value, written)
        self.rzm_story = []       # v3.56 RE356: (stage, what)
        self.ss_rooms = Counter()  # v3.56 RE356: Spawn Site's monster room lines summed
        self.ss_kept = Counter()   # v3.56 RE356: the reasons (kept or refused)
        self.ss_nomap = []        # v3.56 RE356: (stage, room, why)
        self.ss_fn = []           # v3.56 RE356: the functions that were not stock
        self.ss_sess = None       # v3.56 RE356: the session line's groups
        self.ss_anom = 0          # v3.56 RE356: its ANOMALY lines
        self.rzm_ev = None        # v3.51 SN351: (event dogs, stock event dogs, roll again On / Off) at the start
        self.rzm_sess4 = None     # v3.51 SN351: (rolled again, roll again On / Off, event dogs planned, kept asleep) the session
        self.rzm_dead = Counter()  # v3.58 RZ358: 'rooms', 'dead', 'hid', 'killed', 'lost' (room lines), 'v_hid', 'v_lost', 'saves', 'save_dead', 'save_lost'
        self.rzm_cp = Counter()   # v3.58 RZ358: the copies' lines: 'rooms', 'made', 'drawn', 'sited', 'files', 'not', 'files_lines'
        self.rzm_cp_by = Counter()  # v3.58 RZ358: copies made by type
        self.rzm_sess5 = None     # v3.58 RZ358: the dead-stay-dead session line (ints)
        self.rzm_sess6 = None     # v3.58 RZ358: the copies' session line (ints)
        self.cp_recs = Counter()  # v3.58 RZ358: 'rooms', 'not' (copies not made for the enemy records), 'v' (verbose lines)
        self.rzl = None           # v3.49 RZ349L: (EileenOutfit, Ghosts) at the start
        self.rzl_bad = []         # v3.49 RZ349L: (key, text, default) unknown ini values
        self.rzl_first = None     # v3.49 RZ349L: (ts, stage, room, outfit, its state, ghosts, their state)
        self.rzl_out = []         # v3.49 RZ349L: (ts, kind, detail): draw / single / back / takeover
        self.rzl_gh = Counter()   # v3.49 RZ349L: 'rooms', 'victims', 'moved', 'revisits'; 'ct' the cheat table's randomizer
        self.rzl_sets = []        # v3.49 RZ349L: (ts, key, new, old / from)
        self.rzl_sess = None      # v3.49 RZ349L: the looks session line's groups
        self.rzl_anom = 0         # v3.49 RZ349L: its ANOMALY lines (outfit: / ghosts:)
        self.rzl_room = Counter() # v3.50 RL350: 'draws', 'changed', 'held' (the outfit's room draws)
        self.rzl_gv = Counter()   # v3.50 RL350: per victim: 'far' (800 +), 'near', 'recmoved', 'reckept', 'dmax'
        self.rzl_sess2 = None     # v3.50 RL350: the v3.50 looks session line's groups
        self.r3_own = []          # v3.50 R302: (ts, when, look, walk, Playable Eileen, down, up, feel, walk speed, waiting)
        self.r3_hook = None       # v3.50 R302: (ts, when) the look hooks went in
        self.r3_fail = 0          # v3.50 R302: look hook writes rolled back
        self.r3_visits = 0        # v3.50 R302: visit count lines
        self.r3_n = [0] * 8       # v3.50 R302: seconds, look, eased, clamped, up, down, rebuilt, walk ticks
        self.r3_sets = []         # v3.50 R302: (ts, key, old, new, why)
        self.r3_seed = []         # v3.50 R302: (key, value, from) the first start's seed
        self.r3_bad = []          # v3.50 R302: (key, text, used) ini values off the rows
        self.r3_nopage = None     # v3.50 R302: why Option 4 has no Room 302 page
        self.r3_rebuilt = 0       # v3.50 R302: view rebuild lines
        self.r3_anom = 0          # v3.50 R302: its ANOMALY lines
        self.frg = None           # v3.56 FR356: (on / off / not used, the top rate or None, ts) from the start line
        self.frg_downs = []       # v3.56 FR356: (ts, from, to, mean, target, ratio, stage, room, budget level, locked, quick (v3.58 GX358))
        self.frg_ups = []         # v3.56 FR356: (ts, from, to, stage, room)
        self.frg_retry = []       # v3.56 FR356: (ts, minutes, at, higher)
        self.frg_miss = [0, 0]    # v3.56 FR356: missed windows counted, held (the budget shedding)
        self.frg_worst = None     # v3.56 FR356: the worst missed window (ratio, rate, mean, target, ts)
        self.frg_resets = []      # v3.56 FR356: (ts, chosen, held at) / ('vsync', ts, cap, lifted)
        self.t60f = None          # v3.56 FR356: (sites in, of) or ('NOT', va)
        self.cr = None            # v3.59 FR359: (scenes, window, peephole, sites, caves) from the start line, or 'off'
        self.cr_starts = {}       # v3.59 FR359: kind -> phase-1 stretches started at the gameplay rate
        self.cr_ends = []         # v3.59 FR359: (ts, kind, rate, seconds, camera cuts, actor jumps, drift ms)
        self.cr_cuts = [0, 0]     # v3.59 FR359: the [v] cut lines: camera cuts, actors put elsewhere
        self.rg = None            # v3.47 RG347: (in / OFF, Regen, rate, delay, who) from the install line
        self.rg_not = []          # v3.47 RG347: why it is not installed
        self.rg_pe = None         # v3.47 RG347: 'with' / 'without' SH4PE_RegenActive
        self.rg_who = []          # v3.47 RG347: (ts, who, Regen, delay)
        self.rg_sets = []         # v3.47 RG347: (ts, new, old, from)
        self.rg_delay = []        # v3.47 RG347: (ts, seconds) read again
        self.rg_seed = []         # v3.47 RG347: (key, value, from) the first start's seed
        self.rg_runs = Counter()  # v3.47 RG347: healing runs started, by body
        self.rg_stops = Counter()  # v3.47 RG347: runs ended, by reason
        self.rg_gain = 0.0        # v3.47 RG347: HP healed in the ended runs
        self.rg_hurt = 0.0        # v3.47 RG347: HP lost by the hits that ended runs
        self.rg_open = 0          # v3.47 RG347: a run started and not ended (yet)
        self.rg_anom = 0          # v3.47 RG347: its ANOMALY lines
        self.ls_route = []        # v3.47 RG347: (ts, 'game' / 'direct') Life Steal's heal route
        self.pool_n = [None, None]  # v3.36 TP336: task slots, IK handles (from the install lines)
        self.pool_rooms = []      # (stage:room, ts, tasks used, of, free, high, ik used or None, ik free, ik of, actors, heap)
        self.drops = Counter()    # v3.37 MN337: given, faded, made way, refused by the room cap / lying cap / records, renumbered
        self.drop_caps = None     # (anywhere, a room, kept free) from the install line
        self.drop_lying = 0       # the most lying a refusal line names
        self.drop_level = None    # v3.38 DR338: the level at the install line
        self.drop_changes = []    # (ts, level) of the menu changes and ini re-reads
        self.drop_alias = 0       # v3.37's Generous read as Abundant
        self.drop_chances = None  # v3.38 DR338c: (level factors {name: x}, [(class, c, u, r) on Normal], any %) from the levels line
        self.drop_equal = False   # v3.38 DR338c: the shares ('common 1:1:1, uncommon 2:1, rare 1:1:1:1') from the shares line
        self.drop_items = Counter()  # drops given, by item name
        self.perf = []            # v3.37 CP337: (ts, why, secs, frames, fps, lowest low, worst, refined, va used, va of, va most)
        self.backend = None       # v3.37 CP337: the Direct3D chain
        self.rend = None          # v3.39 PR339: (why, row greyed / open, state) at the start
        self.rend_sw = []         # v3.39 PR339: (ts, old label, new label) replaces
        self.rend_fail = []       # v3.39 PR339: (ts, label, reason) refusals
        self.opening = None       # v3.39 PR339: the level at the start
        self.open_ms = None       # v3.39 PR339: the boot screens skipped this many ms after the start
        self.open_saved = []      # v3.39 PR339: (ts, level) saved for the next start
        self.cam_page = [0, None]  # v3.37 CP337: pause Camera page openings, the settings at its last close
        self.mu_set = None        # v3.51 MU351: (ts, 'paused' / 'v3.50', PauseSound) from the install line
        self.mu = Counter()       # v3.51 MU351: (list, event short name) -> lines; (list, 'paused') -> opened from the pause, paused
        self.mu_loud = []         # v3.51 MU351: (ts, list, event, verdict) the music not paused while the list stayed in the pause
        self.mu_anom = 0          # v3.51 MU351: its ANOMALY lines
        self.ql_set = None        # v3.52 QL352: (ts, 'pause' / 'v3.51', QuickLoadInPause, why off) from the install line
        self.ql = Counter()       # v3.52 QL352: quick loads by path ('pause', 'key', 'closed', 'unknown'), 'late', 'loud', 'back', 'notrun', 'drop', 'refused'
        self.ql_black = []        # v3.52 QL352: frames to black of each quick load inside the pause
        self.ql_ms = []           # v3.52 QL352: ms from the restart to the loaded room running (the music back)
        self.ql_closed = Counter()  # v3.52 QL352: the v3.51 fallbacks by their reason
        self.ql_anom = 0          # v3.52 QL352: its ANOMALY lines
        self.pv_set = None        # v3.56 PV356: (ts, scene / picture, PreviewLayers) from the install line
        self.pv = Counter()       # v3.56 PV356: kept, heat, spirit, made, kept_t, notkept, fail; page shows by kind
        self.pv_max = [0, 0, 0, 0.0]  # v3.56 PV356: the most bodies, ghosts, soul fires in a pause's layers; the slowest capture (ms)
        self.pv_why = Counter()   # v3.56 PV356: why a pause kept no layers
        self.pv_page = Counter()  # v3.56 PV356: (filter, what) the page showed
        self.pv_anom = 0          # v3.56 PV356: its ANOMALY lines
        self.pv358 = None         # v3.58 PV358: (ts, Preview, the stock file present / missing, PreviewLayers, Filter Strength) from the install line
        self.tv = None            # v3.59 TV359: ('in', '') / ('OFF', why) from the start's walls-hide-heat line
        self.tv_rooms = []        # v3.59 TV359: (stage:room, heat frames, grid frames, kept, why, prints depth-tested)
        self.tv_first = None      # v3.59 TV359: (ts, stage:room) of the first frame whose depth the grid kept
        self.tv_shapes = []       # v3.59 TV359: (ts, shape, why) the player's footprint shape at each change
        self.pv_mig = []          # v3.58 PV358: the migrations: 'PreviewScene=X -> Preview=Y', 'FilterStrength=X (v3.15) -> High'
        self.pv_off = 0           # v3.58 PV358: page openings with Preview Off
        self.pv_stockbad = Counter()  # v3.58 PV358: the stock picture missing / not usable lines
        self.pv_fsfail = 0        # v3.58 PV358: plain copies that failed (Filter Strength Low / Medium)
        self.pv_f7 = Counter()    # v3.59 PV359: F7 pictures: 'both' / 'heat' / 'spirit' / 'none' (layers saved), 'late', 'fail'
        self.pv_f7why = Counter()  # v3.59 PV359: why a layer was not saved
        self.pv_lay = Counter()   # v3.59 PV359: (kind, verdict) of the picture layers the page loaded
        self.pv_shstock = Counter()  # v3.59 PV359: the Shadows page with Preview Stock: what it showed
        self.pv_stocklay = None   # v3.59 PV359: the stock picture's layer files at the start
        self.snd_in = None        # v3.52 SND352: (pan, volume, reverb, mono streams, mono music) from the install line, or 'off'
        self.snd_start = None     # v3.52 SND352: the first frame's start line (ts, groups)
        self.snd_first = None     # v3.52 SND352: (ts, mode, the save's byte) of the first start's migration, or a bad Mode
        self.snd_diff = None      # v3.52 SND352: (ts, byte, mode): the save's Sound differed from the ini at the start
        self.snd_changes = []     # v3.52 SND352: (ts, what, from, to, why)
        self.snd_min = []         # v3.52 SND352: the minute lines' numbers
        self.snd_fix = Counter()  # v3.52 SND352: 'byte' / 'engine' / 'discard'
        self.snd_notin = []       # v3.52 SND352: sites not installed (what, va)
        self.snd_anom = 0         # v3.52 SND352: its ANOMALY lines
        self.sd_bg = None         # v3.56 SD356: (Background, sites, keep-awake, device watch) from the install line
        self.sd_keep = None       # v3.56 SD356: (ts, dBFS, level), or 'none' / 'FAILED'
        self.sd_keep_re = 0       # v3.56 SD356: keep-awake restarts / status failures
        self.sd_focus = []        # v3.56 SD356: (ts, 'away' / 'back', seconds before the change)
        self.sd_focus_cap = False
        self.sd_dev = None        # v3.56 SD356: (ts, the game's device, how) or (ts, name, 'NOT REGISTERED hr ...')
        self.sd_dev_ev = []       # v3.56 SD356: (ts, 'new' / 'back' / the game device's state, name)
        self.sd_dev_off = None    # v3.56 SD356: why there is no device watch
        self.sd_cfg = Counter()   # v3.56 SD356: ini values read as the default
        self.sd_zero = None       # v3.58 SD358: ('silent', 2D, streams, 0x2774) / ('off',) / ('NOT IN',) from the install line
        self.sd_zero_n = [0, 0, 0]  # v3.58 SD358: volumes silenced (2D, 3D, streams) from the minute lines
        self.sd_sess = None       # v3.58 SD358: ('session', n, devices) / ('none', why) from the watch thread's start
        self.sd_mutes = Counter()  # v3.58 SD358: 'mute' / 'more' / 'unmute' requests done, 'sessions' muted, 'fixed' / 'user' at the start
        self.sd_sites = []        # v3.58 SD358: (ts, 'Play' / 'Mute' / 'FAILED') the buffer sites followed the session
        self.sd_stale = []        # v3.58 SD358: (ts, n) sounds with the global focus behind other windows (Mute without a session)
        self.sd_exit = None       # v3.58 SD358: (ts, 'done' / 'no answer') the window closed while muted
        self.sd_keep_hold = 0     # v3.58 SD358: the keep-awake tone stopped behind other windows
        self.sd_sess_lost = []    # v3.58 SD358: (ts, what) ANOMALY: sessions that could not be (un)muted, the session lost
        self.sd_guard = None      # v3.59 SD359: (music, streams, Play2D / Play3D) from the install line, or 'off'
        self.sd_guard_first = None  # v3.59 SD359: the first frame's Play2D / Play3D decision
        self.sd_gone = None       # v3.59 SD359: (ts, 'failed' / 'faulted', where, hr) the first stand-in buffer
        self.sd_gone_n = None     # v3.59 SD359: (ts, all, 2D, 3D, music, streams) the last count line
        self.sd_retry = 0         # v3.59 SD359: a failed create that the second try made
        self.fw_in = None         # v3.59 SD359: the fault watch: 'in' / 'NOT IN' / 'off'
        self.fw = []              # v3.59 SD359: (ts, code, name, module, offset, thread, kind, guard)
        self.fw_cap = False
        self.fw_end = None        # v3.59 SD359: the session end's sums
        self.fw_stop = None       # v3.59 SD359: (ts, the last fault, 'stopped' / 'logged only')
        self.ol_set = None        # v3.53 OL2: 'in' / 'off' from the Options look's install line
        self.ol_pages = OrderedDict()  # v3.53 OL2: page -> [name, the largest estimate, measured with it, ts, lines]
        self.ol_inert = Counter()  # v3.53 OL2: keys on a grey row, by row
        self.ol_over = 0          # v3.53 OL2: pages over the 800 glyphs (its ANOMALY lines)
        self.ly_in = None         # v3.54 OL34: the layout's install line: (ts, cycle, pages, placed, folders, left out, texts, of, over)
        self.ly_off = False       # v3.54 OL34: no Options pages
        self.ly_miss = []         # v3.54 OL34: (page, title, row, why) rows left out
        self.ly_open = []         # v3.54 OL34: (ts, cycle, pages text) at each Options opening
        self.ly_moves = None      # v3.55 OL56: 'on' / 'off' (None: a log before v3.55), and why off
        self.ly_moves_why = ''
        self.ly_vb = None         # v3.55 OL56: 0x525672: 'in', 'bytes <hex>' (another mod), 'failed' 
        self.mn_vl = None         # v3.56 MN356: (rows, lines, in the plan, over) from the value lines' build line
        self.mn_vl_ans = []       # v3.56 MN356: (ts, page, row, line) a value answered with the plan's line (first a row)
        self.mn_vl_miss = []      # v3.56 MN356: (page, row, value, word) a value with no line (a number or a range)
        self.mn_fc = None         # v3.56 MN356: (mode, FolderClick, note) the folder click's line
        self.mn_fc_open = []      # v3.56 MN356: (ts, page, title, row, what) a click that opened a row
        self.mn_fc_refuse = []    # v3.56 MN356: (ts, page, row, why) a click not taken (mid-slide / fade)
        self.mn_em_text = None    # v3.56 MN356: ('once' / 'stock', StockShadow) the Extra Menu's text
        self.hb_hide = []         # v3.38 HB338: (ts, 'on' / 'off') Hide Gauge as the curse meter saw it
        self.hb_ring = Counter()  # v3.38 HB338: 'faded out' / 'shown: <why>' counts (both rings)
        self.hb_pe_ring = None    # v3.41 CM341: (ts, 'pe' / 'refined', why) who draws the player's curse ring
        self.hb_ref = Counter()   # v3.52 HB352: 'hidden' / 'place' -> beings, from the room lines; 'rooms' -> room lines
        self.hb_ref_lines = Counter()  # v3.52 HB352: the per-being lines by reason (bounded: 60 a session)
        self.hb_ref_first = None  # v3.52 HB352: (ts, id, type, room) of the first per-being line
        self.hb_ref_tot = (0, 0)  # v3.52 HB352: the last room line's session totals (hidden, place)
        self.en358 = Counter()  # v3.58 EN358: g_doors (door lines with ghosts), g_door (ghosts at doors), g_rooms, g_tag, g_in, g_arr,
        #                         g_out, g_first, g_drawn, hb_undrawn, hb_dog, w_down, w_stumble, w_gone, w_other
        self.en358_near = None  # v3.58 EN358: the nearest a followed ghost came (units)
        self.en358_wsite = None
        self.en359 = Counter()     # v3.59 EN359: loads, back, saved, kr_back, kr_all, kr_saves, kr_kept, pins, anchors, anchors_fb, edges, w_heal
        self.en359_pin_last = None
        self.en359_pinsite = None
        self.en359_wlong = None
        self.hb_boss = []         # v3.52 HB352: (ts, name, room, seconds or None, why or None)
        self.ft_set = None        # v3.59 FT359: (ts, 'on' / 'off') [ScreenEffects] FilterThemes
        self.ft_look = Counter()  # v3.59 FT359: the looks the enemy bars wore (a line per change)
        self.ft_hid = [0, 0, 0, None]  # v3.59 FT359: victims' bars hidden by room lines, room lines, the last session total, (ts, type, room) of the first
        self.perf_place = OrderedDict()  # v3.38 HB338: 'WxH' -> (where, of what, ts)
        self.fol = Counter()      # v3.38 EN338: doors, follow, most, wait, stopped, came, spotted, stops, story, wheelchairs
        self.fol_wait = Counter()  # v3.38 EN338: the space reasons of the waits
        self.fol_mode = None      # v3.49 EF349: Enemies Follow's value at the start (the enemies install line)
        self.fol_modes = []       # v3.49 EF349: (ts, value) each change (menu / ini read again)
        self.fol_home = [0, 0]    # v3.49 EF349: monsters sent home from other rooms, copies made in their own rooms again
        # v3.50 EF350: followers across saves and Enemy Pursuit
        self.ef = Counter()       # saves, saves_unwritten, s_pur, s_here, s_q, s_tag, s_cw, found, none, restores, r_pur, r_here,
                                  # r_q, r_tag, r_cw, r_norec, r_kind, r_no, lost, sight, down, behind, forgot, lost_doors
        self.ef_mode = None       # Enemy Pursuit at the start (the install line), (None: not in the log)
        self.ef_site = None       # 'in' / 'off' / 'NOT in'
        self.ef_modes = []        # (ts, value) each change
        # v3.56 ER356: Enemies Return and the world visit
        self.er_mode = None       # Enemies Return at the start (the enemies install line; None: not in the log)
        self.er_tsite = None      # 'in' / 'off' (the Tremer site, from the install line / its own line)
        self.er_modes = []        # (ts, value) each change (the menu, the ini read again)
        self.er = Counter()       # migr, migr_fail, rooms, r_bodies, r_bits, r_away, r_copies, worlds, w_back, w_dogs, w_story,
                                  # loadnone, tremer (the last count), entries, loads
        self.ep = Counter()       # v3.51 EP351: in-room give-ups: rooms, give, mon, ghost, again, down, held, stock; v_* the verbose
                                  # lines (v_give, v_unsensed, v_downed, v_far, v_untouched, v_<family>)
        self.ep_site = None       # v3.51 EP351: 'in' / 'off' / 'NOT in'
        self.sh = Counter()       # v3.40 SH340: composites, soft, frames, kept, scene / figure previews, rooms
        self.sh_edge = []         # v3.40 SH340: (ts, edge) at the start and each change
        self.sh_plans = OrderedDict()  # 'Soft 2560x1440' -> (passes, low size, taps, px)
        self.sh_worst = [0, 0.0, 0.0, '']  # most composites in a frame, the worst ms, the average ms of that room, its room
        self.sh_off = None        # (ts, why, where)
        self.sh_rooms = {}        # v3.52 SH352: room -> [composites, soft, hard, no darkness, no reason given, Counter(hard reasons)]
        self.sh_sw = []           # v3.58 GX358: (ts, 'On' / 'Off', 'stage:room kept|none' or '')
        self.sh_off_rooms = {}    # v3.58 GX358: room -> composites the game drew with Shadows Off
        self.sh_pv_off = 0        # v3.58 GX358: the Shadows page's preview showed the game's Off
        self.sh_ns = []           # v3.52 SH352: the once-a-room reason lines (ts, room, why)
        self.sh_not = None
        self.sh_vram = 0.0
        self.sh_rib_state = ''    # v3.49 SH349: the Tremers' wall shadows: 'the Darkness' / "the game's own (why)"
        self.sh_rib_vals = OrderedDict()  # v3.49 SH349: 'AA -> BB at P %' -> the share of the game's darkening kept (%)
        self.sh_copy = [0, 0, 0.0]  # v3.41 SH341: pause copies with their targets made / kept, the slowest copy (ms)
        self.sh_old = None        # v3.41 SH341: v3.40's Edge value read as Soft
        self.dof = Counter()      # v3.43 DF343: drawn frames, asked frames, room lines, pause copies
        self.dof_set = []         # v3.43 DF343: (ts, 'Mode Strength') at the start and each change
        self.dof_worst = [0.0, 0.0, '']  # v3.43 DF343: the worst ms, that room's average, its room
        self.dof_probe = Counter()  # v3.43 DF343: probe verdicts
        self.dof_skip = []        # v3.43 DF343: rooms the probe turned it off in
        self.dof_off = None       # v3.43 DF343: (ts, why)
        self.dof_not = None
        self.dof_mig = None       # v3.45 QS345: (old value, new, written?) of the one-time DofStrength migration
        self.dof_why = Counter()  # v3.44 DF344: frames not drawn, by reason (the room lines)
        self.dof_design = Counter()  # v3.44 DF344: frames skipped by design (paused, Cut-scenes in play, a view, Room 302)
        self.dof_ramps = None     # v3.47 DF347: 'Low 4+4, Medium 8+4, High 12+8' (the install's verbose line)
        self.dof_first = None     # v3.47 DF347: 'eased in' / 'whole levels' at the first drawn frame
        self.dof_noblend = 0      # v3.47 DF347: "the blend shader could not be made" lines (a device each)
        self.dof_close = 0        # v3.52 FX352: frames drawn with the player close to the camera (the room lines)
        self.dof_close_rooms = []  # v3.52 FX352: 'stage:room' of each room's first close shot
        self.dof_note59 = None    # v3.59 DF359: (the ini's value, its strength) kept and noted as a bit gentler
        self.dof_spirit = None    # v3.59 DF359: the time depth of field first kept out of Spirit Vision
        self.dof_spirit_keep = False  # v3.59 DF359: [ScreenEffects] DofSpirit=1 (blurred under Spirit Vision, v3.58's order)
        self.fx_pic = [0, 0, 0.0, 0, 0.0]  # v3.41 SH341: pause pictures on the GPU (made, kept, most ms), read backs (n, worst ms)
        self.started = None       # v3.40 LG340: datetime of the "log: started" line
        self.d3d_head = None      # v3.40 LG340: 'dgvoodoo' / 'none' from the session header's d3d9.dll line
        self.vw = None            # v3.41 VW341: the window views' install line
        self.vw_views = []        # v3.41 VW341: (ts, 'L | R at WxH') each window view with black sides
        self.edges = []           # v3.41 VW341: (ts, windows, what, 'in front' / 'not in front') the Borderless edge covers
        self.edges_free = 0       # v3.41 VW341: "nothing covers them now"
        self.e2d = []             # v3.42 EG342: ('WxH', 'aligned' / 'stock') each size's 2D edge decision
        self.e2d_sides = {}       # v3.42 EG342: 'WxH' -> 'L | R' the black sides' widths there
        self.pp = None            # v3.43 PP343: (reset hook, bloom, grading, lens) at the start
        self.pp_ini_off = False   # v3.43 PP343: [ScreenEffects] PostEffects=0
        self.pp_set = []          # v3.43 PP343: (ts, row, value) the page's changes
        self.pp_fail = []         # v3.43 PP343: (ts, effect, why)
        self.pp_lens = Counter()  # v3.43 PP343: 'in' / 'out' fades
        self.pp_lens_max = [0.0, '']  # v3.43 PP343: the strongest lens target (%) and its stage:room
        self.pp_chain = OrderedDict()  # v3.43 PP343: 'WxH' -> MB of the bloom targets
        self.pp_notf = Counter()  # v3.43 PP343: 'colour grading under Thermal' ...
        self.pp_drawn = Counter()  # v3.43 PP343: 'bloom Low' ... (the first draw of each state)
        self.pp_mig = None        # v3.52 FX352: (old value, new, written?) of the one-time Bloom migration
        self.pp_lens_rooms = []   # v3.52 FX352: (stage:room, shown s, of s, max strength %) the lens' room lines
        self.fn = {'set': None, 'live': False, 'match': set(), 'nomatch': set(), 'stock': None,
                   'row': False, 'switches': []}  # v3.43 FN343; v3.44 FN344: + row, switches
        self.sb_row = None  # v3.47 SB347: (label, Menu Font, width) of the Bank row's first draw
        self.sb_clicks = Counter()  # v3.47 SB347: 'left arrow' / 'right arrow' / 'Bank row' -> mouse clicks
        self.sb_stuck = 0  # v3.47 SB347: clicks that did not change the bank (bank 1 / the last bank, a refused file)
        self.cur_lv = 'n'  # v3.48 SB348: the level of the line parse() sees ('n', 'v', 't')
        self.sb_steps = Counter()  # v3.48 SB348: 'save banks: bank N' lines by level ('n' = v3.47's noise, or Option 6)
        self.sb_changes = []  # v3.48 SB348: (ts, from, to, list, steps, keys, pad, mouse, by10) settled changes
        self.sb_bar = None  # v3.49 SB349: ('blue' / 'yellow', why) of the lists' highlight bar

    def parse_fadein(self, ts, msg):  # v3.50 FI350: True = a Fade In line read
        t = FI_ROOM_TAR.search(msg) if msg.startswith('fade in: room ') else None  # v3.51 PT351: the portal part
        if t:
            msg = msg[:t.start()] + msg[t.end():]
        m = FI_ROOM.match(msg)
        if m and t:
            g = t.groups()
            for k, name in enumerate(('game', 'Refined', 'vortex')):
                if int(g[k]):
                    self.fi_by[name] += int(g[k])
            self.fi_held += int(g[3])
            if float(g[4]) > 0 or float(g[5]) > 0:
                self.fi_cost[0] += 1
                self.fi_cost[1] = max(self.fi_cost[1], float(g[4]))
                self.fi_cost[2] = max(self.fi_cost[2], float(g[5]))
        if m:
            g = [int(x) for x in m.groups()]
            self.fi_rooms += 1
            for k, name in enumerate(FI_SRC):
                if g[3 + k]:
                    self.fi_src[name] += g[3 + k]
            self.fi_portals += g[10]
            for k, name in enumerate(FI_SKIP):
                if g[12 + k]:
                    self.fi_skip[name] += g[12 + k]
            return True
        m = FI_START.match(msg)
        if m:
            self.fi_starts[m.group(1)] += 1
            if m.group(6):  # v3.51 PT351
                self.fi_tar_starts['%s, %s' % ('game' if m.group(6).startswith('the game') else 'Refined', m.group(7))] += 1
            return True
        m = FI_IN.match(msg)
        if m:
            self.fi = ('in', 'scene pass' if m.group(1).startswith('from') else 'NOT available', m.group(3), m.group(4))
            if m.group(2):  # v3.51 PT351
                self.fi_tar = (m.group(2), m.group(5) or '?', m.group(6) or '?')
            if m.group(7):  # v3.58 FI358
                self.fi_big = (m.group(7), m.group(8), m.group(9))
            m59 = FI_IN359.search(msg)  # v3.59 FI359
            if m59:
                self.fi59 = (m59.group(1), m59.group(2))
            return True
        m = FI_ARRIVE.match(msg)  # v3.59 FI359
        if m:
            g = m.groups()
            self.fi_arrive.append((g[0], g[1], int(g[2]) if g[2] is not None else None, int(g[3] or 0), int(g[5] or 100),
                                   int(g[6]) if g[6] is not None else None, int(g[7]) if g[7] is not None else None,
                                   int(g[8] or 0), g[9], int(g[10]), int(g[11]), float(g[12] or 0)))
            return True
        m = FI_WAITCAP.match(msg)
        if m:
            self.fi_waitcap += 1
            return True
        m = FI_SPIRIT.match(msg)
        if m:
            self.fi_spirit.add('wall portal' if m.group(1).startswith('the game') else 'tar portal')
            return True
        m = FI_CLOSE.match(msg)  # v3.58 FI358
        if m:
            g = m.groups()
            self.fi_close.append((g[0], float(g[2]), int(g[4]), int(g[5]), int(g[6]), int(g[8]), int(g[9]), int(g[10]),
                                  float(g[11]), g[12], int(g[13]), int(g[14]), g[15], float(g[16]), float(g[17]), int(g[18]),
                                  int(g[19]), float(g[20]), float(g[21])))
            return True
        m = FI_STICK_BAD.match(msg)
        if m:
            self.fi_stick_bad = m.group(1)
            return True
        m = FI_LETGO.match(msg)  # v3.51 PT351
        if m:
            g = m.groups()
            self.fi_letgo.append((g[0], g[2], float(g[3]), g[4], g[5], int(g[6]), int(g[7]), int(g[8]), float(g[9]),
                                  float(g[10]), float(g[11]), int(g[12])))
            return True
        m = FI_REAL.match(msg)
        if m:
            self.fi_real += 1
            return True
        m = FI_NEVER.match(msg)
        if m:
            self.fi_never += 1
            return True
        m = FI_GFAULT.match(msg)
        if m:
            self.fi_gfault = (ts, m.group(1))
            return True
        m = FI_NOTSTOCK.match(msg)
        if m:
            self.fi_notstock.append('%s (%s)' % m.groups())
            return True
        m = FI_TDM_BAD.match(msg)
        if m:
            self.fi_tdm_bad = m.group(1)
            return True
        m = FI_OFF.match(msg)
        if m:
            self.fi = ('OFF', m.group(1))
            return False  # (a "not the stock code" line: the warnings group keeps it too)
        m = FI_REREAD.match(msg)
        if m:
            self.fi_sets.append((ts, m.group(1), '[Gameplay] read again'))
            return True
        m = FI_SET.match(msg)
        if m:
            self.fi_sets.append((ts, m.group(1), 'NOT saved' if m.group(2) else 'menu'))
            return True
        m = FI_BAD.match(msg)
        if m:
            self.fi_bad = m.groups()
            return True
        m = FI_FAULT.match(msg)
        if m:
            self.fi_fault = (ts, m.group(1), m.group(2))
            return True
        m = FI_DRAWN.match(msg)
        if m:
            if self.fi_drawn is None:
                self.fi_drawn = (ts, m.group(2))
            if m.group(1) and not self.fi_tar_drawn:  # v3.51 PT351: the first tar portal drawn
                self.fi_tar_drawn = (ts, 'the game\'s own draw' if m.group(1).startswith('the game') else 'Refined\'s draw')
            return True
        m = FI_OWN.match(msg)
        if m:
            self.fi_own.add('type %s (pre-draw %s)' % m.groups())
            return True
        return False

    def parse_pv(self, ts, msg):  # v3.56 PV356: True = an Effects preview line read
        m = PV_SET358.match(msg)  # v3.58 PV358
        if m:
            self.pv358 = (ts, m.group(1), m.group(3), m.group(5), m.group(6))
            self.pv_set = (ts, 'preview ' + m.group(1).lower(), m.group(5))
            m2 = PV_STOCKLAY.search(msg)  # v3.59 PV359
            self.pv_stocklay = m2.group(1).replace('with its ', '') if m2 else None
            return True
        m = PV_F7.match(msg)  # v3.59 PV359
        if m:
            heat, spirit = m.group(2) == 'saved', m.group(4) == 'saved'
            self.pv_f7['both' if heat and spirit else 'heat' if heat else 'spirit' if spirit else 'none'] += 1
            for ok, note in ((heat, m.group(3)), (spirit, m.group(5))):
                if not ok:
                    self.pv_f7why[note.strip('()')] += 1
            return True
        if PV_F7LATE.match(msg):
            self.pv_f7['late'] += 1
            return True
        if PV_F7FAIL.match(msg):
            self.pv_f7['fail'] += 1
            return True
        m = PV_LAY.match(msg)
        if m:
            self.pv_lay[(m.group(1), m.group(4) or m.group(3))] += 1
            return True
        m = PV_SHSTOCK.match(msg)
        if m:
            self.pv_shstock['%s: %s' % (m.group(1), m.group(2))] += 1
            return True
        m = PV_OFF.match(msg)
        if m:
            self.pv_off += 1
            self.pv_page[('(off)', 'Preview Off')] += 1
            return True
        m = PV_MIG.match(msg)
        if m:
            self.pv_mig.append('PreviewScene=%s -> %s' % (m.group(1), 'Preview=' + m.group(2) if m.group(2) else 'removed (Preview decides)'))
            return True
        m = PV_FSLEG.match(msg)
        if m:
            self.pv_mig.append("FilterStrength=%s (v3.15's) -> High" % m.group(1))
            return True
        m = PV_STOCKBAD.match(msg)
        if m:
            self.pv_stockbad[m.group(2)] += 1
            return True
        if PV_FSFAIL.match(msg):
            self.pv_fsfail += 1
            return True
        m = PV_SET.match(msg)
        if m:
            self.pv_set = (ts, 'scene' if m.group(1).startswith('SH4') else 'picture', m.group(4))
            return True
        m = PV_KEPT.match(msg)
        if m:
            g = m.groups()
            self.pv['kept'] += 1
            self.pv['heat'] += g[0] == 'on'
            self.pv['spirit'] += g[3] == 'on'
            self.pv['made' if g[9] == 'targets made' else 'kept_t'] += 1
            self.pv_max = [max(self.pv_max[0], int(g[1])), max(self.pv_max[1], int(g[4])), max(self.pv_max[2], int(g[5])),
                           max(self.pv_max[3], float(g[11]))]
            return True
        m = PV_NOT.match(msg)
        if m:
            self.pv['notkept'] += 1
            self.pv_why[m.group(1)] += 1
            return True
        if PV_FAIL.match(msg):
            self.pv['fail'] += 1
            return True
        m = PV_PAGE.match(msg)
        if m:
            back = m.group(2)
            what = m.group(3) or ''
            kind = ('the scene' if back.startswith('SH4') else 'the picture' if back.startswith('the preview picture') else
                    'the stock picture' if back.startswith('the stock picture') else  # v3.58 PV358
                    'the hallway' if back.startswith('the stock') else 'a pause')
            if what.startswith("the pause's heat layer"):
                kind += " + its heat layer"
            elif what.startswith("the pause's spirit layer"):
                kind += " + its spirit layer"
            elif what.startswith("the picture's heat layer"):  # v3.59 PV359: the F7 set's files
                kind += " + its heat file"
            elif what.startswith("the picture's spirit layer"):
                kind += " + its spirit file"
            elif what:
                kind += ' + ' + what.split(' (')[0]
            if m.group(4):
                kind += ' (NOT drawn)'
            if m.group(5):  # v3.58 PV358
                kind += ' at ' + m.group(5)
            self.pv_page[(m.group(1), kind)] += 1
            return True
        if PV_LAID.match(msg):
            self.pv['laid'] += 1
            return True
        return False

    def parse_tv(self, ts, msg):  # v3.59 TV359: True = a Thermal Vision line read
        if TV_IN.match(msg):
            self.tv = ('in', '')
            return True
        m = TV_OFF.match(msg)
        if m:
            g = m.group(1)
            self.tv = ('OFF', 'ThermalTrueDepth=0' if g.startswith('[') else '004EFBDC not stock' if 'not stock' in g else
                       '004EFBDC not written')
            return True
        m = TV_ROOM.match(msg)
        if m:
            g = m.groups()
            self.tv_rooms.append(('%s:%s' % (g[0], g[1]), int(g[2]), int(g[3]), int(g[4]), g[5], int(g[6])))
            return True
        m = TV_FIRST.match(msg)
        if m:
            self.tv_first = (ts, '%s:%s' % m.groups())
            return True
        m = TV_SHAPE.match(msg)
        if m:
            self.tv_shapes.append((ts, m.group(1), m.group(2)))
            return True
        return False

    def tv_line(self):  # v3.59 TV359: None when the log has none of its lines
        if not self.tv and not self.tv_rooms and not self.tv_shapes and not self.tv_first:
            return None
        line = 'Thermal Vision (v3.59): walls hide heat %s' % (
            'in' if self.tv and self.tv[0] == 'in' else 'OFF (%s)' % self.tv[1] if self.tv else '(no start line)')
        if self.tv_rooms:
            fr = sum(r[1] for r in self.tv_rooms)
            ran = sum(r[2] for r in self.tv_rooms)
            kept = sum(r[3] for r in self.tv_rooms)
            pz = sum(r[5] for r in self.tv_rooms)
            seen = [r[0] for r in self.tv_rooms if r[4] == 'HEAT SEEN THROUGH WALLS']
            some = [r[0] for r in self.tv_rooms if r[4] == 'walls hid the heat in some frames']
            line += '; %d room visit(s), %d heat frame(s): the light grid drew in %d, the depth kept in %d; footprints ' \
                    'depth-tested in %d' % (len(self.tv_rooms), fr, ran, kept, pz)
            if seen:
                line += '; HEAT SEEN THROUGH WALLS in %d room visit(s): %s' % (len(seen), ', '.join(seen[:6]))
            if some:
                line += '; kept in some frames only in %s' % ', '.join(some[:6])
        if self.tv_first:
            line += '; first kept frame %s in %s' % self.tv_first
        if self.tv_shapes:
            c = Counter(x[1] for x in self.tv_shapes)
            line += "; the player's footprints: %s (last: %s, %s)" % (
                ', '.join('%s x%d' % kv for kv in c.most_common()), self.tv_shapes[-1][1], self.tv_shapes[-1][2])
        return line

    def pv_line(self):  # v3.56 PV356: None when the log has none of its lines
        if not self.pv_set and not self.pv and not self.pv_page and not self.pv_anom and not self.pv_f7 and not self.pv_shstock:  # v3.59 PV359
            return None
        if self.pv358:  # v3.58 PV358
            line = 'effects preview: Preview %s (the stock picture %s), Filter Strength %s; the pause layers %s at the start' % (
                self.pv358[1], self.pv358[2], self.pv358[4], 'on' if self.pv358[3] == '1' else 'off (PreviewLayers=0)')
            if self.pv_mig:
                line += '; migrated: ' + ', '.join(self.pv_mig)
            if self.pv_stockbad:
                line += '; the stock picture %s' % ', '.join('%s x%d' % kv for kv in self.pv_stockbad.most_common())
            if self.pv_fsfail:
                line += '; %d plain cop(y/ies) failed' % self.pv_fsfail
            if self.pv_stocklay:  # v3.59 PV359
                line += ' (%s)' % self.pv_stocklay
        elif self.pv_set:
            line = 'effects preview: with no paused game %s; the pause layers %s at the start' % (
                "SH4 Refined's scene" if self.pv_set[1] == 'scene' else 'PreviewImage first (Picture)',
                'on' if self.pv_set[2] == '1' else 'off (PreviewLayers=0)')
        else:
            line = 'effects preview: (no start line)'
        if self.pv['kept'] or self.pv['notkept']:
            line += '; %d pause(s) kept layers (heat in %d, at most %d bodies; spirit in %d, at most %d ghost(s), %d soul fire(s); ' \
                    'targets made %d, kept %d; the slowest %.2f ms)' % (
                        self.pv['kept'], self.pv['heat'], self.pv_max[0], self.pv['spirit'], self.pv_max[1], self.pv_max[2],
                        self.pv['made'], self.pv['kept_t'], self.pv_max[3])
            if self.pv['notkept']:
                line += ', %d not: %s' % (self.pv['notkept'], ', '.join('%s x%d' % (k, v) for k, v in self.pv_why.most_common(3)))
        if self.pv_page:
            line += '; the page showed: ' + ', '.join('%s over %s x%d' % (f, k, n) for (f, k), n in self.pv_page.most_common(8))
        if self.pv['fail']:
            line += '; %d failure(s)' % self.pv['fail']
        if self.pv_f7:  # v3.59 PV359
            saved = sum(self.pv_f7[k] for k in ('both', 'heat', 'spirit', 'none'))
            line += '; F7: %d picture(s) (layers: both %d, heat only %d, spirit only %d, none %d)' % (
                saved, self.pv_f7['both'], self.pv_f7['heat'], self.pv_f7['spirit'], self.pv_f7['none'])
            if self.pv_f7why:
                line += ' [not saved: %s]' % ', '.join('%s x%d' % kv for kv in self.pv_f7why.most_common(3))
            if self.pv_f7['late']:
                line += ', %d press(es) dropped (left play)' % self.pv_f7['late']
            if self.pv_f7['fail']:
                line += ', %d layer failure(s)' % self.pv_f7['fail']
        if self.pv_lay:
            line += '; picture layers: ' + ', '.join('%s %s x%d' % (k, v, n) for (k, v), n in self.pv_lay.most_common(4))
        if self.pv_shstock:
            line += '; Shadows page, a still picture alone: ' + ', '.join('%s x%d' % kv for kv in self.pv_shstock.most_common(4))
        if self.pv_anom:
            line += '; %d ANOMALY line(s)' % self.pv_anom
        return line

    def parse_mu(self, ts, msg):  # v3.51 MU351: True = a sound line of the Extra Menu's lists read
        m = MU_SET.match(msg)
        if m:
            self.mu_set = (ts, 'paused' if m.group(1).startswith('paused') else 'v3.50', m.group(2))
            return True
        m = MU_LINE.match(msg)
        if not m:
            return False
        lst, event, verdict = m.group(1), m.group(2), m.group(6)
        short = next(s for e, s in MU_EVENTS if event.startswith(e))
        self.mu[(lst, short)] += 1
        if short == 'from the pause' and verdict == 'stays paused':
            self.mu[(lst, 'paused')] += 1
        if short in ('from the pause', 'back in the pause') and verdict != 'stays paused' and not m.group(7):
            if len(self.mu_loud) < 20:
                self.mu_loud.append((ts, lst, short, verdict))
            self.mu[(lst, 'loud')] += 1
        return True

    def parse_ft(self, ts, msg):  # v3.59 FT359: True = a filter-theme line of the enemy bars read
        m = FT_SET.match(msg)
        if m:
            self.ft_set = (ts, m.group(1))
            return True
        m = FT_LOOK.match(msg)
        if m:
            self.ft_look[m.group(1)] += 1
            return True
        m = FT_HID1.match(msg)
        if m:
            if self.ft_hid[3] is None:
                self.ft_hid[3] = (ts, m.group(1), m.group(2))
            return True
        m = FT_HIDR.match(msg)
        if m:
            self.ft_hid[0] += int(m.group(2))
            self.ft_hid[1] += 1
            self.ft_hid[2] = max(self.ft_hid[2], int(m.group(3)))
            return True
        return False

    def ft_line(self):  # v3.59 FT359: None when the log has none of its lines
        if not self.ft_set and not self.ft_look and not self.ft_hid[1] and self.ft_hid[3] is None:
            return None
        line = 'enemy bars under the special filters: %s' % ('FilterThemes ' + self.ft_set[1] if self.ft_set else 'FilterThemes not in the log')
        if self.ft_look:
            order = ('Thermal Vision', 'Spirit Vision', 'Night Vision', 'Night Mono', 'stock')
            line += '; looks worn: ' + ', '.join('%s %d' % (k, self.ft_look[k]) for k in order if self.ft_look[k])
        if self.ft_hid[1] or self.ft_hid[3] is not None:
            n = max(self.ft_hid[0], self.ft_hid[2])
            line += '; Thermal Vision hid %d victim bar(s) (%d room line(s))%s' % (
                n, self.ft_hid[1], '; first type %s in room %s [%s]' % (self.ft_hid[3][1], self.ft_hid[3][2], self.ft_hid[3][0])
                if self.ft_hid[3] else '')
        return line

    def hb_ref_line(self):  # v3.52 HB352: the enemy bars refused, the bosses' first bars
        hid = max(self.hb_ref['hidden'], self.hb_ref_tot[0], self.hb_ref_lines['hidden'])
        plc = max(self.hb_ref['place'], self.hb_ref_tot[1], self.hb_ref_lines['place'])
        line = ('enemy health bars: no bar for %d enemy(ies) the game does not draw yet (hidden, arriving or a placeholder) and %d with '
                'no place in the room (%d room line(s), %d verbose line(s)%s)' % (
                    hid, plc, self.hb_ref['rooms'], sum(self.hb_ref_lines.values()),
                    '; first %s type %s in room %s [%s]' % (self.hb_ref_first[1], self.hb_ref_first[2], self.hb_ref_first[3],
                                                            self.hb_ref_first[0]) if self.hb_ref_first else ''))
        g = self.en358  # v3.58 EN358
        if g['hb_undrawn'] or g['hb_dog']:
            line += '; %d of them nothing draws yet (%d event dog(s) before story flag 130)' % (g['hb_undrawn'] + g['hb_dog'], g['hb_dog'])
        if g['w_down'] or g['w_stumble'] or g['w_gone'] or g['w_other'] or self.en358_wsite:
            line += ('; Walter downed: %d down with the revive bar (at most %d s), %d down without a countdown, %d gone, %d '
                     'other%s' % (g['w_down'], g['w_most'], g['w_stumble'], g['w_gone'], g['w_other'],
                                  ' (timers: the stalker %s, the final %s)' % self.en358_wsite if self.en358_wsite else ''))
        g9 = self.en359  # v3.59 EN359
        if g9['anchors'] or g9['edges'] or self.en359_wlong:
            line += ('; anchors over the heads: %d type(s) seen (%d by the fallback height), %d held at the view\'s top edge; '
                     'Walter\'s long down %s (%d heal countdown(s))' % (g9['anchors'], g9['anchors_fb'], g9['edges'],
                                                                     self.en359_wlong or 'not in the log', g9['w_heal']))
        if self.hb_boss:
            up = [b for b in self.hb_boss if b[3] is not None]
            late = [b for b in self.hb_boss if b[3] is None]
            line += '; bosses: bar up %d time(s)%s, no bar 5 s after it became active %d time(s)%s' % (
                len(up), ' (at most %.1f s after it became active; first %s in room %s [%s])' % (
                    max(b[3] for b in up), up[0][1], up[0][2], up[0][0]) if up else '',
                len(late), ' (last: %s, %s in room %s [%s])' % (late[-1][4], late[-1][1], late[-1][2], late[-1][0]) if late else '')
        return line

    def parse_ql(self, ts, msg):  # v3.52 QL352: True = a Quick Load path line read
        m = QL_SET.match(msg)
        if m:
            self.ql_set = (ts, 'pause' if m.group(1).startswith('runs') else 'v3.51', m.group(2), m.group(3))
            return True
        m = QL_LOAD.match(msg)
        if m:
            path = m.group(4) or ''
            if path == 'the Extra Menu inside the pause':
                self.ql['pause'] += 1
                if m.group(5):
                    self.ql_black.append(int(m.group(5)))
                if m.group(6):
                    self.ql['late'] += 1
                if m.group(10) and m.group(10) != 'stays paused':
                    self.ql['loud'] += 1
            elif path.startswith('the Quick Load key'):
                self.ql['key'] += 1
            elif path.startswith('the Extra Menu after the pause closed'):
                self.ql['closed'] += 1
                r = re.search(r'\(v3\.51: (.*)\)$', path)
                self.ql_closed[r.group(1) if r else '?'] += 1
            else:
                self.ql['unknown'] += 1  # before v3.52 (no path)
            return True
        m = QL_BACK.match(msg)
        if m:
            self.ql['back'] += 1
            self.ql_ms.append(int(m.group(1)))
            if m.group(2):
                self.ql['notrun'] += 1
            return True
        if QL_DROP.match(msg):
            self.ql['drop'] += 1
            return True
        if QL_REFUSED.match(msg):
            self.ql['refused'] += 1
            return True
        return False

    def ql_line(self):  # v3.52 QL352: None when the log has no quick load and no install line
        n = sum(self.ql[k] for k in ('pause', 'key', 'closed', 'unknown'))
        if not self.ql_set and not n and not self.ql['refused']:
            return None
        if self.ql_set:
            head = 'quick load: %s at the start (QuickLoadInPause=%s%s)' % (
                'inside the pause' if self.ql_set[1] == 'pause' else 'the pause closes first (v3.51)', self.ql_set[2],
                ': ' + self.ql_set[3] if self.ql_set[3] else '')
        else:
            head = 'quick load: (no start line)'
        line = head + '; %d quick load(s)' % n
        parts = ['%d %s' % (self.ql[k], name) for k, name in (('pause', 'inside the pause'), ('key', 'by the key in play'),
                                                               ('closed', 'after the pause closed (v3.51)'),
                                                               ('unknown', 'path not logged'))] if n else []
        parts = [p for p in parts if not p.startswith('0 ')]
        if parts:
            line += ' (%s)' % ', '.join(parts)
        if self.ql_closed:
            line += '; v3.51 path because: %s' % ', '.join('%s x%d' % kv for kv in self.ql_closed.most_common())
        if self.ql['pause']:
            if self.ql_black:
                line += '; black in %d frame(s) (worst %d)' % (min(self.ql_black), max(self.ql_black))
            if self.ql_ms:
                line += '; the music held until the loaded room ran: %d ms on average (worst %d)' % (
                    sum(self.ql_ms) // len(self.ql_ms), max(self.ql_ms))
            owed = self.ql['pause'] - self.ql['back'] - self.ql['drop']
            if self.ql['drop']:
                line += '; %d handed to a menu / cut-scene of the loaded room' % self.ql['drop']
            if owed > 0:
                line += '; %d still owed at the log end' % owed
        if self.ql['late']:
            line += '; %d NOT black in time' % self.ql['late']
        if self.ql['loud']:
            line += '; the music NOT paused at black %d time(s)' % self.ql['loud']
        if self.ql['notrun']:
            line += '; the loaded room NOT seen running %d time(s)' % self.ql['notrun']
        if self.ql['refused']:
            line += '; %d refused at black' % self.ql['refused']
        if self.ql_anom:
            line += '; %d ANOMALY line(s)' % self.ql_anom
        return line

    def mu_line(self):  # v3.51 MU351: None when the log has none of its lines
        if not self.mu_set and not self.mu:
            return None
        if self.mu_set:
            line = 'extra menu sound: %s at the start (PauseSound=%s)' % (
                'paused in the three lists as in the pause menu' if self.mu_set[1] == 'paused' else 'v3.50 (the music plays quietly in '
                'the three lists)', self.mu_set[2])
        else:
            line = 'extra menu sound: (no start line)'
        for lst in MU_LISTS:
            opened = self.mu[(lst, 'from the pause')] + self.mu[(lst, 'from play')]
            closed = sum(self.mu[(lst, s)] for s in ('back in the pause', 'back to play', 'a save loads'))
            if not (opened or closed):
                continue
            line += '; %s: opened %d (%d from the pause, music paused in %d%s)' % (
                lst, opened, self.mu[(lst, 'from the pause')], self.mu[(lst, 'paused')],
                ', %d from play' % self.mu[(lst, 'from play')] if self.mu[(lst, 'from play')] else '')
            parts = ['%d %s' % (self.mu[(lst, s)], s) for s in ('back in the pause', 'back to play', 'a save loads') if self.mu[(lst, s)]]
            line += ', closed %d%s' % (closed, ' (%s)' % ', '.join(parts) if parts else '')
        if self.mu_loud:
            line += '; the music NOT paused %d time(s): %s' % (
                sum(self.mu[(l, 'loud')] for l in MU_LISTS), ', '.join('%s %s (%s) [%s]' % (l, e, v, t) for t, l, e, v in self.mu_loud[:4]))
        if self.mu_anom:
            line += '; %d ANOMALY line(s)' % self.mu_anom
        return line

    def parse_sd(self, ts, msg):  # v3.56 SD356: True = a Background / keep-awake / focus / device line read
        m = SD_BG.match(msg)
        if m:
            self.sd_bg = m.groups()
            return True
        m = SD_KEEP_ON.match(msg)
        if m:
            self.sd_keep = (ts, m.group(1), m.group(2))
            return True
        if SD_KEEP_NONE.match(msg):
            self.sd_keep = 'none'
            return True
        if SD_KEEP_RE.match(msg):
            self.sd_keep_re += 1
            return True
        m = SD_FOCUS_AWAY.match(msg)
        if m:
            self.sd_focus.append((ts, 'away', float(m.group(1))))
            return True
        m = SD_FOCUS_BACK.match(msg)
        if m:
            self.sd_focus.append((ts, 'back', float(m.group(1))))
            return True
        if SD_FOCUS_CAP.match(msg):
            self.sd_focus_cap = True
            return True
        m = SD_DEV_GAME.match(msg)
        if m:
            self.sd_dev = (ts, m.group(1), m.group(2) if m.group(3) is None else 'NOT REGISTERED hr ' + m.group(3))
            return True
        m = SD_DEV_NEW.match(msg)
        if m:
            self.sd_dev_ev.append((ts, 'new', m.group(1)))
            return True
        m = SD_DEV_BACK.match(msg)
        if m:
            self.sd_dev_ev.append((ts, 'back', m.group(1)))
            return True
        m = SD_DEV_STATE.match(msg)
        if m:
            self.sd_dev_ev.append((ts, m.group(2), m.group(1)))
            return True
        m = SD_DEV_OFF.match(msg)
        if m:
            self.sd_dev_off = (ts, m.group(1)[:120])
            return True
        m = SD_CFG.match(msg)
        if m:
            self.sd_cfg[m.group(1)] += 1
            return True
        return self.parse_sd358(ts, msg)

    def parse_sd358(self, ts, msg):  # v3.58 SD358: True = a Volume 0 / Mute-by-session line read
        m = SD_ZERO_IN.match(msg)
        if m:
            self.sd_zero = (m.group(1),) + ((m.group(2), m.group(3), m.group(4)) if m.group(2) else ())
            return True
        m = SD_ZERO_MIN.match(msg)
        if m:
            for i in range(3):
                self.sd_zero_n[i] += int(m.group(i + 1))
            return True
        m = SD_SESS.match(msg)
        if m:
            self.sd_sess = ('session', int(m.group(1)), int(m.group(2)))
            return True
        m = SD_SESS_NONE.match(msg)
        if m:
            self.sd_sess = ('none', m.group(1))
            return True
        m = SD_SESS_MUTED.match(msg)
        if m:
            self.sd_mutes['more' if m.group(2) else 'mute'] += 1
            self.sd_mutes['sessions'] += int(m.group(1))
            return True
        m = SD_SESS_UNMUTED.match(msg)
        if m:
            self.sd_mutes['unmute'] += 1
            return True
        m = SD_SESS_FIXED.match(msg)
        if m:
            self.sd_mutes['fixed'] += int(m.group(1))
            return True
        m = SD_SESS_USER.match(msg)
        if m:
            self.sd_mutes['user'] += int(m.group(1))
            return True
        m = SD_SESS_KEPT.match(msg)  # v3.58 SD358-001
        if m:
            self.sd_mutes['kept'] += int(m.group(1))
            return True
        m = SD_SESS_DROP.match(msg)
        if m:
            self.sd_mutes['dropped'] += int(m.group(1))
            return True
        m = SD_SESS_TOUCH.match(msg)
        if m:
            self.sd_mutes['touched'] += 1
            return True
        m = SD_MARK_FAIL.match(msg)  # v3.58 SD358-002
        if m:
            self.sd_mutes['markfail'] += 1
            self.sd_mutes['markfail ' + m.group(1)] += 1
            return True
        m = SD_SESS_PEND.match(msg)  # v3.58 SD358-003
        if m:
            self.sd_mutes['pending'] += int(m.group(1))
            self.sd_mutes['pending muted'] += int(m.group(2))
            return True
        if SD_SESS_BACK.match(msg):
            self.sd_mutes['back'] += 1
            return True
        m = SD_SITES.match(msg)
        if m:
            self.sd_sites.append((ts, m.group(2) or 'FAILED'))
            return True
        m = SD_STALE.match(msg)
        if m:
            self.sd_stale.append((ts, int(m.group(1))))
            return True
        m = SD_EXIT.match(msg)
        if m:
            self.sd_exit = (ts, m.group(1))
            return True
        m = SD_KEEP_HOLD.match(msg)
        if m:
            if m.group(1).startswith('stopped'):
                self.sd_keep_hold += 1
            return True
        return self.parse_sd359(ts, msg)

    def parse_sd359(self, ts, msg):  # v3.59 SD359: True = a lost-device guard line read
        m = SD_GUARD_IN.match(msg)
        if m:
            self.sd_guard = m.groups()
            return True
        m = SD_GUARD_FIRST.match(msg)
        if m:
            self.sd_guard_first = m.group(1)[:160]
            return True
        if SD_GUARD_OFF.match(msg):
            self.sd_guard = 'off'
            return True
        m = SD_GONE_FIRST.match(msg)
        if m:
            self.sd_gone = (ts,) + m.groups()
            return True
        m = SD_GONE_N.match(msg)
        if m:
            self.sd_gone_n = (ts,) + tuple(int(x) for x in m.groups())
            return True
        if SD_RETRY.match(msg):
            self.sd_retry += 1
            return True
        return False

    def parse_fw(self, ts, msg):  # v3.59 SD359: True = a fault watch line read
        m = FW_IN.match(msg)
        if m:
            self.fw_in = m.group(1)
            return True
        m = FW_END.match(msg)
        if m:
            self.fw_end = tuple(int(x) for x in m.groups()[:7]) + (m.group(8), m.group(9))
            return True
        m = FW_LINE.match(msg)
        if m:
            self.fw.append((ts,) + m.groups())
            return True
        if FW_CAP.match(msg):
            self.fw_cap = True
            return True
        m = FW_STOP.match(msg)
        if m:
            self.fw_stop = (ts, m.group(1), 'logged only' if 'D3DFaultStop=0' in msg else 'stopped')
            return True
        return False

    def fw_line(self):  # v3.59 SD359: None when the log has no fault watch line
        if not (self.fw_in or self.fw or self.fw_end or self.fw_stop):
            return None
        parts = ['fault watch %s' % (self.fw_in or '?')]
        if self.fw:
            kinds = Counter(k for t, c, n, mod, off, th, k, g in self.fw)
            parts.append('%d fault(s) logged%s (%s)' % (len(self.fw), ', more counted' if self.fw_cap else '',
                                                       ', '.join('%d %s' % (v, k) for k, v in kinds.most_common())))
            parts.append('first: %s %s at %s+%s, %s%s [%s]' % (self.fw[0][1], self.fw[0][2], self.fw[0][3], self.fw[0][4], self.fw[0][5],
                                                             ", Refined's %s guard" % self.fw[0][7] if self.fw[0][7] else '', self.fw[0][0]))
        if self.fw_stop:
            parts.append("Refined's Direct3D work %s after two faults in a second (the last %s) [%s]" % (
                'STOPPED' if self.fw_stop[2] == 'stopped' else 'kept (D3DFaultStop=0)', self.fw_stop[1], self.fw_stop[0]))
        if self.fw_end:
            e = self.fw_end
            parts.append('at the end %d seen (Refined %d, the game %d, Direct3D / driver %d, other %d, none %d; %d not read): %s' % (
                e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]))
        return 'faults: ' + '; '.join(parts)

    def parse_snd(self, ts, msg):  # v3.52 SND352: True = a Sound line read
        if self.parse_sd(ts, msg):  # v3.56 SD356
            return True
        m = SND_IN.match(msg)
        if m:
            self.snd_in = m.groups()
            return True
        if SND_OFF.match(msg):
            self.snd_in = 'off'
            return True
        m = SND_START.match(msg)
        if m:
            self.snd_start = (ts, m.groups())
            return True
        m = SND_FIRST.match(msg)
        if m:
            self.snd_first = (ts, m.group(1), m.group(2))
            return True
        m = SND_BADMODE.match(msg)
        if m:
            self.snd_first = (ts, m.group(2), 'not a mode: "%s"' % m.group(1)[:30])
            return True
        m = SND_DIFF.match(msg)
        if m:
            self.snd_diff = (ts, m.group(1), m.group(2))
            return True
        m = SND_CHANGE.match(msg)
        if m:
            self.snd_changes.append((ts,) + m.groups())
            return True
        m = SND_MIN.match(msg)
        if m:
            self.snd_min.append(tuple(int(x) if x.isdigit() else x for x in m.groups()))
            return True
        if SND_BYTEFIX.match(msg):
            self.snd_fix['byte'] += 1
            return True
        if SND_ENGFIX.match(msg):
            self.snd_fix['engine'] += 1
            return True
        if SND_DISC.match(msg):
            self.snd_fix['discard'] += 1
            return True
        m = SND_NOTIN.match(msg)
        if m:
            self.snd_notin.append((m.group(1), m.group(2)))
            return True
        return False

    def parse_ol(self, ts, msg):  # v3.53 OL2: True = an Options look line read
        m = LKL_IN.match(msg)
        if m:
            self.ol_set = m.group(1)
            return True
        m = LKL_GLYPH.match(msg)
        if m:
            page, name, est, meas = int(m.group(1)), m.group(2), int(m.group(3)), int(m.group(9))
            cur = self.ol_pages.get(page)
            if cur is None:
                self.ol_pages[page] = [name, est, meas, ts, 1]
            else:
                cur[4] += 1
                if est > cur[1] or meas > cur[2]:
                    cur[0], cur[1], cur[2], cur[3] = name, max(est, cur[1]), max(meas, cur[2]), ts
            return True
        m = LKL_INERT.match(msg)
        if m:
            self.ol_inert[m.group(1)] += 1
            return True
        return msg.startswith('options look: ')

    def parse_ly(self, ts, msg):  # v3.54 OL34: True = an Options layout line read
        m = LYL_IN.match(msg)
        if m:
            g = m.groups()
            self.ly_moves, self.ly_moves_why = g[0], g[1] or ''  # v3.55 OL56 (None: the v3.54 line)
            g = g[2:]
            self.ly_in = (ts, g[0], int(g[1]), int(g[2]), int(g[3]), int(g[4]), int(g[6]), int(g[7]), bool(g[8]))
            return True
        if LYL_VB_IN.match(msg):  # v3.55 OL56
            self.ly_vb = 'in'
            return True
        m = LYL_VB_BYTES.match(msg)
        if m:
            self.ly_vb = 'bytes ' + m.group(1)
            return True
        if LYL_OFF.match(msg):
            self.ly_off = True
            return True
        m = LYL_MISS.match(msg)
        if m:
            self.ly_miss.append((int(m.group(1)), m.group(2), m.group(3), m.group(4)))
            return True
        m = LYL_OPEN.match(msg)
        if m:
            self.ly_open.append((ts, m.group(1), m.group(2)))
            return True
        return False

    def parse_mn(self, ts, msg):  # v3.56 MN356: True = a value-line, folder-click or Extra Menu text line read
        m = VBX_ON.match(msg)  # v3.58 MN358: the value box
        if m:
            self.vbx = ('on', bool(m.group(1)), m.group(2))
            return True
        m = VBX_OFF.match(msg)
        if m:
            self.vbx = ('off', 'off (ValueBox=0)' if m.group(1).startswith('off') else 'NOT on (%s not stock)' % m.group(2))
            return m.group(1).startswith('off')
        m = VBX_PAGE.match(msg)
        if m:
            g = m.groups()
            self.vbx_pages[int(g[0])] = (g[1], int(g[2]), int(g[3]), int(g[4]), int(g[5]), int(g[6]), int(g[7]), g[8], bool(g[9]))
            return True
        m = VBX_GROW.match(msg)
        if m:
            self.vbx_grow.append((ts,) + tuple(int(x) for x in m.groups()))
            return True
        m = MN_VL_IN.match(msg)
        if m:
            self.mn_vl = (int(m.group(1)), int(m.group(2)), int(m.group(3)), 'OVER THE ROW LIMIT' in m.group(4))
            return True
        m = MN_VL_ANS.match(msg)
        if m:
            self.mn_vl_ans.append((ts, m.group(1), m.group(2), m.group(3)))
            return True
        m = MN_VL_MISS.match(msg)
        if m:
            self.mn_vl_miss.append((m.group(1), m.group(2), int(m.group(3)), m.group(4)))
            return True
        m = MN_FC_SET.match(msg)
        if m:
            self.mn_fc = ('row' if m.group(1).startswith('anywhere') else 'v3.55', int(m.group(2)), m.group(3).lstrip('; '))
            return True
        m = MN_FC_OPEN.match(msg)
        if m:
            self.mn_fc_open.append((ts, int(m.group(1)), m.group(2), int(m.group(3)), m.group(4)))
            return True
        m = MN_FC_REFUSE.match(msg)
        if m:
            self.mn_fc_refuse.append((ts, int(m.group(1)), int(m.group(2)), m.group(3)))
            return True
        m = MN_EM_TEXT.match(msg)
        if m:
            self.mn_em_text = ('once' if m.group(1) == 'once' else 'stock', int(m.group(2)))
            return True
        return False

    def mn_line(self):  # v3.56 MN356: None when the log has none of its lines
        if not (self.mn_vl or self.mn_vl_ans or self.mn_vl_miss or self.mn_fc or self.mn_fc_open or self.mn_fc_refuse or
                self.mn_em_text):
            return None
        parts = []
        if self.mn_vl:
            parts.append('value lines in %d row(s) (%d lines of the plan\'s %d)%s' % (
                self.mn_vl[0], self.mn_vl[1], self.mn_vl[2], ' OVER THE ROW LIMIT' if self.mn_vl[3] else ''))
        else:
            parts.append('value lines: no build line')
        if self.mn_vl_ans:
            parts.append('%d row(s) showed a value\'s own line where v3.55 showed the row\'s (%s%s)' % (
                len(self.mn_vl_ans), ', '.join('%s: "%s"' % (a[2], a[3]) for a in self.mn_vl_ans[:6]),
                ', ...' if len(self.mn_vl_ans) > 6 else ''))
        if self.mn_vl_miss:
            parts.append('%d value(s) kept the row\'s line (no line in the plan: %s%s)' % (
                len(self.mn_vl_miss), ', '.join('%s %s' % (a[1], a[3]) for a in self.mn_vl_miss[:6]),
                ', ...' if len(self.mn_vl_miss) > 6 else ''))
        if self.mn_fc:
            parts.append('folder rows open on a click %s (FolderClick=%d%s)' % (
                'anywhere on the row' if self.mn_fc[0] == 'row' else 'as v3.55 (Enter)', self.mn_fc[1],
                ('; ' + self.mn_fc[2]) if self.mn_fc[2] else ''))
        if self.mn_fc_open or self.mn_fc_refuse:
            moved = sum(1 for a in self.mn_fc_open if 'the cursor moved there' in a[4])
            parts.append('%d click(s) opened a row (%d moved the cursor there)%s; %d click(s) not taken%s' % (
                len(self.mn_fc_open), moved, (', the last %s row %d [%s]' % (self.mn_fc_open[-1][2], self.mn_fc_open[-1][3],
                                                                          self.mn_fc_open[-1][0])) if self.mn_fc_open else '',
                len(self.mn_fc_refuse), (' (%s [%s])' % (self.mn_fc_refuse[-1][3], self.mn_fc_refuse[-1][0])) if self.mn_fc_refuse else ''))
        if self.mn_em_text:
            parts.append('Extra Menu text %s' % ('drawn once (StockShadow=0)' if self.mn_em_text[0] == 'once' else
                                                 'with the stock second shadow (StockShadow=1)'))
        return 'options menu (v3.56): ' + '; '.join(parts)

    def ly_line(self):  # v3.54 OL34: None when the log has no Options layout line
        if not (self.ly_in or self.ly_off or self.ly_miss or self.ly_open):
            return None
        if self.ly_off:
            line = 'options layout: off (no Options pages: the game\'s own three)'
        elif self.ly_in:
            i = self.ly_in
            line = ('options layout: the cycle %s; %d pages, %d rows placed (%d folder rows), %d left out; %d of %d texts%s [%s]' % (
                i[1], i[2], i[3], i[4], i[5], i[6], i[7], ' (OVER A LIMIT)' if i[8] else '', i[0]))
            if self.ly_moves:  # v3.55 OL56: the layout and the volume bars
                line = line.replace('options layout: ', 'options layout: stock moves %s%s; ' % (
                    self.ly_moves, ' (%s)' % self.ly_moves_why if self.ly_moves_why else ''), 1)
                if self.ly_moves == 'on':
                    line += {'in': '; volume bars by record (0x525672)', 'failed': '; volume bars by label: writing 0x525672 FAILED',
                             None: '; volume bars: no 0x525672 line'}.get(
                        self.ly_vb, '; volume bars by label: 0x525672 is %s (another mod?)' % (self.ly_vb or '')[6:])
        else:
            line = 'options layout: (no install line: a log before v3.54)'
        if self.ly_miss:
            line += '; left out: %s' % ', '.join('%s on %d %s (%s)' % (r, p, t, w) for p, t, r, w in self.ly_miss[:8])
        if self.ly_open:
            last = self.ly_open[-1]
            pages = last[2].split(', ')
            line += '; Options opened %d time(s), the last [%s]: %d pages (%s%s)' % (
                len(self.ly_open), last[0], len(pages), ', '.join(pages[:6]), ', ...' if len(pages) > 6 else '')
        return line

    def ol_line(self):  # v3.53 OL2: None when the log has no Options look line
        if not (self.ol_set or self.ol_pages or self.ol_inert or self.ol_over):
            return None
        line = 'options look: %s' % {'in': 'in', 'off': 'off ([Options] Look=0)', None: '(no install line)'}[self.ol_set]
        if self.ol_pages:
            top = max(self.ol_pages.items(), key=lambda kv: (kv[1][1], kv[1][2]))
            meas = [v[2] for v in self.ol_pages.values() if v[2] >= 0]
            line += ('; menu glyphs on %d page(s): the busiest page %d (%s) about %d%s of the 800 [%s]' % (
                len(self.ol_pages), top[0], top[1][0], top[1][1], (', measured %d' % top[1][2]) if top[1][2] >= 0 else '', top[1][3]))
            if meas:
                line += ', the most measured %d' % max(meas)
            line += ' (%s)' % ', '.join('%d %s %d' % (k, v[0], v[1]) for k, v in list(self.ol_pages.items())[:12])
        if self.ol_inert:
            line += '; keys on grey rows ignored: %s' % ', '.join('%s %d' % kv for kv in self.ol_inert.most_common(6))
        if self.ol_over:
            line += '; %d page(s) OVER the 800 glyphs (ANOMALY)' % self.ol_over
        return line

    def snd_line(self):  # v3.52 SND352: None when the log has no Sound line
        if not (self.snd_in or self.snd_start or self.snd_changes or self.snd_min or self.snd_fix or self.snd_notin or self.snd_anom):
            return None
        if self.snd_in == 'off':
            line = 'sound: off ([Sound] Enabled=0: the game\'s own)'
        elif self.snd_start:
            t, g = self.snd_start
            line = ('sound: at the start Mode %s (the game\'s Sound %s, engine %s), pan law %s, Stereo Width %s %%, Room Reverb %s (%s), '
                    'Night Mode %s, Mono fold %s; speakers %s  [%s]' % (g[0], g[1], g[2], g[3], g[4], g[6], g[7], g[8], g[9], g[10], t))
        else:
            line = 'sound: (no start line: the first frame never came, or a log before v3.52)'
        if self.snd_in and self.snd_in != 'off':
            names = ('pan', 'volume', 'reverb', 'mono streams', 'mono music')
            bad = [n for n, v in zip(names, self.snd_in) if v == 'NOT IN']
            line += '; sites %s' % ('all in' if not bad else 'NOT IN: ' + ', '.join(bad))
            if self.snd_in[0] == 'Stock':
                line += ' (PanLaw=Stock)'
        if self.snd_first:
            line += '; first start: Mode %s from the save\'s Sound %s  [%s]' % (self.snd_first[1], self.snd_first[2], self.snd_first[0])
        if self.snd_diff:
            line += '; the save said Sound %s, the ini Mode %s won  [%s]' % (self.snd_diff[1], self.snd_diff[2], self.snd_diff[0])
        if self.snd_changes:
            line += '; %d change(s): %s' % (len(self.snd_changes), ', '.join(
                '%s %s -> %s (%s) [%s]' % (w, a, b, why, t) for t, w, a, b, why in self.snd_changes[:8]))
            if len(self.snd_changes) > 8:
                line += ', ...'
        if self.snd_min:
            s = [sum(r[i] for r in self.snd_min) for i in (0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12)]
            def pc(a, b):
                return '%.1f %%' % (100.0 * a / b) if b else '-'
            line += ('; %d minute line(s): pans 2D %d (one ear only %s; the stock law %s), streams %d (one ear only %s; the stock law '
                     '%s); volumes 2D %d, 3D %d, streams %d (Night Mode changed %d); Mono: %d stream block(s), %d music fold(s)' % (
                         len(self.snd_min), s[4], pc(s[5], s[4]), pc(s[6], s[4]), s[7], pc(s[8], s[7]), pc(s[9], s[7]), s[0], s[1], s[2],
                         s[3], s[10], s[11]))
        if self.snd_fix:
            line += '; the Sound byte put back %d time(s), the engine re-applied %d time(s) (%d at an Options discard)' % (
                self.snd_fix['byte'], self.snd_fix['engine'] + self.snd_fix['discard'], self.snd_fix['discard'])
        if self.snd_notin:
            line += '; NOT the GOG code: %s' % ', '.join('%s %s' % (w, va) for w, va in self.snd_notin[:4])
        if self.sd_zero:  # v3.58 SD358
            z = self.sd_zero
            if z[0] == 'silent' and len(z) == 4:
                bad = [n for n, v in zip(('2D / music', 'streams', 'the 0x2774 fade'), z[1:]) if v == 'NOT IN']
                line += '; volume 0 silent (%s)' % ('all in' if not bad else 'NOT IN: ' + ', '.join(bad))
            else:
                line += "; volume 0 the game's own (%s)" % z[0]
            if any(self.sd_zero_n):
                line += ': %d 2D, %d 3D, %d stream volume(s) silenced' % tuple(self.sd_zero_n)
        if self.snd_anom:
            line += '; %d ANOMALY line(s)' % self.snd_anom
        return line

    def sd_line(self):  # v3.56 SD356: None when the log has no Background / keep-awake / focus / device line
        if not (self.sd_bg or self.sd_keep or self.sd_focus or self.sd_dev or self.sd_dev_ev or self.sd_dev_off or self.sd_cfg or
                self.sd_keep_re or self.sd_sess or self.sd_mutes or self.sd_sites or self.sd_stale or self.sd_exit or self.sd_sess_lost or
                self.sd_keep_hold or self.sd_guard or self.sd_gone or self.sd_gone_n or self.sd_retry):  # v3.58 SD358  # v3.59 SD359
            return None
        parts = []
        if self.sd_bg:
            parts.append('Background %s (buffer sites %s)' % (self.sd_bg[0], self.sd_bg[1]))
        if self.sd_keep == 'FAILED':
            parts.append('keep-awake FAILED (ANOMALY)')
        elif self.sd_keep == 'none':
            parts.append('keep-awake: no sound device')
        elif self.sd_keep:
            parts.append('keep-awake on at %s dBFS (level %s)  [%s]' % (self.sd_keep[1], self.sd_keep[2], self.sd_keep[0]))
        elif self.sd_bg and self.sd_bg[2] == 'off':
            parts.append('keep-awake off')
        if self.sd_keep_re:
            parts.append('keep-awake restarted / not readable %d time(s)' % self.sd_keep_re)
        if self.sd_focus or self.sd_focus_cap:
            away = [x for x in self.sd_focus if x[1] == 'away']
            back = [x for x in self.sd_focus if x[1] == 'back']
            parts.append('%d time(s) behind other windows (longest %.1f s)%s' % (
                len(away), max([x[2] for x in back] or [0.0]), ', more not logged' if self.sd_focus_cap else ''))
        if self.sd_dev:
            parts.append('the game plays on "%s" (%s)  [%s]' % (self.sd_dev[1], self.sd_dev[2], self.sd_dev[0]))
        if self.sd_dev_off:
            parts.append('no device watch: %s  [%s]' % (self.sd_dev_off[1], self.sd_dev_off[0]))
        if self.sd_dev_ev:
            parts.append('%d device event(s): %s' % (len(self.sd_dev_ev), ', '.join(
                '%s "%s" [%s]' % ('a new default' if k == 'new' else 'the default back on' if k == 'back' else 'the game\'s device ' + k, n, t)
                for t, k, n in self.sd_dev_ev[:6])))
            nb = [k for t, k, n in self.sd_dev_ev if k in ('new', 'back')]
            if nb and nb[-1] == 'new':
                parts.append('at the end another default device: a restart takes it')
        if self.sd_cfg:
            parts.append('ini read as the default: %s' % ', '.join(sorted(self.sd_cfg)))
        parts += self.sd358_parts()  # v3.58 SD358
        parts += self.sd359_parts()  # v3.59 SD359
        return 'sound background: ' + '; '.join(parts)

    def sd359_parts(self):  # v3.59 SD359: the lost-device guard (the sd_line's tail)
        parts = []
        if self.sd_guard == 'off':
            parts.append('lost-device guard off (LostDeviceGuard=0)')
        elif self.sd_guard:
            parts.append('lost-device guard: music %s, streams %s, %s' % self.sd_guard)
        if self.sd_guard_first:
            parts.append('at the first frame: %s' % self.sd_guard_first)
        if self.sd_gone:
            parts.append('THE SOUND DEVICE WENT AWAY: the first stand-in buffer in %s (hr %s) [%s]' % (self.sd_gone[2], self.sd_gone[3],
                                                                                                  self.sd_gone[0]))
        if self.sd_gone_n:
            g = self.sd_gone_n
            parts.append('%d sound(s) made silent (2D %d, 3D %d, music %d, streams %d) [%s]' % (g[1], g[2], g[3], g[4], g[5], g[0]))
        if self.sd_retry:
            parts.append('%d failed create(s) made by the second try' % self.sd_retry)
        return parts

    def sd358_parts(self):  # v3.58 SD358: Mute by the sound session (the sd_line's tail)
        parts = []
        if self.sd_sess and self.sd_sess[0] == 'session':
            parts.append('Mute by the sound session (%d session(s) on %d device(s))' % self.sd_sess[1:])
        elif self.sd_sess:
            parts.append("Mute by DirectSound's focus rule (no sound session: %s)" % self.sd_sess[1])
        mu = self.sd_mutes
        if mu['mute'] or mu['unmute'] or mu['more']:
            parts.append('muted %d time(s) (%d session(s)%s), unmuted %d time(s)' % (
                mu['mute'], mu['sessions'], ', %d re-sweep(s) found more' % mu['more'] if mu['more'] else '', mu['unmute']))
        if mu['fixed']:
            parts.append('%d session(s) left muted by an earlier game unmuted at the start' % mu['fixed'])
        if mu['user']:
            parts.append('%d session(s) muted in Windows by the user (left: the game is muted in the Volume Mixer)' % mu['user'])
        if mu['kept'] or mu['dropped']:  # v3.58 SD358-001
            parts.append('SH4Refined.muted: %d entr(y/ies) kept for a later start, %d dropped (30 days)' % (mu['kept'], mu['dropped']))
        if mu['touched']:
            parts.append('%d held session(s) changed by the user in the Volume Mixer (left as set)' % mu['touched'])
        if mu['back']:
            parts.append('the session came back %d time(s)' % mu['back'])
        if self.sd_sess_lost:
            parts.append('%d session problem(s) (ANOMALY): %s [%s]' % (len(self.sd_sess_lost), self.sd_sess_lost[0][1], self.sd_sess_lost[0][0]))
        if self.sd_sites:
            parts.append('buffer sites now %s [%s]%s' % (self.sd_sites[-1][1], self.sd_sites[-1][0],
                                                       ' (%d change(s))' % len(self.sd_sites) if len(self.sd_sites) > 1 else ''))
        if self.sd_stale:
            parts.append('%d time(s) sounds with the global focus played on behind other windows (most %d)' % (
                len(self.sd_stale), max(n for t, n in self.sd_stale)))
        if self.sd_keep_hold:
            parts.append('keep-awake stopped behind other windows %d time(s)' % self.sd_keep_hold)
        if self.sd_exit:
            parts.append('closed behind other windows: sessions unmuted first (%s) [%s]' % (self.sd_exit[1], self.sd_exit[0]))
        if mu['markfail']:  # v3.58 SD358-002
            parts.append('SH4Refined.muted could not be written %d time(s) (%s): the old one kept, sessions it could not name not muted' % (
                mu['markfail'], ', '.join(sorted(k[9:] for k in mu if k.startswith('markfail ')))))
        if mu['pending']:  # v3.58 SD358-003
            parts.append('SH4Refined.muted: %d pending entr(y/ies) at the start (a mute not confirmed; left as they are, %d muted: '
                         'unmute in the Volume Mixer if silent), dropped' % (mu['pending'], mu['pending muted']))
        return parts

    def fadein_line(self):  # v3.50 FI350: None when the log has no Fade In line
        if not (self.fi or self.fi_rooms or self.fi_sets or self.fi_bad or self.fi_fault or self.fi_starts or self.fi_letgo or
                self.fi_close or self.fi_arrive):  # v3.59 FI359: + the arrive lines
            return None
        if self.fi is None:
            line = 'fade in: (no start line)'
        elif self.fi[0] == 'OFF':
            line = 'fade in: OFF (%s)' % self.fi[1]
        else:
            line = 'fade in: %s at the start (%s s; portal %s)' % (self.fi[2], self.fi[3], self.fi[1])
        n = sum(self.fi_src.values())
        line += '; %d fade(s) in %d room visit(s)' % (n, self.fi_rooms)
        if n:
            line += ' (%s)' % ', '.join('%s %d' % (k, self.fi_src[k]) for k in FI_SRC if self.fi_src[k])
        if self.fi_portals or self.fi_drawn:
            line += '; %d %s portal(s), %s' % (self.fi_portals, 'blood' if self.fi_tar is None else 'tar / blood',
                                               'first drawn [%s] (%s viewport)' % self.fi_drawn
                                               if self.fi_drawn else 'NEVER DRAWN')
        line += self.fadein_tar_part()  # v3.51 PT351
        k = sum(self.fi_skip.values())
        if k:
            line += '; not faded %d (%s)' % (k, ', '.join('%s %d' % (r, self.fi_skip[r]) for r in FI_SKIP if self.fi_skip[r]))
        if self.fi_starts:
            line += '; verbose: %d start line(s)' % sum(self.fi_starts.values())
        if self.fi_own:
            line += '; own draw states: %s' % ', '.join(sorted(self.fi_own))
        if self.fi_bad:
            line += '; unknown ini value: FadeIn "%s" -> %s' % self.fi_bad
        if self.fi_sets:
            line += '; %d change(s): %s' % (len(self.fi_sets), ', '.join('%s (%s) [%s]' % (v, how, t) for t, v, how in self.fi_sets[:6]))
        if self.fi_fault:
            line += '; FAULT in %s (code %s) [%s]: off for the session' % (self.fi_fault[1], self.fi_fault[2], self.fi_fault[0])
        return line

    def fadein_big_part(self):  # v3.58 FI358: the bigger, stickier portal ('' when the log has none of it)
        out = ''
        if self.fi_big:
            out += '; v3.58 portal: strands %s s, pieces up to %s, sticky strands %s' % self.fi_big
        if self.fi_stick_bad is not None:
            out += '; unknown ini value: FadeInStrandSeconds "%s" -> 3.0' % self.fi_stick_bad
        c = self.fi_close
        if c:
            seen = [x for x in c if x[9] != 'not seen']
            own = [x for x in c if x[11]]
            out += '; %d closed (grids at most %d, %.1f a portal; ' % (len(c), max(x[2] for x in c), sum(x[2] for x in c) / len(c))
            out += ('portal / body %.2f .. %.2f x (%d by bones, %d by type)' % (
                min(x[8] for x in seen), max(x[8] for x in seen), sum(1 for x in seen if x[9] == 'its bones'),
                sum(1 for x in seen if x[9] != 'its bones')) if seen else 'no body seen')
            out += '; strands held %.1f .. %.1f s after it was in' % (min(x[13] for x in c), max(x[13] for x in c))
            if own:
                out += ", Refined's %d (%d on its box), %d stretched loose" % (
                    sum(x[11] for x in own), sum(x[11] for x in own if x[12] == 'its box'), sum(x[16] for x in own))
            out += '; closed at %.1f .. %.1f s; draw at most %.3f ms a frame a portal, worst %.3f)' % (
                min(x[1] for x in c), max(x[1] for x in c), max(x[17] for x in c), max(x[18] for x in c))
            if seen and min(x[8] for x in seen) < 1.0:
                out += '; a portal SMALLER than its body'
        return out

    def fadein_floor_part(self):  # v3.59 FI359: the portal on the floor, the arrivals, Spirit Vision ('' when none of it)
        out = ''
        if self.fi59:
            out += '; v3.59: %s, Spirit Vision %s' % (self.fi59[0], 'spirit tar' if self.fi59[1] == '1' else "v3.58's vortex")
        a = self.fi_arrive
        if a:
            fl = [x for x in a if x[2] is not None]
            out += '; %d arrival(s): %d on the floor' % (len(a), len(fl))
            if fl:
                out += ' (foot %d .. %d u above it, raised up to %d u, %d under a low ceiling)' % (
                    min(x[2] for x in fl), max(x[2] for x in fl), max(x[3] for x in fl), sum(1 for x in fl if x[4] < 100))
            if len(fl) < len(a):
                out += ', %d with no floor found' % (len(a) - len(fl))
            vic = [x for x in a if x[6] is not None]
            if vic:
                out += '; victims came out %d .. %d u above the floor (own height %d .. %d u), %d lifted (up to %d u)' % (
                    min(x[5] for x in vic), max(x[5] for x in vic), min(x[6] for x in vic), max(x[6] for x in vic),
                    sum(1 for x in vic if x[7]), max(x[7] for x in vic))
            st = Counter('AI %s step %d anim %d' % (x[8], x[9], x[10]) for x in a)
            out += '; as seen: %s' % ', '.join('%s x%d' % (k, st[k]) for k in sorted(st))
            w = [x for x in a if x[11] > 0]
            if w:
                out += '; %d kept unseen while out of a wall by itself (%.2f .. %.2f s)' % (
                    len(w), min(x[11] for x in w), max(x[11] for x in w))
            if fl and min(x[2] for x in fl) < 0:
                out += '; a portal UNDER THE FLOOR'
        if self.fi_waitcap:
            out += '; %d at the wait\'s cap' % self.fi_waitcap
        if self.fi_spirit:
            out += '; Spirit Vision drew: %s' % ', '.join(sorted(self.fi_spirit))
        return out

    def fadein_tar_part(self):  # v3.51 PT351: the tar portal and the hold ('' when the log has none of it)
        out = ''
        if self.fi_tar is not None:
            tar, tdm, hold = self.fi_tar
            if tar.startswith('NOT available'):
                out += '; tar portal %s' % tar
            else:
                draw = 'Refined\'s draw' if 'FadeInPortalDraw=Refined' in tar or 'the game\'s is not stock' in tar else \
                    'the game\'s own draw, else Refined\'s'
                out += '; tar portal: %s (%s), %s' % (draw, tdm, 'held' if hold == '1' and 'no hold' not in tar else 'NOT held')
        if sum(self.fi_by.values()) or self.fi_held:
            out += '; by draw: %s; %d held' % (', '.join('%s %d' % (k, self.fi_by[k]) for k in ('game', 'Refined', 'vortex')
                                                      if self.fi_by[k]) or 'none', self.fi_held)
        tar_drawn = self.fi_tar_drawn
        if tar_drawn:
            out += '; first tar portal drawn [%s] (%s)' % tar_drawn
        elif self.fi_by['game'] or self.fi_by['Refined']:
            out += '; tar portal NEVER DRAWN'
        if self.fi_cost[0]:
            out += '; portal draw at most %.3f ms a frame on average in a room (worst frame %.3f ms)' % (self.fi_cost[1], self.fi_cost[2])
        if self.fi_letgo:
            n = len(self.fi_letgo)
            st = Counter(x[4] for x in self.fi_letgo)
            early = Counter(x[3] for x in self.fi_letgo if x[3] != 'held to the end')
            moved, unheld = sum(x[6] for x in self.fi_letgo), sum(x[7] for x in self.fi_letgo)
            out += '; %d let go (strands: %s; moved %d of %d u%s)' % (
                n, ', '.join('%s %d' % (k, st[k]) for k in sorted(st)), moved, unheld,
                '; early: ' + ', '.join('%s %d' % (k, early[k]) for k in sorted(early)) if early else '')
        if self.fi_tar_starts:
            out += '; tar starts: %s' % ', '.join('%s %d' % (k, self.fi_tar_starts[k]) for k in sorted(self.fi_tar_starts))
        out += self.fadein_big_part()  # v3.58 FI358
        out += self.fadein_floor_part()  # v3.59 FI359
        if self.fi_real:
            out += '; %d placeholder(s) turned real' % self.fi_real
        if self.fi_never:
            out += '; %d portal(s) closed with nobody' % self.fi_never
        if self.fi_notstock:
            out += '; NOT stock: %s' % ', '.join(self.fi_notstock)
        if self.fi_tdm_bad is not None:
            out += '; unknown ini value: FadeInPortalDraw "%s" -> Auto' % self.fi_tdm_bad
        if self.fi_gfault:
            out += '; FAULT in the game\'s own portal draw (code %s) [%s]: Refined\'s draw after' % (self.fi_gfault[1], self.fi_gfault[0])
        return out

    def fol_mode_line(self):  # v3.49 EF349: Enemies Follow's value (None while it is Off all along, or not in the log)
        if not self.fol_modes and self.fol_mode in (None, 'Off') and not any(self.fol_home):
            return None
        line = 'enemies follow setting: %s at the start' % (self.fol_mode or 'not in the log')
        if self.fol_modes:
            line += '; %d change(s): %s' % (len(self.fol_modes), ', '.join('%s [%s]' % (v, ts) for ts, v in self.fol_modes))
        if self.fol_home[0] or self.fol_home[1]:
            line += ('; with no monsters following (Off / Victims): %d monster(s) sent home from other rooms, %d cop(ies) '
                     'made in their own rooms again' % tuple(self.fol_home))
        return line

    def ef_line(self):  # v3.50 EF350: followers across saves, Enemy Pursuit (None when nothing of it is in the log)
        e = self.ef
        if self.ef_mode is None and not e and not self.ef_modes and not self.ep and self.ep_site is None:  # v3.51 EP351: + ep
            return None
        line = 'enemy pursuit: %s at the start' % (self.ef_mode or 'not in the log')
        if self.ef_modes:
            line += '; %d change(s): %s' % (len(self.ef_modes), ', '.join('%s [%s]' % (v, ts) for ts, v in self.ef_modes))
        line += ('; %d lost track of you at %d door(s) (%d out of their senses, %d down when you left, %d did not catch up), %d '
                 'forgot before a door' % (e['lost'], e['lost_doors'], e['sight'], e['down'], e['behind'], e['forgot']))
        line += ('; followers across saves %s: %d save(s) kept followers (%d pursuer(s), %d of them with you, %d on their way, %d '
                 'ghost tag(s), %d cop(ies))' % (self.ef_site or 'not in the log', e['saves'], e['s_pur'], e['s_here'], e['s_q'],
                                                e['s_tag'], e['s_cw']))
        if e['saves_unwritten']:
            line += ', %d NOT WRITTEN to the side file' % e['saves_unwritten']
        line += ('; %d load(s) found theirs, %d had none; %d restore(s): %d pursuer(s) back (%d with you), %d on their way, %d '
                 'ghost(s) let in, %d cop(ies); left out %d without a record, %d of a kind not followed now' % (
                     e['found'], e['none'], e['restores'], e['r_pur'], e['r_here'], e['r_q'], e['r_tag'], e['r_cw'], e['r_norec'],
                     e['r_kind']))
        if e['r_no']:
            line += '; %d found but not restored (another room, or Follow off)' % e['r_no']
        g9 = self.en359  # v3.59 EN359
        if g9['loads'] or g9['kr_saves']:
            line += ('; followers restored %d of %d in %d load(s) (randomized followers\' records: %d kept at %d save(s), %d of %d put '
                     'back)' % (g9['back'], g9['saved'], g9['loads'], g9['kr_kept'], g9['kr_saves'], g9['kr_back'], g9['kr_all']))
        if g9['pins'] or self.en359_pinsite:
            line += '; pinned ghosts stay %s: %d let in pinned by the gate%s' % (
                self.en359_pinsite or 'not in the log', g9['pins'],
                ' (last %s in room %s [%s])' % (self.en359_pin_last[1], self.en359_pin_last[2], self.en359_pin_last[0])
                if self.en359_pin_last else '')
        p = self.ep  # v3.51 EP351
        if self.ep_site is not None or p:
            line += ('; inside rooms %s: %d gave up the chase in %d room(s) (%d monster(s), %d ghost(s); %d again, %d down when you '
                     'walked off), %d set back in their rest, %d left to the game' % (
                         self.ep_site or 'not in the log', p['give'], p['rooms'], p['mon'], p['ghost'], p['again'], p['down'],
                         p['held'], p['stock']))
            if p['undive'] or p['sensed']:  # v3.52 EP352
                line += ("; ghosts' calm: %d ghost dive(s) into a wall stopped, %d ghost(s) sensed you again" % (
                    p['undive'], p['sensed']))
            if p['v_give']:
                fams = sorted(k[2:] for k in p if k.startswith('f_'))
                line += (' [verbose: %d lines: %d unsensed, %d downed, %d far, %d untouched%s%s]' % (
                    p['v_give'], p['v_unsensed'], p['v_downed'], p['v_far'], p['v_untouched'],
                    ', %d out of sight' % p['v_sight'] if p['v_sight'] else '',  # v3.52 EP352
                    '; ' + ', '.join('%s %d' % (f, p['f_' + f]) for f in fams) if fams else ''))
        return line

    def parse_er(self, ts, msg):  # v3.56 ER356: True = an Enemies Return / world visit line read (the install line: False)
        e = self.er
        if msg.startswith('enemies: in ('):
            m = ER_MODE_IN.match(msg)
            if m:
                self.er_mode = m.group(2)
                self.er_tsite = 'in' if 'Tremer bodies 0048ACE5' in m.group(1) else (self.er_tsite or 'not in')
            return False
        if msg.startswith('gameplay: '):
            m = ER_MIGR.match(msg)
            if m:
                e['migr'] += 1
                e['migr_fail'] += not m.group(2).startswith('written')
                return True
            m = ER_MODE_SET.match(msg)
            if m:
                if not self.er_modes or self.er_modes[-1][1] != m.group(1):
                    self.er_modes.append((ts, m.group(1)))
            return False
        if msg.startswith('world visit: '):
            m = WV_ENTRY.match(msg)
            if m:
                e['entries'] += 1
                return True
            if WV_LOADL.match(msg):
                e['loads'] += 1
                return True
            return True  # (the verbose ones: left the world, a room load without a room)
        if msg.startswith('enemies: return: '):
            m = ER_WORLD.match(msg)
            if m:
                g = [int(x) for x in m.groups()]
                e['worlds'] += 1
                e['w_back'] += g[2]
                e['w_dogs'] += g[5]
                e['w_story'] += g[7]
                return True
            if ER_LOADNONE.match(msg):
                e['loadnone'] += 1
                return True
            m = ER_TREMER.match(msg)
            if m:
                e['tremer'] = max(e['tremer'], int(m.group(1)))
                return True
            if ER_TSITE.match(msg):
                self.er_tsite = 'off'
                return True
            return True  # (the mode's own line, the dogs' line)
        if msg.startswith('enemies: revive: room '):
            m = ER_ROOM.match(msg)
            if m:
                g = [int(x) for x in m.groups()]
                e['rooms'] += 1
                e['r_bodies'] += g[2]
                e['r_bits'] += g[3]
                e['r_away'] += g[4]
                e['r_copies'] += g[5]
                return True
        return False

    def er_line(self):  # v3.56 ER356: None when the log has no Enemies Return line and it is Off (or not in the log)
        e = self.er
        if self.er_mode in (None, 'Off') and not self.er_modes and not any(v for k, v in e.items() if k not in ('entries', 'loads')):  # v3.56 MERGE
            return None
        mode = self.er_mode or 'not in the log'
        line = 'enemies return: %s at the start%s' % (mode, ' (v3.55\'s On: read as By Room)' if mode == 'On' else '')
        if e['migr']:
            line += '; EnemyRevive On migrated to By Room%s' % (' (NOT written: tried again at the next start)' if e['migr_fail'] else '')
        if self.er_modes:
            line += '; %d change(s): %s' % (len(self.er_modes), ', '.join('%s [%s]' % (v, ts) for ts, v in self.er_modes))
        line += '; world visits: %d entr(ies), %d load(s) (no room change)' % (e['entries'], e['loads'])
        line += ('; By Room: %d room(s) brought back their dead (%d bod(ies), %d kill bit(s), %d of other rooms, %d cop(ies))' % (
            e['rooms'], e['r_bodies'], e['r_bits'], e['r_away'], e['r_copies']))
        line += ('; By World: %d world entr(ies) brought back %d killed monster(s) (%d event dog(s) marked, %d story room(s) left)' % (
            e['worlds'], e['w_back'], e['w_dogs'], e['w_story']))
        line += '; %d load(s) brought nothing back' % e['loadnone']
        line += '; Tremer site %s, %d Tremer bod(ies) kept' % (self.er_tsite or 'not in the log', e['tremer'])
        return line

    def drop_level_line(self):  # v3.38 DR338
        def name(lv):
            return 'Generous (= Abundant since v3.38)' if lv == 'Generous' else lv
        lv = self.drop_level
        head = 'supply drops level: %s' % (name(lv) if lv else 'not logged')
        x = self.drop_chances[0].get('Abundant' if lv == 'Generous' else lv) if self.drop_chances and lv else None
        if x is not None:  # v3.38 DR338c: this level's class chances (Normal's x the factor)
            head += ' (x%g of Normal: any item %g %% a roll; common / uncommon / rare: %s)' % (
                x, self.drop_chances[2] * x, ', '.join('%s %g / %g / %g %%' % (n, round(c * x, 4), round(u * x, 4), round(r * x, 4))
                                                    for n, c, u, r in self.drop_chances[1]))
        if self.drop_equal:
            head += '; shares ' + self.drop_equal
        if self.drop_alias:
            head += '; the ini\'s old "Generous" read as Abundant'
        if self.drop_changes:
            head += '; changed: ' + ', '.join('%s at %s' % (name(l), t) for t, l in self.drop_changes[:6]) + (
                ' (+%d)' % (len(self.drop_changes) - 6) if len(self.drop_changes) > 6 else '')
        it = self.drop_items
        groups = []
        for g, items in DROP_GROUPS:
            groups.append('%s %s' % (g, ' / '.join('%s %d' % (short, it[full]) for full, short in items)))
        return head + '; given by group: ' + ', '.join(groups)

    def add_group(self, d, key, ts, text):
        g = d.get(key)
        if g is None:
            d[key] = [1, ts, ts, text]
        else:
            g[0] += 1
            g[2] = ts

    def read(self):
        last = None
        wrap = 0.0
        with open(self.path, 'r', encoding='utf-8', errors='replace') as f:
            for raw in f:
                line = raw.rstrip('\r\n')
                if not line:
                    continue
                self.lines += 1
                m = TS.match(line)
                if m:
                    t = secs(m) + wrap
                    if last is not None and t < last - 60:  # past midnight
                        wrap += 86400.0
                        t += 86400.0
                    last = t
                    if self.t0 is None:
                        self.t0 = t
                    self.t1 = t
                    ts = line[:8]
                    msg = line[13:]
                else:
                    ts, msg = '', line
                lv = LEVEL.match(msg)
                if lv:
                    self.levels[lv.group(1)] += 1
                    msg = msg[lv.end():]
                else:
                    self.levels['n'] += 1
                self.cur_lv = lv.group(1) if lv else 'n'  # v3.48 SB348
                self.parse(ts, msg)
        if self.t0 is not None:
            self.span = self.t1 - self.t0

    def parse_omni(self, ts, msg):
        """v3.46 OM346: one Stick Movement line; True when it was one of ours. (v3.48 OM348: Omni-Directional Movement)"""
        if msg.startswith(('omni: latch', 'omni: lock-on walk', 'omni: [Input] OmniLatch', 'omni: LockOnWalk',
                           'omni: [Gameplay] LockOnWalk')):  # v3.59 CT359
            return self.parse_latch(ts, msg)
        m = OM_IN.match(msg)
        if m:
            self.om = (m.group(1), m.group(2), float(m.group(3)))
            return True
        m = OM_NOT.match(msg)
        if m:
            self.om_not = m.group(1)
            return False  # (the warnings group keeps it too)
        m = OM_SET.match(msg)
        if m:
            self.om_sets.append((ts, m.group(1), m.group(2), m.group(3)))
            return True
        m = OM_REREAD.match(msg)
        if m:
            self.om_sets.append((ts, m.group(1), m.group(2), '[Input] read again'))
            return True
        m = OM_DZ.match(msg)
        if m:
            self.om_dz.append((ts, float(m.group(1))))
            return True
        m = OM_OLD.match(msg)  # v3.48 OM348
        if m:
            how = m.group(3) or 'read as On'
            self.om_old.append((ts, m.group(1), {'written as On': 'written as On', 'not written': 'NOT written (no ini)',
                                                 'could not be written': 'NOT WRITTEN (ini error)'}.get(how, how)))
            return True
        m = OM_BIND.match(msg)
        if m:
            self.om_bind.append((ts, m.group(5), ' '.join(m.group(1, 2, 3, 4))))
            return True
        m = OM_CHECK.match(msg)
        if m:
            g = m.groups()
            self.om_checks.append((ts, int(g[0]), int(g[1]), int(g[2]), int(g[3]), int(g[4]), g[5]))
            return True
        m = OM_COUNTS.match(msg)
        if m:
            for i in range(3):
                self.om_counts[i] += int(m.group(2 + i))
            return True
        m = OM_GATE.match(msg)
        if m:
            st = '%s / %s, overlay %s, phase %s' % m.groups()
            if st not in self.om_gate:
                self.om_gate.append(st)
            return True
        if OM_SWAP.match(msg):
            self.om_swap = True
            return True
        m = OM_ROW.match(msg)  # v3.47 CT347
        if m:
            self.om_row = 'on Option 2, row %s of %s' % m.groups()
            return True
        m = OM_NOROW.match(msg)
        if m:
            self.om_row = 'NONE (%s)' % m.group(1)
            return True
        m = OM_PE.match(msg)
        if m:
            self.om_pe = {'f': 'follows the push', 'r': 'has the push off', 'd': 'keeps her speed (older version)'}[m.group(1)[0]]
            return True
        m = OM_BG.match(msg)
        if m:
            self.om_bg[0] += 1
            if m.group(3):
                self.om_bg[1] += int(m.group(3))
            return True
        m = OL_IN.match(msg)  # v3.49 LK349
        if m:
            self.ol = m.group(1)
            return True
        m = OL_REREAD.match(msg)
        if m:
            self.ol_sets.append((ts, m.group(1), m.group(2)))
            return True
        m = OL_BAD.match(msg)
        if m:
            self.ol_bad.append(m.group(1))
            return True
        m = OL_BIND.match(msg)
        if m:
            self.ol_bind = (ts, m.group(5), ' '.join(m.group(1, 2, 3, 4)))
            return True
        m = OL_CHECK.match(msg)
        if m:
            self.ol_check = (ts, int(m.group(1)), int(m.group(2)), 'on' if m.group(3).startswith('the analog') else 'none')
            return True
        m = OL_COUNT.match(msg)
        if m:
            self.ol_places += 1
            for i in range(13):
                self.ol_n[i] += int(m.group(4 + i))
            if m.group(1) == 'Room 302':
                self.ol_room[0] += 1
                self.ol_room[1] += int(m.group(12))
            return True
        m = OM_AGAIN.match(msg)
        if m:
            k = self.om_bg if m.group(2) else self.om_again
            if not m.group(2):
                k[0] += 1
            k[1] += int(m.group(1))
            return True
        return False

    def parse_latch(self, ts, msg):
        """v3.59 CT359: one line of the latch (omnilatch.inc L1) or Lock-On Walk (L2); True when it was one of ours."""
        m = LT_IN.match(msg)
        if m:
            self.lt = m.groups()
            return True
        m = LW_IN.match(msg)
        if m:
            self.lw = m.groups()
            return True
        m = LT_NOT.match(msg)
        if m:
            self.lt_not.append('%s (%s)' % (m.group(1), m.group(2)))
            return False  # (the warnings group keeps it too)
        m = LT_EP.match(msg)
        if m:
            g = m.groups()
            self.lt_eps.append((ts, g[0], float(g[1]), g[2].startswith('the Shoulder'), g[3] == 'On', bool(g[4]), bool(g[5]),
                                g[6].startswith('Refined'), g[7], int(g[8]), float(g[9]), int(g[10]), int(g[11])))
            return True
        m = LT_ROOM.match(msg)
        if m:
            self.lt_rooms += 1
            self.lt_supp += int(m.group(4))
            self.lt_over += int(m.group(5))
            return True
        m = LW_ROOM.match(msg)
        if m:
            self.lw_rooms.append((int(m.group(3)), int(m.group(4)), m.group(5)))
            return True
        m = LW_FIRST.match(msg)
        if m:
            self.lw_first = (ts, m.group(1), m.group(2))
            return True
        m = LT_REREAD.match(msg)
        if m:
            self.lt_sets.append((ts, m.group(1)))
            return True
        m = LW_SET.match(msg)
        if m:
            self.lw_sets.append((ts, m.group(1), m.group(3)))
            return True
        m = LW_REREAD.match(msg)
        if m:
            self.lw_sets.append((ts, m.group(1), '[Gameplay] read again'))
            return True
        m = LT_BAD.match(msg) or LW_BAD.match(msg)
        if m:
            self.lt_bad.append(m.group(1))
            return True
        m = LT_FAULT.match(msg)
        if m:
            self.lt_fault.append((ts, m.group(1)))
            return False  # (a fault: the warnings group keeps it too)
        return False

    def latch_line(self):
        """v3.59 CT359: the control-direction latch and Lock-On Walk summary line (None without any of their lines)."""
        if not (self.lt or self.lw or self.lt_not or self.lt_eps or self.lt_rooms or self.lw_rooms or self.lw_sets or
                self.lt_fault):
            return None
        line = '  latch / lock-on walk: '
        line += ('latch %s (OmniLatch %s)' % self.lt) if self.lt else 'latch (no install line)'
        if self.lt_sets:
            line += ', OmniLatch read again %s [%s]' % (self.lt_sets[-1][1], self.lt_sets[-1][0])
        if self.lt_eps:
            e = self.lt_eps
            trig = Counter(t.split(' (')[0] for _, t, *_ in e)
            why = Counter(x[8].split(' (')[0] for x in e)
            full = [x for x in e if x[8] == 'its time ran out']
            sig = [x for x in full if x[4] and x[3]]  # the analog stick under the Shoulder view, held to the end
            line += '; %d episode(s) (%s): let go by %s' % (len(e), ', '.join('%s %d' % kv for kv in sorted(trig.items())),
                                                            ', '.join('%s %d' % kv for kv in sorted(why.items())))
            line += '; longest %.2f s, the control frame up to %d deg from the camera, %d blended frame(s)' % (
                max(x[2] for x in e), max(x[11] for x in e), sum(x[12] for x in e))
            if full:
                line += '; %d held to the end (%d with Omni On under the Shoulder view: the analog control loss\'s signature%s)' % (
                    len(full), len(sig), ', the game\'s rule' if any(not x[7] for x in sig) else '')
            if any(x[6] for x in e):
                line += '; %d right after a warp of the player' % sum(1 for x in e if x[6])
            if any(x[5] for x in e):
                line += '; %d with the Shoulder camera\'s Follow on' % sum(1 for x in e if x[5])
        if self.lt_rooms:
            line += '; %d room summary line(s), %d rig look-rate trigger(s) ignored under the Shoulder camera%s' % (
                self.lt_rooms, self.lt_supp, ', %d episode line(s) over the quota' % self.lt_over if self.lt_over else '')
        line += ('; lock-on walk %s (LockOnWalk %s)' % self.lw) if self.lw else '; lock-on walk (no install line)'
        if self.lw_sets:
            line += ', %d change(s): %s' % (len(self.lw_sets), ', '.join('%s (%s) [%s]' % (v, frm, ts) for ts, v, frm in
                                                                       self.lw_sets[-3:]))
        if self.lw_first:
            line += ', first step below a full push [%s] (weapon 0x%s, %s)' % self.lw_first
        if self.lw_rooms:
            line += ', %d frame(s) raised in %d room(s) (%d locked on)' % (sum(x[0] for x in self.lw_rooms), len(self.lw_rooms),
                                                                         sum(x[1] for x in self.lw_rooms))
        if self.lt_bad:
            line += '; UNKNOWN value(s) %s' % ', '.join(self.lt_bad[-3:])
        if self.lt_not:
            line += '; NOT INSTALLED: %s' % ', '.join(self.lt_not)
        if self.lt_fault:
            line += '; FAULTED: %s' % ', '.join('%s [%s]' % (w, ts) for ts, w in self.lt_fault)
        return line

    def omni_line(self):
        """v3.46 OM346: the Stick Movement summary line (None without any of its lines). v3.48 OM348: "omni-directional movement"."""
        if not (self.om or self.om_not or self.om_sets or self.om_bind or self.om_checks or self.om_anom or self.om_row or
                self.om_bg[0] or self.om_again[0] or self.om_old or self.ol or self.ol_places):  # v3.47 CT347: + the row, the focus losses  # v3.48 OM348: + old values  # v3.49 LK349: + the look
            return None
        line = '  omni-directional movement: '
        if self.om:
            line += '%s at the start (%s, dead zone %.2f)' % self.om
        elif self.om_not:
            line += 'NOT installed (%s is not the stock code)' % self.om_not
        else:
            line += '(no install line)'
        if self.om_old:  # v3.48 OM348: a v3.46 / v3.47 value (Direction / Analog / 2) read as On
            line += '; old value(s): %s' % ', '.join('%s %s [%s]' % (v, how, ts) for ts, v, how in self.om_old[-3:])
        if self.om_sets:
            line += '; %d change(s): %s' % (len(self.om_sets), ', '.join('%s (%s) [%s]' % (new, frm, ts) for ts, new, old, frm
                                                                        in self.om_sets[-4:]))
        if self.om_dz:
            line += '; dead zone read again %.2f [%s]' % (self.om_dz[-1][1], self.om_dz[-1][0])
        if self.om_bind:
            ts, verdict, codes = self.om_bind[-1]
            line += '; bindings %s (%s) [%s]' % (verdict.upper() if verdict == 'not a stick' else verdict, codes, ts)
        if self.om_checks:
            ts, chk, d_ok, d_n, c_ok, c_n, src = self.om_checks[-1]
            line += '; self-check %d: source %s (GetDeviceState %d/%d, call sites %d/%d) [%s]' % (
                chk, src.upper() if src == 'none' else src, d_ok, d_n, c_ok, c_n, ts)
        elif self.om and self.om[1] != 'Off' or any(new != 'Off' for _, new, _, _ in self.om_sets):
            line += '; no self-check yet (the stick not used in gameplay?)'
        if self.om_rechecks:
            line += '; %d re-check(s) (last: %s %d/%d [%s])' % (len(self.om_rechecks), self.om_rechecks[-1][1],
                                                              self.om_rechecks[-1][2], self.om_rechecks[-1][3], self.om_rechecks[-1][0])
        if any(self.om_counts):
            line += '; written %d frame(s) (%d from the call sites), %d stopped inside the dead zone' % tuple(self.om_counts)
        if self.om_swap:
            line += '; swapped sticks without GetDeviceState: 8-way'
        if self.om_gate:
            line += '; pushed outside gameplay in %d state(s): %s' % (len(self.om_gate), '; '.join(self.om_gate[:4]))
        if self.om_row:  # v3.47 CT347
            line += '; the row %s' % self.om_row
        if self.om_pe:
            line += '; Playable Eileen %s' % self.om_pe
        if self.om_bg[0]:
            line += '; %d focus loss(es) (GetDeviceState failed while the game was not in front%s: not a defect)' % (
                self.om_bg[0], ', %d frame(s)' % self.om_bg[1] if self.om_bg[1] else '')
        if self.om_again[0]:
            line += '; the pad failed with the game in front %d time(s) (%d frame(s)), then read again' % tuple(self.om_again)
        if self.ol or self.ol_sets or self.ol_places or self.ol_check:  # v3.49 LK349: the analog look
            line += '; look: OmniLook %s' % (self.ol or '(no start line)')
            if self.ol_sets:
                line += ', read again %s [%s]' % (self.ol_sets[-1][1], self.ol_sets[-1][0])
            if self.ol_bad:
                line += ', UNKNOWN value(s) %s' % ', '.join(self.ol_bad[-3:])
            if self.ol_bind and self.ol_bind[1] == 'not a stick':
                line += ', bindings NOT A STICK (%s)' % self.ol_bind[2]
            if self.ol_check:
                line += ', self-check %s (%d/%d) [%s]' % ('on' if self.ol_check[3] == 'on' else 'NONE', self.ol_check[1],
                                                       self.ol_check[2], self.ol_check[0])
            if self.ol_places:
                n = self.ol_n
                line += (', %d place line(s): pushed %d frame(s), %d analog, %d the game\'s own (not here %d, key %d, no read %d, '
                         'check %d, not a stick %d); reversal latch %d frame(s) (x %d, y %d; Room 302 %d); 8-way misses %d; turn '
                         'kept %d' % (self.ol_places, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9], n[10],
                                      self.ol_room[1], n[11], n[12]))
        if self.om_anom:
            line += '; %d ANOMALY line(s)' % self.om_anom
        return line

    def parse_regen(self, ts, msg):
        """v3.47 RG347: one Regeneration line (or Life Steal's route, or the first start's seed); True when it was ours."""
        m = RG_IN.match(msg)
        if m:
            g = m.groups()
            self.rg = (g[0], g[1], float(g[2]), int(g[3]), g[4])
            return g[0] == 'in'  # (OFF: the warnings group keeps it too)
        m = RG_NOT.match(msg)
        if m:
            self.rg_not.append(m.group(1))
            return False
        m = RG_PE.match(msg)
        if m:
            self.rg_pe = m.group(1)
            return True
        m = RG_WHO.match(msg)
        if m:
            self.rg_who.append((ts, m.group(1), m.group(2), int(m.group(3))))
            return True
        m = RG_SET.match(msg)
        if m:
            self.rg_sets.append((ts, m.group(1), m.group(2), m.group(3)))
            return 'could not be saved' not in msg
        m = RG_REREAD.match(msg)
        if m:
            self.rg_sets.append((ts, m.group(1), m.group(2), '[Gameplay] read again'))
            return True
        m = RG_DELAY.match(msg)
        if m:
            self.rg_delay.append((ts, int(m.group(1))))
            return True
        m = RG_START.match(msg)
        if m:
            self.rg_runs[m.group(1)] += 1
            self.rg_open += 1
            return True
        m = RG_HURT.match(msg)
        if m:
            self.rg_stops['hurt'] += 1
            self.rg_hurt += float(m.group(1))
            self.rg_gain += float(m.group(4))
            self.rg_open = max(0, self.rg_open - 1)
            return True
        m = RG_STOP.match(msg)
        if m:
            self.rg_stops[m.group(1)] += 1
            self.rg_gain += float(m.group(4))
            self.rg_open = max(0, self.rg_open - 1)
            return True
        m = RG_SEED.match(msg)
        if m:
            self.rg_seed.append(m.groups())
            return True
        m = LS_ROUTE.match(msg)
        if m:
            self.ls_route.append((ts, 'game' if 'through' in m.group(1) else 'direct'))
            return True
        return False

    def parse_monsters(self, ts, msg):
        """v3.49 RZ349: one line of the Randomizer's Monsters; None = not one of them, else True when it was ours (no warning)."""
        m = RZ_KEYMIG.match(msg)  # v3.56 RE356: the renamed keys  # v3.56 MERGE: RZ_KEYMIG (RI356 has its own RZ_MIG)
        if m:
            self.rz_mig.append((m.group(1), m.group(2), m.group(3), m.group(4), m.group(6).startswith('written')))
            return m.group(6).startswith('written') and not m.group(5)
        m = RZM_STORY.match(msg)  # v3.56 RE356
        if m:
            self.rzm_story.append((int(m.group(1)), m.group(2)))
            return True
        m = SS_ROOM.match(msg)  # v3.56 RE356: Spawn Site's monsters
        if m:
            g = m.groups()
            r = self.ss_rooms
            r['rooms'] += 1
            r['moved'] += int(g[3])
            r['n'] += int(g[4])
            r['recs'] += int(g[5])
            r['cells'] += int(g[6])
            r['ms'] += float(g[7])
            r['maps'] += int(g[6]) > 0
            r['revisits'] += int(g[2]) > 1
            for part in (g[8] or '').split(', '):
                name, _, n = part.rpartition(' ')
                if name and n.isdigit():
                    self.ss_kept[name] += int(n)
            return True
        m = SS_NOMAP.match(msg)
        if m:
            self.ss_nomap.append((int(m.group(1)), int(m.group(2)), m.group(3)))
            return False
        m = SS_FN.match(msg)
        if m:
            self.ss_fn.append(m.group(1))
            return False
        m = SS_SESS.match(msg)
        if m:
            self.ss_sess = m.groups()
            return True
        m = RZM_IN.match(msg)
        if m:
            g = m.groups()
            self.rzm = (g[0], int(g[1]), int(g[2]), int(g[3]), int(g[4]), int(g[5]), g[6], g[7], g[8])
            return 'FAULTED' not in g[6] and 'not the stock' not in g[6] and not g[8].startswith('NOT')
        m = RZM_BADKEY.match(msg)
        if m:
            self.rzm_bad = m.groups()
            return False
        m = RZM_SET.match(msg)
        if m:
            self.rzm_sets.append((ts, m.group(1), m.group(2)))
            return True
        m = RZM_REREAD.match(msg)
        if m:
            self.rzm_sets.append((ts, m.group(1), '[Randomizer] read again'))
            return True
        m = RZM_GUM.match(msg)
        if m:
            self.rzm_gum = m.group(1)
            return True
        m = RZM_CT.match(msg)
        if m:
            self.rzm_ct.append((ts, m.group(1)))
            return True
        m = RZM_FIRST.match(msg)
        if m:
            self.rzm_first = (ts, int(m.group(1)), int(m.group(2)), m.group(3))
            return True
        m = RZM_ROOM.match(msg)
        if m:
            g = m.groups()
            r = self.rzm_rooms
            r['rooms'] += 1
            r['revisits'] += int(g[3]) > 1
            r['changed'] += int(g[4])
            r['slots'] += int(g[5])
            r['sticky'] += int(g[8])
            r['drops'] += int(g[9])
            r['bones'] += int(g[10])
            r['files'] += int(g[11])
            r['floor'] += int(g[12])
            r['other'] += bool(g[13])
            r['types'] += int(g[14])
            r['kb'] += int(g[15])
            if g[16] is not None:  # v3.50 RM350: the reasons (a v3.49 line has none)
                r['why_lines'] += 1
                for key, i in (('adopted', 16), ('adopt_changed', 17), ('roll_stock', 18), ('same', 19), ('away', 20),
                               ('other_rec', 21), ('none', 22)):
                    r[key] += int(g[i])
            if g[23] is not None:  # v3.51 SN351
                r['again'] += int(g[23])
            if g[24] is not None:  # v3.51 SN351
                r['ev_rooms'] += 1
                r['ev_set' if g[25] == 'set' else 'ev_clear'] += int(g[24])
            if g[26] is not None:  # v3.58 RZ358: the dead kept dead
                d = self.rzm_dead
                d['rooms'] += 1
                d['dead'] += int(g[26])
                d['hid'] += int(g[27])
                d['killed'] += int(g[28])
                d['lost'] += bool(g[29])
            for part in g[7].split(', '):
                name, _, n = part.rpartition(' ')
                if name and n.isdigit():
                    self.rzm_by[name] += int(n)
            return True
        m = RZM_DEADV.match(msg)  # v3.58 RZ358 (before RZM_LIST: the same start)
        if m:
            self.rzm_dead['v_hid'] += 1
            return True
        m = RZM_DEADLOST.match(msg)  # v3.58 RZ358: a body that stood again (no kill index): a warning
        if m:
            self.rzm_dead['v_lost'] += 1
            return False
        m = RZM_SAVEDEAD.match(msg)  # v3.58 RZ358
        if m:
            self.rzm_dead['saves'] += 1
            self.rzm_dead['save_dead'] += int(m.group(1))
            self.rzm_dead['save_lost'] += bool(m.group(2))
            return not m.group(2)
        m = RZM_COPIES.match(msg)  # v3.58 RZ358
        if m:
            g = m.groups()
            c = self.rzm_cp
            c['rooms'] += 1
            for key, i in (('made', 3), ('drawn', 5), ('sited', 6), ('files', 7), ('not', 8)):
                c[key] += int(g[i])
            for part in g[4].split(', '):
                name, _, n = part.rpartition(' ')
                if name and n.isdigit():
                    self.rzm_cp_by[name] += int(n)
            return True
        m = RZM_CPFILES.match(msg)  # v3.58 RZ358
        if m:
            self.rzm_cp['files_lines'] += 1
            return True
        m = RZM_SESS5.match(msg)  # v3.58 RZ358
        if m:
            self.rzm_sess5 = tuple(int(x) for x in m.groups())
            return not (self.rzm_sess5[3] or self.rzm_sess5[5])
        m = RZM_SESS6.match(msg)  # v3.58 RZ358
        if m:
            self.rzm_sess6 = tuple(int(x) for x in m.groups())
            return True
        if RZM_LIST.match(msg):
            self.rzm_lists += 1
            return True
        m = RZM_OTHER.match(msg)  # v3.49 fix2
        if m:
            self.rzm_misc['other'] += 1
            self.rzm_misc['other_n'] += int(m.group(3))
            return True
        m = RZM_EVIN.match(msg)  # v3.51 SN351
        if m:
            self.rzm_ev = (int(m.group(1)), int(m.group(2)), m.group(3))
            return True
        m = RZM_SESS4.match(msg)  # v3.51 SN351
        if m:
            self.rzm_sess4 = (int(m.group(1)), m.group(2), int(m.group(3)), int(m.group(4)))
            return True
        m = RZM_SESS3.match(msg)  # v3.50 RM350
        if m:
            self.rzm_sess3 = tuple(int(x) for x in m.groups())
            return True
        m = RZM_SESS2.match(msg)
        if m:
            self.rzm_misc['sess_other'] = int(m.group(1))
            self.rzm_misc['prov_full'] = int(m.group(3))
            return True
        for key, rx in (('hit', RZM_HIT), ('offdrop', RZM_OFFDROP), ('left', RZM_LEFT), ('savev', RZM_SAVEV)):
            if rx.match(msg):
                self.rzm_misc[key] += 1
                return True
        m = RZM_SAVE.match(msg)
        if m:
            self.rzm_misc['save'] += 1
            self.rzm_misc['saved_out'] += int(m.group(1))
            return True
        m = RZM_FILES.match(msg)
        if m:
            self.rzm_misc['files'] += 1
            return True
        m = RZM_SILENT.match(msg)
        if m:
            self.rzm_silent.append((m.group(1), int(m.group(2))))
            return True
        m = RZM_SESS.match(msg)
        if m:
            self.rzm_sess = m.groups()
            return True
        return None

    def monsters_line(self):
        """v3.49 RZ349: the Monsters summary line (None without any of its lines)."""
        if not (self.rzm or self.rzm_first or self.rzm_rooms or self.rzm_sets or self.rzm_sess or self.rzm_anom or self.rzm_ct):
            return None
        line = '  randomizer monsters: '
        if self.rzm:
            r = self.rzm
            line += '%s at the start; self-check %d of %d source placements (%d enemy placements) in %d scripts (%s); save filter %s' % (
                r[0], r[1], r[4], r[2], r[3], r[6], r[8])
            if self.rzm_ev:  # v3.51 SN351
                line += '; event dogs %d of %d stock%s (rolled once story flag 130 is set), roll again %s' % (
                    self.rzm_ev[0], self.rzm_ev[1], '' if self.rzm_ev[0] == self.rzm_ev[1] else ' (NOT the stock count)', self.rzm_ev[2])
        else:
            line += '(no start line)'
        if self.rzm_bad:
            line += '; unknown ini value: Monsters "%s" -> %s' % self.rzm_bad
        if self.rzm_gum:
            line += '; Gum Heads %s' % self.rzm_gum
        if self.rzm_misc['hit']:
            line += '; the STOCK hit-shape pool: Monsters stayed as placed'
        if self.rzm_ct:
            line += '; the cheat table\'s Enemy Randomizer: %s' % ', '.join('%s [%s]' % (w, ts) for ts, w in self.rzm_ct[-4:])
        if self.rzm_first:
            line += '; first room %d:%d with Monsters %s [%s]' % (self.rzm_first[1], self.rzm_first[2], self.rzm_first[3], self.rzm_first[0])
        if self.rzm_rooms:
            r = self.rzm_rooms
            line += ('; %d room load(s) randomized (%d of them a room visited again), %d of %d placement(s) changed (made as: %s), %d kept their '
                     'record\'s type, %d record(s) deleted; kept as placed: %d bones, %d files, %d no floor; files loaded for %d type(s) '
                     '(%d KB)' % (r['rooms'], r['revisits'], r['changed'], r['slots'],
                                  ', '.join('%s %d' % kv for kv in sorted(self.rzm_by.items(), key=lambda kv: -kv[1])) or 'none',
                                  r['sticky'], r['drops'], r['bones'], r['files'], r['floor'], r['types'], r['kb']))
            if r['other']:
                line += ', %d room(s) with words NOT the stock value' % r['other']
            if r['why_lines']:  # v3.50 RM350
                line += ("; the game's own records: %d adopted (%d changed, %d kept); stock type kept: %d rolled it, %d by a stock record, "
                         "%d recorded elsewhere, %d for another writer's record, %d with no target" % (
                             r['adopted'], r['adopt_changed'], r['adopted'] - r['adopt_changed'], r['roll_stock'], r['same'], r['away'],
                             r['other_rec'], r['none']))
            if r['again']:  # v3.51 SN351
                line += '; %d rolled again for their own type' % r['again']
            if r['ev_rooms']:  # v3.51 SN351
                line += '; event dogs in %d room load(s): %d rolled (story flag 130 set), %d kept as placed (flag clear)' % (
                    r['ev_rooms'], r['ev_set'], r['ev_clear'])
        if self.rzm_lists:
            line += ' (%d verbose list(s))' % self.rzm_lists
        if self.rzm_misc['files']:
            line += '; %d file load(s) refused' % self.rzm_misc['files']
        if self.rzm_misc['other']:  # v3.49 fix2
            line += "; %d room(s) with %d record(s) another writer retyped, left alone" % (self.rzm_misc['other'],
                                                                                         self.rzm_misc['other_n'])
        if self.rzm_misc['prov_full']:
            line += '; the provenance table was FULL %d time(s)' % self.rzm_misc['prov_full']
        if self.rzm_misc['offdrop']:
            line += '; %d room(s) with records of earlier randomized monsters deleted' % self.rzm_misc['offdrop']
        if self.rzm_silent:
            line += '; may be silent: ' + ', '.join('%s in stage %d' % s for s in self.rzm_silent)
        if self.rzm_misc['save']:
            line += '; %d save(s) left %d randomized record(s) out' % (self.rzm_misc['save'], self.rzm_misc['saved_out'])
        if self.rzm_sets:
            line += '; %d change(s): %s' % (len(self.rzm_sets), ', '.join('Monsters %s (%s) [%s]' % (new, frm, ts) for ts, new, frm
                                                                         in self.rzm_sets[-4:]))
        if self.rzm_sess:
            s = self.rzm_sess
            line += ('; session: %s room load(s), %s randomized, %s changed, %s by record, %s record(s) deleted (+%s off), kept %s / %s / '
                     '%s, %s save(s) %s out, %s left as found%s' % (s[3], s[4], s[5], s[6], s[7], s[8], s[9], s[10], s[11], s[15],
                                                                   s[16], s[17], s[18]))
        if self.rzm_sess3:  # v3.50 RM350
            line += '; session records: %d adopted (%d changed)' % self.rzm_sess3[:2]
        if self.rzm_sess4:  # v3.51 SN351
            line += '; session: %d rolled again (roll again %s), event dogs %d planned / %d kept asleep' % self.rzm_sess4
        d = self.rzm_dead  # v3.58 RZ358: a reroll never brings the dead back
        if d['rooms'] or d['saves'] or d['v_lost'] or self.rzm_sess5:
            line += '; the dead stayed dead in %d room load(s): %d bod(ies) as they died (%d with the placement empty), %d by a kill bit' % (
                d['rooms'], d['dead'], d['hid'], d['killed'])
            if d['lost'] or d['v_lost']:
                line += ', %d BACK (no kill index)' % max(d['lost'], d['v_lost'])
            if d['saves']:
                line += '; %d save(s) kept %d randomized bod(ies) dead as kill bits%s' % (d['saves'], d['save_dead'],
                                                                                     ' (SOME lost)' if d['save_lost'] else '')
        c = self.rzm_cp  # v3.58 RZ358: Extra Enemies' copies drawn
        if c['rooms'] or self.rzm_sess6:
            line += '; copies drawn in %d room load(s): %d made (as %s), %d drawn (%d at a site of their own), %d kept their original\'s type (files), %d not made' % (
                c['rooms'], c['made'], ', '.join('%s %d' % kv for kv in sorted(self.rzm_cp_by.items(), key=lambda kv: -kv[1])) or 'none',
                c['drawn'], c['sited'], c['files'], c['not'])

        if self.rzm_anom:
            line += '; %d ANOMALY line(s)' % self.rzm_anom
        return line

    def spawnsite_line(self):
        """v3.56 RE356: the Spawn Site summary line (None without any of its lines) and the renamed keys."""
        if not (self.ss_rooms or self.ss_sess or self.ss_nomap or self.ss_fn or self.ss_anom or self.rz_mig or self.rzm_story):
            return None
        line = '  randomizer spawn site: '
        parts = []
        if self.rz_mig:
            parts.append('ini migrated: ' + ', '.join('%s "%s" -> %s %s%s' % (o, t, n, v, '' if w else ' (NOT written)')
                                                     for o, t, n, v, w in self.rz_mig))
        if self.rzm_story:
            parts.append('story rule: ' + '; '.join('stage %d: %s' % s for s in self.rzm_story[-4:]))
        r = self.ss_rooms
        if r:
            parts.append('%d room load(s) with monsters (%d of them a room visited again), %d of %d monster(s) at a new site, %d '
                         'record(s) moved; %d floor map(s), %d cell(s), %.1f ms in all' % (r['rooms'], r['revisits'], r['moved'], r['n'],
                                                                                           r['recs'], r['maps'], r['cells'], r['ms']))
            if self.ss_kept:
                parts.append('kept or refused: ' + ', '.join('%s %d' % kv for kv in sorted(self.ss_kept.items(), key=lambda kv: -kv[1])))
        if self.ss_nomap:
            parts.append('NO floor map: ' + ', '.join('%d:%d (%s)' % x for x in self.ss_nomap[-3:]))
        if self.ss_fn:
            parts.append('game code NOT stock at ' + ', '.join(self.ss_fn) + ' (off)')
        if self.ss_sess:
            s = self.ss_sess
            parts.append('session: %s, %s room load(s), %s moved, %s kept, %s record(s), %s map(s) (%s cells, %s ms), %s without a map, '
                         '%s ghost(s) in the box%s' % s)
        if self.ss_anom:
            parts.append('%d ANOMALY line(s)' % self.ss_anom)
        return line + '; '.join(parts)

    def parse_rando(self, ts, msg):
        """v3.48 RZ348: one Randomizer line; True when it was ours."""
        r = self.parse_monsters(ts, msg)  # v3.49 RZ349
        if r is not None:
            return r
        m = RZ_IN.match(msg)
        if m:
            g = m.groups()  # v3.48 RZ348 r2: + PerVisit, weapon spots, the weapons that may move
            # v3.56 RI356: the key names are groups 1 and 4 now; the tuple keeps v3.48's order (keep / specials, room 302, ...)
            self.rz = (g[0], g[2], g[3], int(g[6]), int(g[7]), int(g[9]), int(g[10]), g[12], g[5], int(g[8]), int(g[11]), int(g[13]))
            self.rz_v356 = g[1] == 'Specials'
            return 'FAULTED' not in g[12] and 'not the stock' not in g[12]
        m = RZ_MIG.match(msg)  # v3.56 RI356
        if m:
            self.rz_migs.append((ts,) + m.groups()[:4] + (m.group(6),))
            return m.group(6) == 'migrated once'
        m = RZ_SEEDLINE.match(msg)  # v3.56 RI356
        if m:
            self.rz_seedline = (int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5))
            return True
        m = RZ_ROLL.match(msg)
        if m:
            self.rz_roll = (m.group(1), int(m.group(2)), m.group(3) == 'and written')
            return True
        m = RZ_BADKEY.match(msg)
        if m:
            self.rz_bad.append(m.groups())
            return False  # (kept in the warnings too)
        m = RZ_NOMODELS.match(msg)  # v3.58 MN358
        if m:
            if int(m.group(1)) not in self.rz_nomodels:
                self.rz_nomodels.append(int(m.group(1)))
            return True
        m = RZ_FIRST.match(msg)
        if m:
            self.rz_first = (ts, int(m.group(1)), int(m.group(2)), m.group(3))
            return True
        m = RZ_PLAN.match(msg)
        if m:
            g = m.groups()  # (ts, stage, visit, mode, pooled, n, changed, seed, candles, medallions, swords, swords stock,
            #                    weapons, weapons stock, weapon spots, Change Each Visit, world visit, unplaced?)
            self.rz_plans.append((ts, int(g[0]), int(g[1]), g[2], int(g[3]), int(g[4]), int(g[6]), int(g[7]), int(g[12]),
                                  int(g[14]), int(g[16]), int(g[17]), int(g[18]), int(g[19]), int(g[5]), g[10], int(g[11]),
                                  bool(g[21])))
            return True
        m = RZ_ROOM.match(msg)
        if m:
            self.rz_rooms['pooled'] += 1
            self.rz_rooms['changed'] += int(m.group(5))
            self.rz_rooms['slots'] += int(m.group(6))
            self.rz_rooms['weapons'] += int(m.group(7))
            self.rz_rooms['revisits'] += int(m.group(4)) > 1
            if m.group(9):
                self.rz_rooms['other'] += 1
            return True
        if RZ_LIST.match(msg):
            self.rz_lists += 1
            return True
        m = RZ_SET.match(msg)
        if m:
            self.rz_sets.append((ts, m.group(1), m.group(2), m.group(3)))
            return True
        m = RZ_SEEDSET.match(msg)
        if m:
            self.rz_sets.append((ts, 'Seed', m.group(2), m.group(1) + ('' if m.group(4) == 'saved' else ', NOT saved')))
            return m.group(4) == 'saved'
        m = RZ_REREAD.match(msg)
        if m:
            self.rz_sets.append((ts, m.group(1), m.group(2), '[Randomizer] read again'))
            return True
        m = RZ_SESS.match(msg)
        if m:
            self.rz_sess = m.groups()
            return True
        return False

    def parse_visits(self, ts, msg):
        """v3.56 RI356: the world-visit rule's lines (worldvisit.inc). True = consumed."""
        m = WV_WORLD.match(msg)
        if m:
            self.wv_worlds.append((ts, m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4)), bool(m.group(5))))
            if 2 <= int(m.group(2)) <= 8:  # v3.56 MERGE: a world entry for Enemies Return's summary too (one rule, one line)
                self.er['entries'] += 1
            return True
        m = WV_LOAD.match(msg)
        if m:
            self.wv_loads.append((ts, int(m.group(1)), int(m.group(2)), m.group(3), m.group(4), m.group(5), int(m.group(6))))
            self.er['loads'] += 1  # v3.56 MERGE: a load's room for Enemies Return's summary too
            return True
        m = WV_SESS.match(msg)
        if m:
            self.wv_sess = tuple(int(x) for x in m.groups())
            return True
        return False

    def visits_line(self):
        """v3.56 RI356: the world-visit summary line (None without any of its lines)."""
        if not (self.wv_worlds or self.wv_loads or self.wv_sess):
            return None
        line = '  visits: %d world visit(s) begun' % len(self.wv_worlds)
        if self.wv_worlds:
            line += ' (stages %s)' % ', '.join(str(w[2]) for w in self.wv_worlds[-6:])
            resets = sum(1 for w in self.wv_worlds if w[5])
            if resets:
                line += ', %d after a New Game / title load' % resets
        if self.wv_loads:
            how = Counter(l[5] for l in self.wv_loads)
            sig = Counter(l[3] for l in self.wv_loads)
            line += '; %d load(s), never a room or world entry (%s; marked by %s)' % (
                len(self.wv_loads), ', '.join('%d %s' % (n, k) for k, n in sorted(how.items())),
                ', '.join('%s %d' % (k, n) for k, n in sorted(sig.items())))
        if self.wv_sess:
            v = self.wv_sess
            line += '; session: %d room entries, %d world visit(s), %d load(s) (%d resumed, %d the seed\'s own), %d reset(s), %d room 0' % v
        return line

    def parse_rzl(self, ts, msg):
        """v3.49 RZ349L: one Randomizer looks line: True (ours), False (ours, kept for the warnings), None (not ours)."""
        m = RZL_IN.match(msg)
        if m:
            self.rzl = m.groups()
            return True
        m = RZL_BADKEY.match(msg)
        if m:
            self.rzl_bad.append(m.groups())
            return False
        m = RZL_FIRST.match(msg)
        if m:
            g = m.groups()
            self.rzl_first = (ts, int(g[0]), int(g[1]), g[2], g[3], g[4], g[5])
            return 'NOT available' not in g[3] and 'NOT available' not in g[5]
        m = RZL_DRAW.match(msg)
        if m:
            g = m.groups()
            self.rzl_out.append((ts, 'draw', '%s (visit %s, stage %s)' % (g[4], g[0], g[1])))
            return True
        m = RZL_SINGLE.match(msg)
        if m:
            self.rzl_out.append((ts, 'single', 'only the Dress unlocked (visit %s, stage %s)' % (m.group(1), m.group(2))))
            return True
        m = RZL_BACK.match(msg)
        if m:
            self.rzl_out.append((ts, 'back', '%s back (%s)' % m.groups()))
            return True
        m = RZL_TAKE.match(msg)
        if m:
            self.rzl_out.append((ts, 'takeover', '%s chosen in the Extra Menu' % m.group(1)))
            return True
        m = RZL_GROOM.match(msg)
        if m:
            self.rzl_gh['rooms'] += 1
            self.rzl_gh['moved'] += int(m.group(4))
            self.rzl_gh['victims'] += int(m.group(5))
            self.rzl_gh['revisits'] += int(m.group(3)) > 1
            for v in RZL_GMOVE.finditer(m.group(8)):  # v3.50 RL350 (v3.49 lines have no distance: nothing counted)
                d = int(v.group(4))
                self.rzl_gv['near' if v.group(5) else 'far'] += 1
                self.rzl_gv['recmoved'] += v.group(8) == 'moved'
                self.rzl_gv['dmax'] = max(self.rzl_gv['dmax'], d)
                self.rzl_gv['dsum'] += d
            self.rzl_gv['reckept'] += len(RZL_GKEPT.findall(m.group(8)))
            return True
        m = RZL_RDRAW.match(msg)  # v3.50 RL350
        if m:
            self.rzl_room['draws'] += 1
            self.rzl_room['changed'] += m.group(5) != m.group(6)
            return True
        m = RZL_HELD.match(msg)  # v3.50 RL350
        if m:
            self.rzl_room['held'] += 1
            return True
        if RZL_CT.match(msg):
            self.rzl_gh['ct'] += 1
            return True
        m = RZL_SET.match(msg)
        if m:
            self.rzl_sets.append((ts, m.group(1), m.group(2), m.group(3)))
            return True
        m = RZL_REREAD.match(msg)
        if m:
            self.rzl_sets.append((ts, m.group(1), m.group(2), '[Randomizer] read again'))
            return True
        m = RZL_SESS2.match(msg)  # v3.50 RL350
        if m:
            self.rzl_sess2 = m.groups()
            return 'FAULTED' not in m.group(10) and 'FAULTED' not in m.group(20)
        m = RZL_SESS.match(msg)
        if m:
            self.rzl_sess = m.groups()
            return 'FAULTED' not in m.group(9) and 'FAULTED' not in m.group(16)
        return None

    def rzl_line(self):
        """v3.49 RZ349L: the Randomizer looks summary line (None without any of its lines)."""
        if not (self.rzl or self.rzl_bad or self.rzl_first or self.rzl_out or self.rzl_gh or self.rzl_sets or self.rzl_sess or
                self.rzl_anom or self.rzl_room or self.rzl_sess2):  # v3.50 RL350: + rzl_room, rzl_sess2
            return None
        line = '  randomizer looks: '
        line += ('Eileen Outfit %s, Ghosts %s at the start' % self.rzl) if self.rzl else '(no start line)'
        if self.rzl_bad:
            line += '; unknown ini values: ' + ', '.join('%s "%s" -> %s' % b for b in self.rzl_bad)
        if self.rzl_first:
            f = self.rzl_first
            line += '; first room load %d:%d: outfit %s (%s), ghosts %s (%s) [%s]' % (f[1], f[2], f[3], f[4], f[5], f[6], f[0])
        if self.rzl_out:
            n = Counter(k for _, k, _ in self.rzl_out)
            line += '; outfit: %d draw(s), %d with one outfit, %d back to the set one, %d Extra Menu choice(s); last: %s' % (
                n['draw'], n['single'], n['back'], n['takeover'], ', '.join('%s [%s]' % (d, t) for t, _, d in self.rzl_out[-3:]))
        if self.rzl_room:  # v3.50 RL350
            q = self.rzl_room
            line += '; outfit room draws: %d (%d changed her outfit)' % (q['draws'], q['changed'])
            if q['held']:
                line += ', %d held in the final fight' % q['held']
        if self.rzl_gh['rooms']:
            g = self.rzl_gh
            line += '; ghosts: %d room load(s) with victims (%d of them a room visited again), %d of %d victim(s) moved' % (
                g['rooms'], g['revisits'], g['moved'], g['victims'])
            v = self.rzl_gv  # v3.50 RL350
            if v['far'] + v['near']:
                line += ' (%d 800 or more away, %d nearer: the room allowed no farther; mean %d, most %d; %d record(s) moved)' % (
                    v['far'], v['near'], v['dsum'] // (v['far'] + v['near']), v['dmax'], v['recmoved'])
            if v['reckept']:
                line += ', %d kept by a pinned or down record' % v['reckept']
        if self.rzl_gh['ct']:
            line += "; ghosts left alone: the cheat table's Enemy Randomizer is on"
        if self.rzl_sets:
            line += '; %d change(s): %s' % (len(self.rzl_sets), ', '.join('%s %s (%s) [%s]' % (k, new, frm, ts) for ts, k, new, frm
                                                                         in self.rzl_sets[-4:]))
        if self.rzl_sess:
            s = self.rzl_sess
            line += ('; session: outfit %s world visit(s), %s draw(s) (Dress %s, Nurse %s), %s back, %s chosen%s; ghosts %s room '
                     'load(s), %s victim(s), %s moved, %s kept, %s left as found%s' % (
                         s[1], s[2], s[3], s[4], s[6], s[7], s[8], s[10], s[11], s[12], s[13], s[14], s[15]))
        if self.rzl_sess2:  # v3.50 RL350
            s = self.rzl_sess2
            line += ('; session: outfit %s world visit(s), %s world draw(s), %s room draw(s) (%s changed), Dress %s / Nurse %s, %s back, '
                     '%s chosen%s; ghosts %s room load(s), %s victim(s), %s moved (%s at 800 +, %s record(s) moved), %s kept (%s by a '
                     'record), %s left as found%s' % (s[1], s[2], s[5], s[6], s[3], s[4], s[7], s[8], s[9], s[11], s[12], s[13], s[14],
                                                      s[15], s[16], s[17], s[18], s[19]))
        if self.rzl_anom:
            line += '; %d ANOMALY line(s)' % self.rzl_anom
        return line

    def rando_line(self):
        """v3.48 RZ348: the Randomizer summary line (None without any of its lines)."""
        if not (self.rz or self.rz_first or self.rz_plans or self.rz_rooms or self.rz_sets or self.rz_sess or self.rz_anom or
                self.rz_migs or self.rz_seedline or self.rz_nomodels):  # v3.56 RI356  # v3.58 MN358
            return None
        line = '  randomizer: '
        if self.rz:
            r = self.rz
            line += ('Items %s (%s %s, %s %s, %s %s, seed %d) at the start; self-check %d of %d plain '
                     'pickups and %d of %d weapon spots in %d scripts (%s); %d weapons may move' % (
                         r[0], 'Specials' if self.rz_v356 else 'Keep Specials', r[1],
                         'Room 302 Items' if self.rz_v356 else 'Room 302', r[2],
                         'Reroll' if self.rz_v356 else 'Change Each Visit', r[8],  # v3.56 RI356: the new names
                         r[3], r[4], r[6], r[9], r[10], r[5], r[7], r[11]))
        else:
            line += '(no start line)'
        if self.rz_roll:
            line += '; seed rolled %d (%s)' % (self.rz_roll[1], 'written' if self.rz_roll[2] else 'NOT written')
        if self.rz_migs:  # v3.56 RI356
            line += '; v3.55 keys: ' + ', '.join('%s=%s -> %s=%s (%s)' % m[1:] for m in self.rz_migs)
        if self.rz_seedline:  # v3.56 RI356
            line += '; seed line %d glyph(s) at y %d (%s)' % (self.rz_seedline[3], self.rz_seedline[2],
                                                           'small' if self.rz_seedline[4] == 'the smaller size' else 'FULL SIZE')
        if self.rz_bad:
            line += '; unknown ini values: ' + ', '.join('%s "%s" -> %s' % b for b in self.rz_bad)
        if self.rz_first:
            line += '; first room load %d:%d with Items %s [%s]' % (self.rz_first[1], self.rz_first[2], self.rz_first[3], self.rz_first[0])
        if self.rz_plans:
            few = [p for p in self.rz_plans if p[10] < p[11]]
            line += '; %d plan(s): %s' % (len(self.rz_plans), ', '.join('%d/%d %s %d of %d changed, %d weapon(s)' % (
                p[1], p[2], p[3], p[6], p[4], p[12]) for p in self.rz_plans[-4:]))
            if few:
                line += '; fewer Swords than the stock in %d plan(s)' % len(few)
            bad = [p for p in self.rz_plans if p[17]]
            if bad:
                line += '; %d plan(s) COULD NOT PLACE every item' % len(bad)
        if self.rz_rooms:
            line += '; %d room load(s) with pickups in the pool (%d of them a room visited again), %d of %d pickup(s) changed, %d weapon(s)' % (
                self.rz_rooms['pooled'], self.rz_rooms['revisits'], self.rz_rooms['changed'], self.rz_rooms['slots'],
                self.rz_rooms['weapons'])
            if self.rz_rooms['other']:
                line += ', %d room(s) with words NOT the stock item' % self.rz_rooms['other']
        if self.rz_lists:
            line += ' (%d verbose list(s))' % self.rz_lists
        if self.rz_sets:
            line += '; %d change(s): %s' % (len(self.rz_sets), ', '.join('%s %s (%s) [%s]' % (k, new, frm, ts) for ts, k, new, frm
                                                                        in self.rz_sets[-4:]))
        if self.rz_sess:
            s = self.rz_sess
            line += '; session: %s room load(s), %s pooled, %s changed, %s weapon(s), %s plan(s), %s left as found%s' % (
                s[5], s[6], s[7], s[8], s[9], s[10], s[11])
        if self.rz_nomodels:  # v3.58 MN358
            line += '; stage(s) %s left alone by design (no pickup model files in the world%s)' % (
                ', '.join(str(x) for x in self.rz_nomodels),
                '; written as %d ANOMALY line(s) before v3.58, not counted' % self.rz_nomodels_anom if self.rz_nomodels_anom else '')
        if self.rz_anom:
            line += '; %d ANOMALY line(s)' % self.rz_anom
        return line

    def vbx_line(self):
        """v3.58 MN358: the Options value box (None without its lines)."""
        if not self.vbx and not self.vbx_pages:
            return None
        if not self.vbx:
            line = '  options value box (v3.58): (no install line)'
        elif self.vbx[0] == 'on':
            line = '  options value box (v3.58): on%s' % ('' if self.vbx[1] else '; the volume bars\' arrows NOT moved%s' % (self.vbx[2] or ''))
        else:
            line = '  options value box (v3.58): %s' % self.vbx[1]
        if self.vbx_pages:
            grown = [p for p, v in self.vbx_pages.items() if v[7] != 'the stock places']
            line += '; %d page(s) measured, %d at the stock arrows, %d grown: %s' % (
                len(self.vbx_pages), len(self.vbx_pages) - len(grown), len(grown),
                ', '.join('%s %d / %d' % (self.vbx_pages[p][0], self.vbx_pages[p][5], self.vbx_pages[p][6]) for p in grown[:8]))
            tight = [self.vbx_pages[p][0] for p, v in self.vbx_pages.items() if v[8]]
            if tight:
                line += '; a value wider than the room on %s' % ', '.join(tight)
        if self.vbx_grow:
            line += '; %d page(s) grew for a wider value' % len(self.vbx_grow)
        return line

    def parse_r302(self, ts, msg):
        """v3.50 R302: room302.inc's lines. True = consumed."""
        m = R3_OWN.match(msg)
        if m:
            self.r3_own.append((ts,) + m.groups()[:8] + (bool(m.group(9)),))
            return True
        m = R3_HOOK.match(msg)
        if m:
            self.r3_hook = (ts, m.group(1))
            return True
        if R3_FAIL.match(msg):
            self.r3_fail += 1
            return True
        m = R3_VISIT.match(msg)
        if m:
            self.r3_visits += 1
            for i, g in enumerate((1, 3, 4, 5, 6, 7, 8, 10)):
                self.r3_n[i] += int(m.group(g))
            return True
        m = R3_SET.match(msg)
        if m:
            self.r3_sets.append((ts,) + m.groups())
            return True
        m = R3_SEED.match(msg)
        if m:
            self.r3_seed.append((m.group(1), m.group(2), m.group(3) or 'the default'))
            return True
        m = R3_BAD.match(msg)
        if m:
            self.r3_bad.append(m.groups())
            return True
        m = R3_NOPAGE.match(msg)
        if m:
            self.r3_nopage = m.group(1)
            return True
        if R3_REBUILT.match(msg):
            self.r3_rebuilt += 1
            return True
        return False

    def parse_frg(self, ts, msg):
        """v3.56 FR356: the rate guard's and the final-room sites' lines. True = consumed."""
        m = FRG_ON.match(msg)
        if m:
            t = FRG_TOP.search(msg)
            self.frg = (m.group(1), int(t.group(1)) if t else None, ts)
            return True
        m = FRG_DOWN.match(msg)
        if m:
            g = m.groups()
            self.frg_downs.append((ts, int(g[0]), int(g[1]), float(g[2]), float(g[3]), float(g[4]), g[8], g[9], int(g[10]), bool(g[11]),
                                   g[7] is not None))  # v3.58 GX358: + the quick window
            return True
        m = FRG_UP.match(msg)
        if m:
            self.frg_ups.append((ts, int(m.group(1)), int(m.group(2)), m.group(3), m.group(4)))
            return True
        m = FRG_RETRY.match(msg)
        if m:
            self.frg_retry.append((ts, int(m.group(1)), int(m.group(2)), int(m.group(3))))
            return True
        m = FRG_MISS.match(msg)
        if m:
            self.frg_miss[1 if m.group(6) else 0] += 1
            w = (float(m.group(5)), int(m.group(2)), float(m.group(3)), float(m.group(4)), ts)
            if self.frg_worst is None or w[0] > self.frg_worst[0]:
                self.frg_worst = w
            return True
        m = FRG_RESET.match(msg)
        if m:
            self.frg_resets.append((ts, int(m.group(1)), int(m.group(2))))
            return True
        m = FRG_VSYNC.match(msg)
        if m:
            self.frg_resets.append(('vsync', ts, int(m.group(1)), int(m.group(2))))
            return True
        m = T60F_IN.match(msg)
        if m:
            self.t60f = (int(m.group(1)), int(m.group(2)))
            return True
        m = T60F_BAD.match(msg)
        if m:
            self.t60f = ('NOT', m.group(1))
            return False  # also a warning line (the "is not stock" group)
        return False

    def parse_cr(self, ts, msg):
        """v3.59 FR359: the cut-scene / Room 302 view rate lines. True = consumed."""
        m = CR_IN.match(msg)
        if m:
            g = m.groups()
            self.cr = tuple('fast' if x == 'at the gameplay rate' else '30' for x in g[:3]) + (int(g[3]), int(g[4]))
            return True
        if CR_OFF.match(msg):
            self.cr = 'off'
            return True
        m = CR_START.match(msg)
        if m:
            self.cr_starts[m.group(2)] = self.cr_starts.get(m.group(2), 0) + 1
            return True
        m = CR_END.match(msg)
        if m:
            g = m.groups()
            self.cr_ends.append((ts, g[0], int(g[1]), float(g[2]), int(g[3]), int(g[4]), int(g[5])))
            return True
        m = CR_CUT.match(msg)
        if m:
            self.cr_cuts[0 if m.group(4) == 'a camera cut' else 1] += 1
            return True
        return False

    def cr_line(self):
        """v3.59 FR359: the cut-scene / view rate summary (None without any of its lines)."""
        if not (self.cr or self.cr_starts or self.cr_ends):
            return None
        line = '  cut-scene rate: '
        if self.cr == 'off':
            line += 'cut-scenes and views at 30 (CutsceneRate=0, ViewRate=0)'
        elif self.cr:
            line += 'cut-scenes %s, window view %s, peephole / hole views %s (window view sites %d, caves %d)' % self.cr
        else:
            line += '(no start line)'
        if self.cr_starts:
            line += '; at the gameplay rate: %s' % ', '.join('%s %d' % (k, v) for k, v in sorted(self.cr_starts.items()))
        if self.cr_ends:
            cam = sum(e[4] for e in self.cr_ends)
            act = sum(e[5] for e in self.cr_ends)
            worst = max(self.cr_ends, key=lambda e: abs(e[6]))
            line += '; %d ended, %d camera cut(s) and %d actor jump(s) held, the stream at most %+d ms from real time (%s at %d, %.1f s) [%s]' % (
                len(self.cr_ends), cam, act, worst[6], worst[1], worst[2], worst[3], worst[0])
            far = [e for e in self.cr_ends if abs(e[6]) > CR_DRIFT_MS]
            if far:
                line += '; CHECK: %d stretch(es) over %d ms off real time (lip-sync)' % (len(far), CR_DRIFT_MS)
        if any(self.cr_cuts):
            line += '; [v] %d camera cut / %d actor line(s)' % tuple(self.cr_cuts)
        return line

    def frg_line(self):
        """v3.56 FR356: the rate guard's summary line (None without any of its lines)."""
        if not (self.frg or self.frg_downs or self.frg_ups or self.frg_retry or any(self.frg_miss) or self.t60f):
            return None
        line = '  rate guard: '
        if self.frg:
            line += '%s at the start%s [%s]' % (self.frg[0], ' (top %d)' % self.frg[1] if self.frg[1] else '', self.frg[2])
        else:
            line += '(no start line)'
        if self.frg_downs:
            line += '; %d step(s) down: %s' % (len(self.frg_downs), ', '.join(
                '%d -> %d (%.2f ms against %.2f, %.2f x, stage %s room %s, budget level %d%s%s) [%s]' % (
                    f, t, mean, tgt, r, st, rm, lv, ', locked' if lk else '', ', the quick 1 s window' if q else '', ts)
                for ts, f, t, mean, tgt, r, st, rm, lv, lk, q in self.frg_downs[-4:]))
        else:
            line += '; no step down'
        if self.frg_retry:
            line += '; %d retry(ies) waited for a room change' % len(self.frg_retry)
        if self.frg_ups:
            line += '; %d step(s) back up: %s' % (len(self.frg_ups), ', '.join(
                '%d -> %d at %s:%s [%s]' % (f, t, st, rm.zfill(2), ts) for ts, f, t, st, rm in self.frg_ups[-4:]))
        if any(self.frg_miss):
            line += '; missed 3 s windows: %d counted, %d held while the effect budget shed' % tuple(self.frg_miss)
            if self.frg_worst:
                line += ', the worst %.2f x at %d (%.2f ms against %.2f) [%s]' % self.frg_worst
        for x in self.frg_resets[-3:]:
            if x[0] == 'vsync':
                line += '; the V-Sync cap %d lifted the guard\'s %d [%s]' % (x[2], x[3], x[1])
            else:
                line += '; the Frame Rate row chose %d, the guard started again (it held %d) [%s]' % (x[1], x[2], x[0])
        if self.t60f:
            if self.t60f[0] == 'NOT':
                line += '; final-room sites NOT in (%s)' % self.t60f[1]
            else:
                line += '; final-room sites %d/%d' % self.t60f
        return line

    def r302_line(self):
        """v3.50 R302: the Room 302 summary line (None without any of its lines)."""
        if not (self.r3_own or self.r3_hook or self.r3_visits or self.r3_sets or self.r3_seed or self.r3_anom or self.r3_fail):
            return None
        line = '  room 302: '
        if self.r3_own:
            ts, when, look, walk, pe, d, u, f, w, wait = self.r3_own[-1]
            line += 'look %s, walk %s (%s) [%s]; Playable Eileen %s; LookDownLimit %s, LookUpLimit %s, LookFeel %s, WalkSpeed %s' % (
                look, walk, when, ts, pe.split(': ')[0], d, u, f, w)
            if wait:
                line += '; STILL WAITING (no first-tick decision in the log)'
        else:
            line += '(no owners line)'
        if self.r3_seed:
            line += '; first start: %d key(s) seeded (%s)' % (len(self.r3_seed), ', '.join(
                '%s %s' % (k, v) for k, v, _ in self.r3_seed))
            if any('SH4PlayableEileen.ini' in fr for _, _, fr in self.r3_seed):
                line += ' from Playable Eileen\'s ini'
        if self.r3_hook:
            line += '; look hooks in [%s]' % self.r3_hook[0]
        if self.r3_fail:
            line += '; look hooks NOT written (rolled back)'
        if self.r3_nopage:
            line += '; NO PAGE (%s)' % self.r3_nopage
        if self.r3_bad:
            line += '; ini value(s) off the rows: ' + ', '.join('%s=%s -> %s' % b for b in self.r3_bad[-4:])
        if self.r3_visits:
            n = self.r3_n
            line += ('; %d visit line(s), %d s: look %d tick(s) (eased %d, clamped %d: up %d, down %d; view rebuilt %d), walk %d '
                     'tick(s)' % (self.r3_visits, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]))
        if self.r3_sets:
            line += '; %d change(s): %s' % (len(self.r3_sets), ', '.join('%s %s -> %s (%s) [%s]' % (k, o, nw, why, ts)
                                                                        for ts, k, o, nw, why in self.r3_sets[-4:]))
        if self.r3_anom:
            line += '; %d ANOMALY line(s)' % self.r3_anom
        return line

    def regen_line(self):
        """v3.47 RG347: the Regeneration summary line (None without any of its lines)."""
        if not (self.rg or self.rg_not or self.rg_who or self.rg_sets or self.rg_runs or self.rg_anom or self.rg_seed):
            return None
        line = '  regeneration: '
        if self.rg:
            line += '%s at the start (Regen %s = %g %% a minute, RegenDelay %d s; who: %s)' % self.rg
        else:
            line += '(no install line)'
        if self.rg_not:
            line += '; NOT working: ' + ', '.join(self.rg_not)
        if self.rg_seed:
            line += '; first start: ' + ', '.join('%s = %s (%s)' % s for s in self.rg_seed)
        if self.rg_pe:
            line += '; Playable Eileen %s SH4PE_RegenActive%s' % (self.rg_pe, '' if self.rg_pe == 'with' else ' (v3.46 front end)')
        if self.rg_who:
            line += '; who heals changed %d time(s), last: %s [%s]' % (len(self.rg_who), self.rg_who[-1][1], self.rg_who[-1][0])
        if self.rg_sets:
            line += '; %d change(s): %s' % (len(self.rg_sets), ', '.join('%s (%s) [%s]' % (new, frm, ts) for ts, new, old, frm
                                                                        in self.rg_sets[-4:]))
        if self.rg_delay:
            line += '; RegenDelay read again %d s [%s]' % (self.rg_delay[-1][1], self.rg_delay[-1][0])
        if self.rg_runs:
            line += '; %d healing run(s) (%s), +%.1f HP in the ended ones' % (
                sum(self.rg_runs.values()), ', '.join('%s %d' % (k, v) for k, v in sorted(self.rg_runs.items())), self.rg_gain)
            if self.rg_stops:
                line += '; stopped by: ' + ', '.join('%s %d' % (k, v) for k, v in self.rg_stops.most_common())
            if self.rg_hurt:
                line += ' (hits %.1f HP)' % self.rg_hurt
            if self.rg_open:
                line += '; %d still running at the end of the log' % self.rg_open
        if self.ls_route:
            line += '; Life Steal heals %s [%s]' % ('through the game (PE in charge)' if self.ls_route[-1][1] == 'game' else
                                                 'directly', self.ls_route[-1][0])
        if self.rg_anom:
            line += '; %d ANOMALY line(s)' % self.rg_anom
        return line

    def parse(self, ts, msg):
        area = msg.split(':', 1)[0] if ':' in msg[:40] else ''
        self.kinds[norm(msg, 70)] += 1
        if msg.startswith('ANOMALY: '):
            body = msg[9:]
            m = RZ_NOMODELS.match(body)  # v3.58 MN358: by design (v3.57's ANOMALY form): the randomizer line says it
            if m:
                if int(m.group(1)) not in self.rz_nomodels:
                    self.rz_nomodels.append(int(m.group(1)))
                self.rz_nomodels_anom += 1
                return
            self.add_group(self.anomalies, norm(body), ts, body)
            if body.startswith('room 302: '):  # v3.50 R302
                self.r3_anom += 1
            if body.startswith('regeneration: '):  # v3.47 RG347
                self.rg_anom += 1
            if body.startswith('direct3d: '):  # v3.59 SD359: the Direct3D stop
                self.parse_fw(ts, body)
            if body.startswith('sound: '):  # v3.52 SND352
                self.snd_anom += 1
                if SD_KEEP_FAIL.match(body):  # v3.56 SD356
                    self.sd_keep = 'FAILED'
                m = SD_SESS_LOST.match(body)  # v3.58 SD358
                if m:
                    self.sd_sess_lost.append((ts, m.group(1)[:60]))
            if LKL_OVER.match(body):  # v3.53 OL2
                self.ol_over += 1
            if LYL_VB_FAIL.match(body):  # v3.55 OL56
                self.ly_vb = 'failed' 
            if body.startswith(('extra menu: the level music is not paused', "extra menu: the pause menu's sound was in mode")):  # v3.51 MU351
                self.mu_anom += 1
            if body.startswith('extra menu: Quick Load: '):  # v3.52 QL352
                self.ql_anom += 1
            if body.startswith(('screen effects: the clean pause picture', "screen effects: the preview", "screen effects: the Effects preview",
                                "screen effects: releasing the preview", "screen effects: Filter Strength",  # v3.56 PV356  # v3.58 PV358: + Filter Strength
                                'screen effects: the clean frame', "screen effects: loading the picture's")):  # v3.59 PV359
                self.pv_anom += 1
            if body.startswith('randomizer: spawn site'):  # v3.56 RE356
                self.ss_anom += 1
            elif body.startswith('randomizer: monsters'):  # v3.49 RZ349
                self.rzm_anom += 1
            elif body.startswith(('randomizer: outfit: ', 'randomizer: ghosts: ')):  # v3.49 RZ349L  # v3.49 MERGE: counted apart, as the Monsters' (not in rz_anom: the Items line keeps its own count)
                self.rzl_anom += 1
            elif body.startswith('randomizer: '):  # v3.48 RZ348
                self.rz_anom += 1
            if body.startswith('omni: '):  # v3.46 OM346
                self.om_anom += 1
                m = OM_RECHECK.match(body)
                if m:
                    self.om_rechecks.append((ts, m.group(1), int(m.group(2)), int(m.group(3))))
            return
        if self.version is None:
            m = re.match(r'SH4 Refined v([\w.]+) loading', msg)
            if m:
                self.version = m.group(1)
        m = re.match(r'log: level (\d)', msg)
        if m:
            self.level = int(m.group(1))
        m = STARTED.match(msg)  # v3.40 LG340
        if m and self.started is None:
            try:
                self.started = datetime.datetime(*(int(x) for x in m.groups()))
            except ValueError:
                pass
        if msg.startswith('session: d3d9.dll in the game folder'):  # v3.40 LG340: the renderer of a log before v3.39
            self.d3d_head = 'dgvoodoo'
        elif msg.startswith('session: no d3d9.dll in the game folder'):
            self.d3d_head = 'none'
        if msg.startswith(('session: fault watch', 'session: faults seen', 'fault watch: ', 'direct3d: ')):  # v3.59 SD359
            if self.parse_fw(ts, msg):
                return
        if msg.startswith('session: end'):
            self.end = msg[len('session: '):]
            return
        if msg.startswith('session: features:'):
            self.end_feats = msg[len('session: features: '):]
            return
        if msg.startswith('session: '):
            self.header.append(msg[len('session: '):])
            return
        if msg.startswith('display: '):
            self.display = msg[len('display: '):]
            return
        if msg.startswith('settings not default'):
            self.not_default.append(msg.split(': ', 1)[1] if ': ' in msg else msg)
            return
        if msg.startswith('gameplay: ') and ',' in msg or msg.startswith('player pack: '):
            if len(self.old_settings) < 4 and msg not in self.old_settings and ('MoveSpeed' in msg or 'AmmoSaver' in msg):
                self.old_settings.append(msg)
        if 'faulted' in msg:
            self.add_group(self.faults, norm(msg), ts, msg)
        elif re.search(r'is not the stock|differs? from the GOG v7|could not be (?:written|put back)|not installed|'
                       r'is not stock|cannot resolve|could not write', msg):
            self.add_group(self.warnings, norm(msg), ts, msg)
        m = BUDGET_IN.match(msg)  # v3.35 FX335
        if m:
            self.budget = m.group(1)
            return
        if msg.startswith('budget: '):  # v3.56 PW356: the high-rate fallback (the step lines also count below)
            m = BUDGET_HR_IN.match(msg)
            if m:
                self.bhr = m.group(1)
                return
            m = BUDGET_HR_RULES.match(msg)
            if m:
                self.bhr_rules.append((ts, int(m.group(1)), 'high' if m.group(2).startswith('the high') else 'v3.55'))
                return
            m = BUDGET_KEPT.match(msg)
            if m:
                self.bhr_kept.append((ts, int(m.group(1)), float(m.group(2))))
                return
            m = BUDGET_QUICK_IN.match(msg)  # v3.58 GX358
            if m:
                self.bq = m.group(1)
                return
            m = BUDGET_PRE.match(msg)  # v3.58 GX358 (a step line: counted below too)
            if m:
                self.bq_pre.append((ts, int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)))
            m = BUDGET_REFILL.match(msg)
            if m:
                self.bq_refill[0 if m.group(3).startswith('the powers') else 1] += 1
            if BUDGET_QUICK.match(msg):
                self.bq_quick += 1
            m = BUDGET_YIELD.match(msg)
            if m:
                self.bhr_yield.append((ts, int(m.group(1)), int(m.group(2))))
                return
            if BUDGET_BLIND.match(msg):
                self.bhr_blind += 1
            m = BUDGET_BACK.match(msg)
            if m:
                self.bhr_back.append((ts, float(m.group(3))))
            m = BUDGET_ROOM_HR.search(msg)
            if m and msg.startswith('budget: room '):
                g = m.groups()
                sr = msg[len('budget: room '):].split(':', 2)
                self.bhr_rooms.append(('%s:%s' % (sr[0], sr[1].zfill(2)) if len(sr) > 1 else '?', int(g[0]), int(g[1]), int(g[2]),
                                       int(g[3]), int(g[4])))
                q = BUDGET_ROOM_GX.search(msg)  # v3.58 GX358
                if q:
                    self.bq_rooms.append((self.bhr_rooms[-1][0], int(q.group(1)), int(q.group(2))))
        m = BUDGET_STEP.match(msg)
        if m:
            a, b = int(m.group(1)), int(m.group(2))
            self.budget_steps[0 if b > a else 1] += 1
            self.budget_steps[2] = max(self.budget_steps[2], b)
            return
        m = BUDGET_ROOM.match(msg)
        if m:
            g = m.groups()
            self.budget_rooms.append((g[0], g[1].zfill(2), dict(now=int(g[2]), what=g[3], max=int(g[4]), down=int(g[5]), up=int(g[6]),
                                                                thin=int(g[7]), mean=float(g[8]), worst=float(g[9]), target=float(g[10]),
                                                                late=float(g[11]), ts=ts)))
            self.budget_steps[2] = max(self.budget_steps[2], int(g[4]))
            return
        if msg.startswith('gameplay: '):  # v3.38 DR338: the Supply Drops level's changes
            m = DROP_LEVEL_SET.match(msg)
            if m:
                self.drop_changes.append((ts, m.group(1)))
            elif DROP_LEVEL_ALIAS.match(msg):
                self.drop_alias += 1
            m = FOL_MODE_SET.match(msg)  # v3.49 EF349
            if m and (not self.fol_modes or self.fol_modes[-1][1] != m.group(1)):
                self.fol_modes.append((ts, m.group(1)))
            m = EF_SET.match(msg)  # v3.50 EF350
            if m and (not self.ef_modes or self.ef_modes[-1][1] != m.group(1)):
                self.ef_modes.append((ts, m.group(1)))
        if self.parse_er(ts, msg):  # v3.56 ER356
            return
        if msg.startswith('supply drops: '):  # v3.37 MN337
            m = DROP_LEVEL_IN.match(msg)  # v3.38 DR338
            if m:
                self.drop_level = m.group(1)
                return
            m = DROP_LEVELS_LINE.match(msg)  # v3.38 DR338c
            if m:
                g = m.groups()
                self.drop_chances = (dict(zip(DROP_LEVEL_NAMES, (float(g[0]), float(g[1]), float(g[2])))),
                                     [(c[0], float(c[1]), float(c[2]), float(c[3])) for c in DROP_CLASS_ITEM.findall(g[3])], float(g[4]))
                return
            m = DROP_SHARES.match(msg)
            if m:
                g = m.groups()
                self.drop_equal = 'common %s, uncommon %s, rare %s' % (':'.join(g[0:3]), ':'.join(g[3:5]), ':'.join(g[5:9]))
                return
            m = DROP_CAPS.match(msg)
            if m:
                self.drop_caps = tuple(int(x) for x in m.groups())
                return
            m = DROP_NO.match(msg)
            if m:
                why = m.group(1)
                self.drops['room cap' if why.startswith("the room's") else 'lying cap' if why.startswith('the cap') else 'records'] += 1
                self.drop_lying = max(self.drop_lying, int(m.group(2)))
                return
            if DROP_WAY.match(msg):
                self.drops['made way'] += 1
                return
            if DROP_RENUM.match(msg):
                self.drops['renumbered'] += 1
                return
            if DROP_FADED.match(msg):
                self.drops['faded'] += 1
                return
            m = DROP_GIVEN.match(msg)
            if m:
                self.drops['given'] += 1
                self.drop_items[m.group(1)] += 1  # v3.38 DR338
                return
        m = PERF_SUM.match(msg)  # v3.37 CP337
        if m:
            g = m.groups()
            self.perf.append((ts, g[0], float(g[1]), int(g[2]), float(g[3]), float(g[4]), float(g[7]), float(g[8]), int(g[10]),
                              int(g[11]), int(g[12])))
            return
        m = PERF_BACKEND.match(msg)
        if m:
            self.backend = m.group(1)
            return
        if msg.startswith('renderer: '):  # v3.39 PR339
            m = REND_START.match(msg)
            if m:
                self.rend = m.groups()
                return
            m = REND_SWITCH.match(msg)
            if m:
                self.rend_sw.append((ts, m.group(1), m.group(4)))
                return
            m = REND_FAIL.match(msg)
            if m:
                self.rend_fail.append((ts, m.group(1), m.group(3)))
                return
        if msg.startswith('opening: '):  # v3.39 PR339
            m = OPEN_IN.match(msg)
            if m:
                self.opening = m.group(1)
                return
            m = OPEN_SKIP.match(msg)
            if m:
                self.open_ms = int(m.group(1))
                return
            m = OPEN_SAVED.match(msg)
            if m:
                self.open_saved.append((ts, m.group(1)))
                return
        if msg.startswith('pixel check alignment ('):  # v3.47 PX347
            m = PX_ALIGN.match(msg)
            if m:
                o = float(m.group(2))
                self.px_align.append((ts, m.group(1), -o if m.group(3) == 'left' else o, int(m.group(4))))
                return
            m = PX_ALIGN_NO.match(msg)
            if m:
                self.px_align.append((ts, m.group(1), None, 0))
                return
        if (msg.startswith('post effects: colour grading ') and ': the test colours ' in msg) or (
                msg.startswith('shadows: pixel check (') and ' %): K ' in msg):  # v3.47 PX347: the source as read back
            m = PX_SRC.search(msg)
            if m:
                k = 'as asked' if m.group(1).endswith('as asked') else 'not read back' if m.group(1).endswith('not read back') else 'lost'
                self.px_src[k] += 1
                if k == 'lost':
                    self.px_lost.append((ts, msg.split(':')[1].strip() if msg.startswith('post') else msg[len('shadows: '):msg.index(' %)') + 3],
                                         int(m.group(2))))
            return
        if msg.startswith('read back: '):  # v3.45 RB345
            m = RB_OK.match(msg)
            if m:
                self.rb = (ts, 'ok', m.group(1), '%s, %sx%s' % m.group(2, 3, 4))
                return
            m = RB_OFF.match(msg)
            if m:
                self.rb = (ts, 'none', m.group(1), '')
                return
            m = RB_SW.match(msg) or RB_LOST.match(msg)
            if m:
                self.rb_sw.append((ts, 'switch' if msg.startswith('read back: now') else 'lost', m.group(1)))
                return
        if msg.startswith('screen effects: Thermal Vision: '):  # v3.59 TV359
            if self.parse_tv(ts, msg):
                return
        if msg.startswith(("screen effects: the Effects page's preview", "screen effects: the pause's look layers",
                           'screen effects: the preview scene laid out',
                           'screen effects: [ScreenEffects] PreviewScene=', 'screen effects: [ScreenEffects] FilterStrength=',  # v3.58 PV358
                           'screen effects: the stock picture ', 'screen effects: Filter Strength ',  # v3.56 PV356  # v3.58 PV358
                           "screen effects: F7", "screen effects: the picture's ",  # v3.59 PV359
                           "screen effects: the Shadows page's preview: Preview ")):
            if self.parse_pv(ts, msg):
                return
        if msg.startswith('screen effects: the pause picture kept on the GPU') or msg.startswith('screen effects: a frame read back'):
            m = FX_GPU.match(msg)  # v3.41 SH341
            if m:
                self.fx_pic[0 if m.group(3) == 'target made' else 1] += 1
                self.fx_pic[2] = max(self.fx_pic[2], round(float(m.group(4)) + float(m.group(5)), 2))
                return
            m = FX_READ.match(msg)
            if m:
                self.fx_pic[3] += 1
                self.fx_pic[4] = max(self.fx_pic[4], round(float(m.group(4)) + float(m.group(5)) + float(m.group(6)), 2))
                return
        if msg.startswith('window views: '):  # v3.41 VW341
            m = VW_VIEW.match(msg)
            if m:
                self.vw_views.append((ts, '%s | %s at %sx%s' % m.groups()))
            else:
                self.vw = msg[len('window views: '):]
            return
        if msg.startswith('window: borderless edges: '):  # v3.41 VW341
            m = EDGE_COVER.match(msg)
            if m:
                self.edges.append((ts, int(m.group(1)), m.group(2), m.group(3)))
            else:
                self.edges_free += 1
            return
        if msg.startswith('save banks: '):  # v3.47 SB347
            m = SB_ROW.match(msg)
            if m:
                self.sb_row = (m.group(1), m.group(2), int(m.group(3)))
                return
            m = SB_CLICK.match(msg)
            if m:
                self.sb_clicks[m.group(1)] += 1
                self.sb_stuck += m.group(2) == m.group(3)
                return
            if SB_STEP.match(msg):  # v3.48 SB348
                self.sb_steps[self.cur_lv] += 1
                return
            m = SB_BAR.match(msg)  # v3.49 SB349
            if m:
                self.sb_bar = (m.group(1), m.group(2))
                return
            m = SB_SETTLE.match(msg)  # v3.48 SB348
            if m:
                g = m.groups()
                self.sb_changes.append((ts, int(g[0]), int(g[1]), g[2]) + tuple(int(x) for x in g[3:]))
                return
        if msg.startswith('settings: [Textures] MenuFont ') or msg.startswith('font: '):  # v3.43 FN343
            m = FN_SET.match(msg)
            if m:
                self.fn['set'] = (m.group(3), m.group(2), int(m.group(4)), int(m.group(5)))
                return
            m = FN_SWITCH.match(msg)  # v3.44 FN344
            if m:
                self.fn['switches'].append(m.group(1))
                return
            if FN_ROW.match(msg):  # v3.44 FN344
                self.fn['row'] = True
                return
            m = FN_LIVE.match(msg)
            if m:
                self.fn['live'] = True
                return
            m = FN_MATCH.match(msg)
            if m:
                self.fn['match'].add(int(m.group(1)))
                return
            m = FN_NOMATCH.match(msg)
            if m:
                self.fn['nomatch'].add(int(m.group(1)))
                return
            m = FN_STOCK.match(msg)
            if m:
                self.fn['stock'] = m.group(1)
                return
        if msg.startswith('2D edges: '):  # v3.42 EG342
            m = EDGE_2D_SIDES.match(msg)
            if m:
                self.e2d_sides['%sx%s' % m.group(1, 2)] = '%s | %s' % m.group(3, 4)
                return
            m = EDGE_2D.match(msg)
            if m:
                self.e2d.append(('%sx%s' % m.group(1, 2), 'stock' if 'PixelAlign2D=0' in m.group(3) else 'aligned'))
            return
        if msg.startswith('depth of field: '):  # v3.43 DF343
            m = DOF_ROOM.match(msg)
            if m:
                self.dof['rooms'] += 1
                self.dof['drawn'] += int(m.group(3))
                self.dof['asked'] += int(m.group(4))
                if float(m.group(6)) > self.dof_worst[0]:
                    self.dof_worst = [float(m.group(6)), float(m.group(5)), '%s:%s' % (m.group(1), m.group(2))]
                if DOF_NOREAD.search(msg):  # v3.45 RB345: the room's probe could not read back
                    self.dof_noread += 1
                c = DOF_CLOSE.search(msg)  # v3.52 FX352
                if c:
                    self.dof_close += int(c.group(1))
                w = DOF_WHY.search(msg)  # v3.44 DF344
                if w:
                    for txt, ctr in ((w.group(1), self.dof_why), (w.group(2), self.dof_design)):
                        for item in txt.split(', '):
                            name, _, n = item.rpartition(' ')
                            if name and n.isdigit():
                                ctr[name] += int(n)
                return
            m = DOF_PROBE.match(msg)
            if m:
                v = m.group(3)
                self.dof_probe['no read back' if v.endswith('no read back') else
                               'ok' if v.startswith('the scene') or v in ('too few', 'not probed') else v] += 1
                if v in ('flat', 'her figure'):
                    self.dof_skip.append('%s:%s (%s)' % (m.group(1), m.group(2), v))
                return
            m = DOF_IN.match(msg) or DOF_SET.match(msg)
            if m:
                self.dof_set.append((ts, '%s %s' % (m.group(1), m.group(2))))
                return
            m = DOF_STR.match(msg)
            if m:
                self.dof_set.append((ts, '%s %s' % (m.group(2), m.group(1))))
                return
            m = DOF_OFF.match(msg)
            if m:
                self.dof_off = (ts, m.group(1))
                return
            m = DOF_MIG.match(msg)  # v3.45 QS345
            if m:
                self.dof_mig = (m.group(1), m.group(2), m.group(3) == 'written')
                return
            m = DOF_NOT.match(msg)
            if m:
                self.dof_not = m.group(1) or m.group(2)
                return
            if DOF_KEPT.match(msg):
                self.dof['kept'] += 1
                return
            m = DOF_RAMPS.match(msg)  # v3.47 DF347
            if m:
                self.dof_ramps = 'Low %s+%s, Medium %s+%s, High %s+%s' % m.groups()[:6]
                if m.group(7):  # v3.59 DF359
                    self.dof_ramps += ', Ultra %s+%s' % m.group(7, 8)
                return
            m = DOF_NOTE59.match(msg)  # v3.59 DF359
            if m:
                self.dof_note59 = m.group(1, 2)
                return
            if DOF_SPIRIT.match(msg):  # v3.59 DF359
                if not self.dof_spirit:
                    self.dof_spirit = ts
                return
            if DOF_SPIRITKEEP.match(msg):  # v3.59 DF359
                self.dof_spirit_keep = True
                return
            m = DOF_FIRST.match(msg)  # v3.47 DF347
            if m:
                self.dof_first = m.group(3)
                return
            if DOF_NOBLEND.match(msg):  # v3.47 DF347
                self.dof_noblend += 1
                return
            m = DOF_CLOSE1.match(msg)  # v3.52 FX352
            if m:
                self.dof_close_rooms.append('%s:%s' % m.group(1, 2))
            return
        if msg.startswith('post effects: '):  # v3.43 PP343
            m = PP_IN.match(msg)
            if m:
                self.pp = m.group(1, 2, 3, 4)
                return
            if PP_INI_OFF.match(msg):
                self.pp_ini_off = True
                return
            m = PP_FAIL.match(msg)
            if m:
                self.pp_fail.append((ts, m.group(1), m.group(2)))
                return
            m = PP_MIG.match(msg)  # v3.52 FX352
            if m:
                self.pp_mig = (m.group(1), m.group(2), m.group(3) == 'written')
                return
            m = PP_LENS_ROOM.match(msg)  # v3.52 FX352
            if m:
                self.pp_lens_rooms.append(('%s:%s' % m.group(1, 2), int(m.group(3)), int(m.group(4)), int(m.group(6))))
                return
            m = PP_LENS.match(msg)
            if m:
                self.pp_lens[m.group(1)] += 1
                if int(m.group(6)) > self.pp_lens_max[0]:
                    self.pp_lens_max = [int(m.group(6)), '%s:%s' % (m.group(2), m.group(3))]
                return
            m = PP_CHAIN.match(msg)
            if m:
                self.pp_chain['%sx%s' % m.group(1, 2)] = float(m.group(4))
                return
            m = PP_NOTF.match(msg)
            if m:
                self.pp_notf['%s under %s' % (m.group(1), m.group(3))] += 1
                return
            m = PP_DRAWN.match(msg)
            if m:
                self.pp_drawn['%s %s' % m.group(1, 2)] += 1
                return
            m = PP_PX.match(msg)  # v3.44 CS344
            if m:
                self.pp_px.append((ts, m.group(1), m.group(2), int(m.group(3)), float(m.group(4)), float(m.group(5)), m.group(6)))
                return
            m = PP_PX_NOT.match(msg)
            if m:
                self.pp_px.append((ts, m.group(1), 'not run', 0, 0.0, 0.0, m.group(2)))
                return
            if PS_LINT.match(msg):
                self.ps_lint = True
                return
        if msg.startswith('screen effects: '):  # v3.43 PP343: the page's three rows
            m = PP_SET.match(msg)
            if m:
                self.pp_set.append((ts, m.group(1), m.group(2)))
                return
        if msg.startswith('shadows: '):  # v3.40 SH340
            m = SH_DARK.match(msg)  # v3.42 SM342
            if m:
                self.sh_dark.append((ts, int(m.group(1))))
                return
            m = SH_DARK_USE.match(msg)  # v3.42 SM342
            if m:
                self.sh_dark_use['%s %s %s %%' % (m.group(2), m.group(3), m.group(1))] = (float(m.group(4)), float(m.group(5)))
                return
            m = SH_PX.match(msg)  # v3.44 CS344
            if m:
                self.sh_px.append((ts, 'play' if m.group(1) == 'in play' else 'page', '%s %s %%' % m.group(2, 3), m.group(4),
                                   float(m.group(5)), float(m.group(6)), m.group(7)))
                return
            m = SH_PX_NOT.match(msg)
            if m:
                self.sh_px.append((ts, 'play' if m.group(1) == 'in play' else 'page', '%s %s %%' % m.group(2, 3), 'not run', 0.0, 0.0,
                                   m.group(4)))
                return
            m = SH_C1.match(msg)  # v3.43 SH343
            if m:
                self.sh_c1[('play' if m.group(1).startswith('in play') else 'page', '%s %s %%' % (m.group(2), m.group(3)))] = (
                    float(m.group(4)), int(m.group(6)), m.group(5) == 'read back', ts)
                return
            m = SH_COVER.match(msg)  # v3.49 SH349
            if m:
                self.sh_rib_state = 'the Darkness' if m.group(1) == 'the Darkness' else self.sh_rib_state or "the game's own"
                return
            m = SH_RIB.match(msg)
            if m:
                self.sh_rib_vals['%s -> %s at %s %%' % (m.group(2), m.group(3), m.group(1))] = int(m.group(4))
                return
            m = SH_RIB_ROOM.match(msg)
            if m:
                self.sh['rib_rooms'] += 1
                self.sh['rib'] += int(m.group(3))
                self.sh['rib_dark'] += int(m.group(4))
                return
            m = SH_RIB_NOT.match(msg)
            if m:
                self.sh_rib_state = ("the game's own (%s)" % ('TremerShadows=0' if m.group(1).startswith('[') else
                                                              'not stock' if 'not stock' in m.group(1) else 'not written'))
                return
            m = SH_NS.match(msg)  # v3.52 SH352
            if m:
                self.sh_ns.append((ts, '%s:%s' % (m.group(1), m.group(2)), m.group(3)))
                return
            m = SH_SW.match(msg)  # v3.58 GX358
            if m:
                h = SH_SW_HERE.search(msg)
                self.sh_sw.append((ts, m.group(1), '%s:%s %s' % (h.group(1), h.group(2).zfill(2), h.group(3)) if h else ''))
                return
            m = SH_PV_OFF.match(msg)
            if m:
                self.sh_pv_off += m.group(1).startswith('Shadows Off')
                return
            m = SH_ROOM.match(msg)
            if m:
                n, soft = int(m.group(3)), int(m.group(6))
                t = SH_ROOM_NS.search(msg)  # v3.52 SH352: why the rest was not soft (none before v3.52)
                hard, nodark, why = (int(t.group(1)), int(t.group(3)), Counter()) if t else (0, 0, Counter())
                if t and t.group(2):
                    for item in t.group(2).split(', '):
                        name, _, cnt = item.rpartition(' ')
                        if cnt.isdigit():
                            why[name] += int(cnt)
                unexpl = max(0, n - soft - hard - nodark)
                rr = self.sh_rooms.setdefault('%s:%s' % (m.group(1), m.group(2)), [0, 0, 0, 0, 0, Counter()])
                for i, v in enumerate((n, soft, hard, nodark, unexpl)):
                    rr[i] += v
                rr[5].update(why)
                o = SH_ROOM_OFF.search(msg)  # v3.58 GX358: drawn with Shadows Off (the game's own Off list)
                if o:
                    k = '%s:%s' % (m.group(1), m.group(2))
                    self.sh_off_rooms[k] = self.sh_off_rooms.get(k, 0) + int(o.group(1))
                self.sh['hard'] += hard
                self.sh['nodark'] += nodark
                self.sh['unexpl'] += unexpl
                self.sh['rooms'] += 1
                self.sh['composites'] += n
                self.sh['soft'] += soft
                self.sh['frames'] += int(m.group(4))
                if int(m.group(5)) > self.sh_worst[0]:
                    self.sh_worst[0] = int(m.group(5))
                if float(m.group(8)) > self.sh_worst[1]:
                    self.sh_worst[1:] = [float(m.group(8)), float(m.group(7)), '%s:%s' % (m.group(1), m.group(2))]
                return
            m = SH_IN.match(msg) or SH_EDGE.match(msg)
            if m:
                self.sh_edge.append((ts, m.group(1)))
                return
            m = SH_PLAN.match(msg)
            if m:
                self.sh_plans['%s %sx%s' % (m.group(1), m.group(2), m.group(3))] = '%d pass(es) to %s, taps %.2f: %.1f px' % (
                    int(m.group(4)), '%sx%s' % (m.group(5), m.group(6)), float(m.group(7)), float(m.group(8)))
                return
            m = SH_PLAN41.match(msg)  # v3.41 SH341
            if m:
                self.sh_plans['%s %sx%s' % (m.group(1), m.group(2), m.group(3))] = (
                    'to %sx%s (%s level(s), %s blur pass(es), %s draws, at most %sx): %.1f px (asked %.1f)' % (
                        m.group(4), m.group(5), m.group(6), m.group(7), m.group(10), m.group(11), float(m.group(8)), float(m.group(9))))
                return
            m = SH_OLDINI.match(msg)
            if m:
                self.sh_old = m.group(1)
                return
            m = SH_KEEP.match(msg)
            if m:
                self.sh['keep'] += 1
                return
            m = SH_LOWVA.match(msg)
            if m:
                self.sh['lowva'] += 1
                return
            m = SH_OFF.match(msg)
            if m:
                self.sh_off = (ts, m.group(1), m.group(3))
                return
            m = SH_NOT.match(msg)
            if m:
                self.sh_not = m.group(1)
                return
            m = SH_KEPT.match(msg)
            if m:
                self.sh['kept'] += 1
                if m.group(3):  # v3.41 SH341
                    self.sh_copy[0 if m.group(3) == 'targets made' else 1] += 1
                    self.sh_copy[2] = max(self.sh_copy[2], round(float(m.group(4)) + float(m.group(5)), 2))
                return
            m = SH_PREV.match(msg)
            if m:
                self.sh['scene' if m.group(1).startswith('the paused') else 'figure' if m.group(1).startswith('a figure') else 'none'] += 1
                return
            m = SH_RT.match(msg)
            if m:
                self.sh_vram = max(self.sh_vram, float(m.group(1)))
                return
        if msg.startswith(('hud bars: filter themes', 'hud bars: the enemy bars wear', 'hud bars: Thermal Vision: ',  # v3.59 FT359
                           'hud bars: room ')) and self.parse_ft(ts, msg):
            return
        if HB_CM42.match(msg):  # v3.42 SM342
            self.hb_cm42 = ts
            return
        m = HB_CM42_OLD.match(msg)  # v3.42 SM342
        if m:
            self.hb_cm42_old = (m.group(1), m.group(2))
            return
        m = HB_PE_RING.match(msg)  # v3.41 CM341
        if m:
            self.hb_pe_ring = (ts, 'refined' if m.group(1) else 'pe', m.group(1) or '')
            return
        m = HB_HIDE.match(msg)  # v3.38 HB338
        if m:
            self.hb_hide.append((ts, m.group(1)))
            return
        m = HB_REF.match(msg)  # v3.52 HB352
        if m:
            self.hb_ref_lines['hidden' if m.group(4) == 'not in the room yet' else 'place'] += 1
            mu = HB_UNDRAWN.match(msg)  # v3.58 EN358
            if mu:
                self.en358['hb_dog' if mu.group(1).startswith('an event dog') else 'hb_undrawn'] += 1
            if self.hb_ref_first is None:
                self.hb_ref_first = (ts, m.group(1), m.group(2), m.group(3))
            return
        m = HB_REF_ROOM.match(msg)
        if m:
            self.hb_ref['rooms'] += 1
            self.hb_ref['hidden'] += int(m.group(2))
            self.hb_ref['place'] += int(m.group(3))
            self.hb_ref_tot = (int(m.group(4)), int(m.group(5)))
            return
        m = HB_UNDRAWN.match(msg)  # v3.58 EN358 (the line also counts as a refusal: HB_REF read it above)
        if m:
            self.en358['hb_dog' if m.group(1).startswith('an event dog') else 'hb_undrawn'] += 1
        m = HB_WALTER.match(msg)  # v3.58 EN358
        if m:
            k = {'down': 'w_down', 'stumble': 'w_stumble', 'gone': 'w_gone'}.get(m.group(2), 'w_other')
            self.en358[k] += 1
            if k == 'w_down':
                self.en358['w_most'] = max(self.en358['w_most'], int(float(m.group(5))))
            if 'he heals to full' in msg:  # v3.59 EN359
                self.en359['w_heal'] += 1
            return
        m = HB_WLONG.match(msg)  # v3.59 EN359
        if m:
            self.en359_wlong = 'in' if m.group(1).startswith('his') else 'NOT in'
            return
        m = HB_ANCHOR.match(msg)
        if m:
            self.en359['anchors'] += 1
            self.en359['anchors_fb'] += not m.group(3).startswith('its top')
            return
        m = HB_EDGE.match(msg)
        if m:
            self.en359['edges'] += 1
            return
        m = HB_WALTER_SITE.match(msg)
        if m:
            self.en358_wsite = (m.group(1), m.group(2))
            return
        m = HB_BOSS.match(msg)
        if m:
            self.hb_boss.append((ts, m.group(1), m.group(4), float(m.group(5)) if m.group(5) else None, m.group(6)))
            return
        m = HB_HIDE_RING.match(msg)
        if m:
            why = m.group(3).lstrip(': ')
            self.hb_ring['faded out' if m.group(2) == 'faded out' else 'shown ' + why.replace('woken by ', 'by ')] += 1
            return
        m = PERF_PLACE.match(msg)
        if m:
            where = {'at the top-left corner, clear of': 'corner', 'under': 'under', 'right of': 'right of',
                     'at the top-left corner, over': 'corner OVER'}[m.group(1)]
            what = 'PE HUD' if m.group(2).startswith("Playable Eileen's HUD") else 'stock gauge'
            self.perf_place['%sx%s' % (m.group(3), m.group(4))] = (where, what, ts)
            return
        if msg.startswith('sound: '):  # v3.52 SND352
            if self.parse_snd(ts, msg):
                return
        if msg.startswith('options layout: '):  # v3.54 OL34
            if self.parse_ly(ts, msg):
                return
        if msg.startswith(('options: value lines: ', 'options: folder rows ', 'options: a click on page ', 'extra menu: text drawn ',
                           'options: value box')):  # v3.58 MN358: + the value box
            if self.parse_mn(ts, msg):  # v3.56 MN356
                return
        if msg.startswith(('options look: ', 'options: page ')):  # v3.53 OL2
            if self.parse_ol(ts, msg):
                return
        if msg.startswith('extra menu: sound in the '):  # v3.51 MU351
            if self.parse_mu(ts, msg):
                return
        if msg.startswith(('quick load: ', 'extra menu: Quick Load ')):  # v3.52 QL352
            if self.parse_ql(ts, msg):
                return
        m = CAM_PAGE.match(msg)
        if m:
            if m.group(1) == 'opened':
                self.cam_page[0] += 1
            else:
                self.cam_page[1] = (m.group(2) or '', ts)
            return
        m = POOL_TASKS.match(msg)  # v3.36 TP336
        if m:
            self.pool_n[0] = int(m.group(1)) if m.group(1) else 300
            return
        m = POOL_IK.match(msg)
        if m:
            self.pool_n[1] = int(m.group(1)) if m.group(1) else 160
            return
        m = POOLS_ROOM.match(msg)
        if m:
            g = m.groups()
            self.pool_rooms.append(('%s:%s' % (g[0], g[1].zfill(2)), ts, int(g[2]), int(g[3]), int(g[4]), int(g[5]),
                                    int(g[6]) if g[6] else None, int(g[7]) if g[7] else None, int(g[8]) if g[8] else None,
                                    int(g[9]), int(g[10])))
            return
        m = FOL_MODE_IN.match(msg)  # v3.49 EF349 (no return: the install line is read by others too)
        if m and self.fol_mode is None:
            self.fol_mode = m.group(1)
        if FOL_MODE_SAY.match(msg):  # v3.49 EF349: the value's own line (the menu / ini lines give the changes)
            return
        if msg.startswith('enemies: follow ') or msg.startswith('enemies: followers across saves'):  # v3.50 EF350
            m = EF_SAVE.match(msg)
            if m:
                g = [int(x) for x in m.groups()[:6]]
                e = self.ef
                e['saves'] += 1
                e['saves_unwritten'] += m.group(7) is not None
                e['s_pur'] += g[0]; e['s_here'] += g[1]; e['s_q'] += g[3]; e['s_tag'] += g[4]; e['s_cw'] += g[5]
                return
            m = EF_LOAD.match(msg)
            if m:
                self.ef['found' if m.group(1).startswith('this') else 'none'] += 1
                return
            m = EF_REST.match(msg)
            if m:
                g = [int(x) for x in m.groups()]
                e = self.ef
                e['restores'] += 1
                for k, v in zip(('r_pur', 'r_here', 'r_away', 'r_q', 'r_tag', 'r_cw', 'r_norec', 'r_kind'), g):
                    e[k] += v
                return
            if EF_REST_NO.match(msg):
                self.ef['r_no'] += 1
                return
            m = EF_RESTORED.match(msg)  # v3.59 EN359
            if m:
                e = self.en359
                e['loads'] += 1
                e['back'] += int(m.group(1)); e['saved'] += int(m.group(2))
                if m.group(3) is not None:
                    e['kr_back'] += int(m.group(3)); e['kr_all'] += int(m.group(4))
                return
            m = EF_KEPTREC.match(msg)
            if m:
                self.en359['kr_saves'] += 1
                self.en359['kr_kept'] += int(m.group(1))
                return
            m = EF_IN.match(msg)
            if m:
                self.ef_site, self.ef_mode = m.group(1), m.group(2)
                return
        if msg.startswith('enemies: ') and (' lies pinned by its Sword' in msg or 'pinned ghosts' in msg or '0046511D' in msg):  # v3.59 EN359
            m = EN_PINGATE.match(msg)
            if m:
                self.en359['pins'] += 1
                self.en359_pin_last = (ts, m.group(1), m.group(3))
                return
            m = EN_PINGATE_IN.match(msg)
            if m:
                self.en359_pinsite = 'in' if msg.startswith('enemies: pinned ghosts stay') else 'off' if 'is off' in msg else 'NOT in'
                return
        if msg.startswith('enemies: '):  # v3.51 EP351: Enemy Pursuit inside the room
            m = ENM_CPRECS.match(msg)  # v3.58 RZ358: copies not made for the enemy records
            if m:
                self.cp_recs['rooms'] += 1
                self.cp_recs['not'] += int(m.group(3))
                return
            if ENM_CPRECV.match(msg):
                self.cp_recs['v'] += 1
                return
            m = EP_ROOM.match(msg)
            if m:
                g = [int(x) for x in m.groups()[1:]]
                p = self.ep
                p['rooms'] += 1
                for k, v in zip(('give', 'mon', 'ghost', 'again', 'down', 'held', 'stock'), g):
                    p[k] += v
                m = EP_ROOM_CALM.search(msg)  # v3.52 EP352
                if m:
                    p['undive'] += int(m.group(1)); p['sensed'] += int(m.group(2))
                return
            m = EP_GIVE.match(msg)
            if m:
                p = self.ep
                p['v_give'] += 1
                p['v_' + EP_WHY[m.group(4)]] += 1
                p['f_' + (m.group(2) or "victim's ghost")] += 1
                return
            m = EP_IN.match(msg)
            if m:
                self.ep_site = m.group(1)
                return
        m = EF_LOST.match(msg)  # v3.50 EF350
        if m:
            e = self.ef
            e['lost_doors'] += int(m.group(2)) > 0
            e['lost'] += int(m.group(2)); e['sight'] += int(m.group(3)); e['down'] += int(m.group(4))
            e['behind'] += int(m.group(5)); e['forgot'] += int(m.group(6))
            return
        m = FOL_HOME.match(msg)  # v3.49 EF349
        if m:
            self.fol_home[0] += int(m.group(2))
            return
        m = FOL_HOME_CW.match(msg)
        if m:
            self.fol_home[1] += int(m.group(1) or m.group(2))
            return
        m = FOL_DOOR_G.match(msg)  # v3.58 EN358 (FOL_DOOR below reads the same line)
        if m and int(m.group(1)):
            self.en358['g_doors'] += 1
            self.en358['g_door'] += int(m.group(1))
        m = FOL_GSUM.match(msg)
        if m:
            self.en358['g_rooms'] += 1
            self.en358['g_in'] += int(m.group(1))
            self.en358['g_tag'] += int(m.group(2))
            self.en358['g_arr'] += int(m.group(4))
            self.en358['g_out'] += int(m.group(5))
            if int(m.group(3)) >= 0 and (self.en358_near is None or int(m.group(3)) < self.en358_near):
                self.en358_near = int(m.group(3))
            return
        m = FOL_GIN.match(msg)
        if m:
            self.en358['g_first'] += 1
            if m.group(2):
                self.en358['g_drawn'] += 1
            return
        m = FOL_DOOR.match(msg)  # v3.38 EN338
        if m:
            n = int(m.group(1))
            self.fol['doors'] += 1
            self.fol['follow'] += n
            self.fol['most'] = max(self.fol['most'], n)
            self.fol['wait'] += int(m.group(2))
            self.fol['stopped'] += int(m.group(3))
            return
        if FOL_CAME.match(msg):
            self.fol['came'] += 1
            return
        if FOL_SPOT.match(msg):
            self.fol['spotted'] += 1
            return
        m = FOL_WAIT.match(msg)
        if m:
            self.fol_wait[m.group(1)] += 1
            return
        m = FOL_STORY.match(msg)
        if m:
            self.fol['story'] += 1
            return
        if FOL_STOP.match(msg):
            self.fol['stops'] += 1
            return
        if FOL_FRESH.match(msg):  # v3.41 EN341
            self.fol['fresh'] += 1
            return
        if FOL_WHEEL.match(msg):
            self.fol['wheelchairs'] += 1
            return
        m = INV_INSTALL.match(msg)  # v3.37 INV337
        if m:
            self.inv = m.groups()
            return
        m = INV_SWITCH.match(msg)
        if m:
            self.inv_sw['%s %s' % m.groups()] += 1
            return
        if msg.startswith('omni: '):  # v3.46 OM346
            if self.parse_omni(ts, msg):
                return
        if msg.startswith(('regeneration: ', 'life steal: ', 'settings: [Gameplay] Regen')):  # v3.47 RG347
            if self.parse_regen(ts, msg):
                return
        if msg.startswith(('room 302', 'settings: [Room302] ')):  # v3.50 R302
            if self.parse_r302(ts, msg):
                return
        if msg.startswith('frame rate: ') and self.parse_cr(ts, msg):  # v3.59 FR359
            return
        if msg.startswith(('rate guard: ', 'true 60: ')):  # v3.56 FR356
            if self.parse_frg(ts, msg):
                return
        if msg.startswith('visits: '):  # v3.56 RI356
            if self.parse_visits(ts, msg):
                return
        if msg.startswith('randomizer: '):  # v3.49 RZ349L: the looks lines first (their own patterns)
            r = self.parse_rzl(ts, msg)
            if r is not None:
                if r:
                    return
            elif self.parse_rando(ts, msg):  # v3.48 RZ348
                return
        if msg.startswith('fade in: '):  # v3.50 FI350
            if self.parse_fadein(ts, msg):
                return
        m = GT_INSTALL.match(msg)  # v3.43 GT343
        if m:
            self.gt = m.groups()
            return
        m = GT_WARP.match(msg)
        if m:
            self.gt_warps[m.group(1)] += 1
            self.gt_faced += GT_FACED in msg  # v3.50 TP350
            return
        m = GT_THRU.match(msg)  # v3.50 TP350
        if m:
            self.gt_warps['through'] += 1
            return
        m = GT_ROOM.match(msg)
        if m:
            g = m.groups()
            self.gt_rooms.append(('%s:%s' % (g[0], g[1]),) + tuple(int(x) for x in (g[2], g[3], g[4], g[5], g[6], g[8])))
            for part in (g[7] or '').split(', '):
                k, _, v = part.rpartition(' ')
                if k and v.isdigit():
                    self.gt_room_no[k] += int(v)
            self.gt_skip += int(g[9] or 0)  # v3.51 TP351
            self.gt_skip_tries += int(g[10] or 0)
            self.gt_room_freed += int(g[11] or 0)  # v3.52 TP352
            self.gt_room_lv[0] += int(g[12] or 0)  # v3.59 GT359
            self.gt_room_lv[1] += int(g[13] or 0)
            self.gt_map_open += 'not finished' in g[14] or 'OVERFLOW' in g[14]
            return
        m = GT_LEVEL.match(msg)  # v3.59 GT359
        if m:
            g = m.groups()
            self.gt_warps['%s a level' % g[0]] += 1
            self.gt_lv.append((g[0], int(g[3]), int(g[5])))
            return
        m = GT_LV_FLAT.match(msg)
        if m:
            self.gt_lv_flat['%s:%s' % (m.group(1), m.group(2))] = m.group(3)
            return
        m = GT_FREE.match(msg)  # v3.52 TP352
        if m:
            self.gt_free[m.group(1)] += 1
            return
        if GT_FREE_NO.match(msg):
            self.gt_free_no += 1
            return
        m = GT_MAP.match(msg)
        if m:
            g = m.groups()
            self.gt_maps.append(('%s:%s' % (g[0], g[1]), int(g[2]), int(g[3]), int(g[4]), float(g[5])))
            return
        m = GT_NO.match(msg)
        if m:
            self.gt_no[m.group(1)] += 1
            self.gt_zone_no += GT_ZONE_NO in msg or GT_ZONE_IN in msg  # v3.49 GT349 (GT349-001: + inside one)
            return
        m = GT_HOLD.match(msg)  # v3.49 GT349
        if m:
            self.gt_holds.append((int(m.group(2)), int(m.group(3))))
            return
        m = GT_ZONES.match(msg)
        if m:
            self.gt_zones['%s:%s' % (m.group(1), m.group(2))] = int(m.group(3))
            return
        if GT_COOL.match(msg):
            self.gt_cool += 1
            return
        m = GT_AHEAD.match(msg)  # v3.44 GT344
        if m:
            self.gt_warps['ahead'] += 1
            return
        m = GT_CHORD.match(msg)
        if m:
            g = m.group(1)
            self.gt_chord['held' if g.startswith('held') else 'NOT taken back' if 'NOT' in g else 'taken back' if 'taken back' in g
                          else 'Action first'] += 1  # v3.49 GT349: + held
            return
        m = GT_FADE.match(msg)
        if m:
            self.gt_fades[0] += 1
            self.gt_fades[1] += int(m.group(1)) == 0
            p = GT_FADE_PATH.search(msg)  # v3.58 GT358
            if p:
                row = self.gt_fpath.setdefault(p.group(6), [0] * 9)
                row[0] += 1
                for j, g in enumerate((1, 2, 3, 4, 5, 7, 8, 9)):
                    row[j + 1] += int(p.group(g))
            return
        m = GT_FADE_DRAW.match(msg)  # v3.58 GT358
        if m:
            self.gt_fdraw = m.group(1)
            return
        m = GT_FADE_NOTIN.match(msg)
        if m:
            self.gt_fnotin = m.group(1)
            return
        m = GT_OFF.match(msg)
        if m:
            self.gt_off[m.group(1)] += 1
            return
        m = INV_COUNTS.match(msg)
        if m:
            self.inv_counts = [int(x) for x in m.groups()]
            return
        m = HIT_ROOM.match(msg)  # v3.36 HE336
        if m:
            g = m.groups()
            self.hits.append(('%s:%s' % (g[0], g[1]), int(g[2]), int(g[3]), int(g[5]), int(g[7]), ts))
            return
        m = REC_ROOM.match(msg)
        if m:
            g = m.groups()
            self.recs.append(('%s:%s' % (g[0], g[1]), int(g[4]), int(g[7]), int(g[8]), int(g[9]), int(g[6]), ts))
            return
        m = WARM_LINE.match(msg)
        if m:
            parts = dict((p, float(v)) for p, v in WARM_PART.findall(m.group(3)))
            self.warms.append((ts, '%s:%s' % (m.group(1), m.group(2).zfill(2)), float(m.group(4)), parts))
            return
        if msg.startswith('death presence: Room 302 ('):  # v3.42 SM342
            m = SM_R302_IN.match(msg)
            if m:
                self.r302 = m.group(1)
                return
            m = SM_R302.match(msg)
            if m:
                self.r302_n += 1
                return
        if msg.startswith('after shock: '):  # v3.56 PW356: the over-the-shoulder fade
            m = AI_OTS_CAM.match(msg)
            if m:
                self.ots_cam[m.group(1)] += 1
                return
            m = AI_OTS_FADE.match(msg)
            if m:
                self.ots_fade = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
                return
            m = AI_OTS_INI.match(msg)
            if m:
                self.ots_ini = int(m.group(1))
                return
            m = AI_OTS_ROOM.match(msg)
            if m:
                self.ots_rooms.append(('%s:%s' % (m.group(1), m.group(2).zfill(2)), int(m.group(3)), int(m.group(4)), int(m.group(5))))
                return
        if msg.startswith('echo: ') or msg.startswith('after shock: '):  # v3.42 SM342: the view gate
            m = SM_VIEW_HIDE.match(msg)
            if m:
                self.vg[m.group(1)] += 1
                self.vg_why[m.group(2)] += 1
                return
            m = SM_VIEW_SHOWN.match(msg)
            if m:
                self.vg_shown[0] += 1
                if float(m.group(2)) > self.vg_shown[1]:
                    self.vg_shown[1:] = [float(m.group(2)), ts]
                return
        if msg.startswith('death presence: stage '):  # v3.41 DP341: the draw gate (the room lines fall through)
            m = DPG_HIDE.match(msg)
            if m:
                self.dpg[m.group(3)] += 1
                return
            m = DPG_SHOWN.match(msg)
            if m:
                self.dpg_shown[0] += 1
                if float(m.group(3)) > self.dpg_shown[1]:
                    self.dpg_shown[1:] = [float(m.group(3)), '%s:%s' % (m.group(1), m.group(2).zfill(2)), ts]
                return
            m = DPG_LONG.match(msg)
            if m:
                self.dpg_long.append((ts, '%s:%s' % (m.group(1), m.group(2).zfill(2)), m.group(3)))
                return
            if DPG_WARP.match(msg):
                self.dpg_warp += 1
                return
        m = ROOM_LINE.match(msg)
        if m:
            g = m.groups()
            r = {'secs': float(g[2]), 'play': float(g[3]), 'ts': ts}
            if g[6] is not None:
                r.update(frames=int(g[6]), mean=float(g[7]), p99=float(g[8]), worst=float(g[9]), at=float(g[10]),
                         why=g[11], spikes=int(g[12]), stalls=int(g[14]))
            else:
                r.update(frames=0, stalls=int(g[15]))
            self.rooms.append((g[0], g[1], r))
            return
        m = LOAD_LINE.match(msg)
        if m:
            g = m.groups()
            self.loads.append((ts, int(g[0]), '%s:%s' % (g[1], g[2].zfill(2)), g[3], int(g[4]), int(g[5] or 0), int(g[6]), int(g[7])))  # v3.35: budget 0 = none
            return
        m = FEAT_LINE.match(msg)
        if m:
            for item in m.group(3).split('; '):
                fm = FEAT_ITEM.match(item + ' ')
                if not fm:
                    continue
                e = self.feats.setdefault(fm.group('name'), [0, 0, 0, Counter()])
                n = int(fm.group('n'))
                e[0] += 1
                e[1] += 1 if n > 0 else 0
                e[2] += n
                e[3][fm.group('val')] += 1
            return
        for rx in OLD_ROOM:
            m = rx.search(msg)
            if m:
                key = '%s:%s' % (m.group(1).upper(), m.group(2).upper().zfill(2))
                self.old_rooms[key] = self.old_rooms.get(key, 0) + 1
                break

    # ------------------------------------------------------------ the report
    def report(self, full, folder):
        out = []
        say = out.append
        when = ''
        if self.t0 is not None:
            when = '%s, %s long' % (hms(self.t0 % 86400), hms(self.span))
        say('== %s (%s; %d lines, %.0f KB; %d verbose, %d trace)' % (
            self.name, when, self.lines, self.size / 1024.0, self.levels['v'], self.levels['t']))
        head = 'SH4 Refined v%s' % (self.version or '?')
        if self.level is not None:
            head += ', log level %d' % self.level
        say('  version: ' + head)
        for h in self.header:
            say('  ' + h[:200])
        if not self.header:
            say('  (no session header: a log of v3.32 or before; room lines and framework checks start with v3.33)')
        if self.display:
            say('  display: ' + self.display[:220])
        if self.not_default:
            say('  not default: ' + ' | '.join(self.not_default)[:400 if not full else 4000])
        elif self.old_settings:
            for s in self.old_settings:
                say('  ' + s[:220])
        # the end
        dump = self.crash_dump(folder)
        if self.end:
            say('  end: ' + self.end[:300])
        elif self.header:
            say('  end: MISSING: the game crashed or was killed before its exit%s' % (
                ('; crash dump ' + dump) if dump else ''))
        elif dump:
            say('  crash dump near this session: ' + dump)
        # rooms
        if self.rooms:
            visits = Counter('%s:%s' % (s, r) for s, r, _ in self.rooms)
            play = sum(x['play'] for _, _, x in self.rooms)
            say('  rooms: %d visits, %d rooms, %s of play; most: %s' % (
                len(self.rooms), len(visits), hms(play),
                ', '.join('%s x%d' % kv for kv in visits.most_common(8 if not full else 99))))
        elif self.old_rooms:
            say('  rooms seen (from the features\' room lines): ' + ', '.join(list(self.old_rooms)[:20 if not full else 999]))
        if self.loads:
            say('  loads: %d: %s' % (len(self.loads), ', '.join(
                '%s %s (game time %d, drops %d/%d, pickup free %d/%d)' % (l[0], l[2], l[1], l[4], l[5], l[6], l[7])
                for l in self.loads[:4 if not full else 999])) + (' ...' if len(self.loads) > 4 and not full else ''))
        # anomalies
        total = sum(g[0] for g in self.anomalies.values())
        if total:
            say('  ANOMALY: %d line(s), %d kind(s):' % (total, len(self.anomalies)))
            for k, (n, first, last_, text) in sorted(self.anomalies.items(), key=lambda kv: -kv[1][0])[:8 if not full else 999]:
                say('    %3dx %s%s' % (n, text[:170], ('  [%s .. %s]' % (first, last_)) if n > 1 else '  [%s]' % first))
        elif self.header:
            say('  ANOMALY: none')
        for title, d in (('faults', self.faults), ('install / write warnings', self.warnings)):
            if d:
                say('  %s: %d' % (title, sum(g[0] for g in d.values())))
                for k, (n, first, last_, text) in list(d.items())[:5 if not full else 999]:
                    say('    %3dx %s  [%s]' % (n, text[:170], first))
        # worst frames
        framed = [(x['worst'], s, r, x) for s, r, x in self.rooms if x.get('frames')]
        if framed:
            framed.sort(key=lambda t: t[0], reverse=True)
            spikes = sum(x['spikes'] for _, _, _, x in framed)
            say('  frames: %d spikes in all; worst rooms:' % spikes)
            for w, s, r, x in framed[:3 if not full else 20]:
                say('    %s:%s %.1f ms at +%.1f s (%s); p99 %.1f, mean %.1f, spikes %d  [%s]' % (
                    s, r, w, x['at'], x['why'][:160], x['p99'], x['mean'], x['spikes'], x['ts']))
        # v3.35 FX335: the effect budget and the warm-up
        if self.budget or self.budget_rooms:
            thinned = [x for x in self.budget_rooms if x[2]['max'] > 0]
            say('  effect budget: %s; %d step(s) down, %d up, deepest level %d; thinned in %d of %d room line(s)' % (
                self.budget or '?', self.budget_steps[0], self.budget_steps[1], self.budget_steps[2], len(thinned), len(self.budget_rooms)))
            for s, r, x in sorted(thinned, key=lambda t: -t[2]['worst'])[:3 if not full else 20]:
                say('    %s:%s at most level %d, %d %% of the frames thinned, effects %.1f ms mean / %.1f max (target %.1f), %.1f %% late  [%s]' % (
                    s, r, x['max'], x['thin'], x['mean'], x['worst'], x['target'], x['late'], x['ts']))
        if self.bhr or self.bhr_rules or self.bhr_rooms or self.bhr_blind:  # v3.56 PW356: the high-rate fallback
            hr_rooms = [x for x in self.bhr_rooms if x[1] > 0]
            rates = ', '.join('%d fps %s [%s]' % (f, 'high-rate rules' if k == 'high' else "v3.55's rules", t) for t, f, k in self.bhr_rules[:6])
            say('  effect budget high rate: %s; %d room line(s) at a high rate; %d blind step(s): %d kept, %d taken back; nothing left to shed %d time(s)%s%s' % (
                self.bhr or '?', len(hr_rooms), self.bhr_blind, len(self.bhr_kept), len(self.bhr_back), len(self.bhr_yield),
                ('; rates: ' + rates) if rates else '', ('; the last go-ahead %d fps at level %d [%s]' % (self.bhr_yield[-1][1], self.bhr_yield[-1][2],
                                                                                                         self.bhr_yield[-1][0])) if self.bhr_yield else ''))
            for x in sorted(hr_rooms, key=lambda t: -(t[2] + t[4]))[:3 if not full else 20]:
                if x[2] or x[5] < 100:
                    say('    %s: %d high-rate frame(s), %d blind step(s), %d kept, %d taken back; the sparks at %d %%' % x)
        if self.bq or self.bq_pre or self.bq_quick or any(self.bq_refill):  # v3.58 GX358: the quick rules
            say('  effect budget quick rules: %s; %d start step(s) (powers starting together)%s; refill: %d level(s) back, %d take-back(s); '
                '%d quick step(s)' % (self.bq or '?', len(self.bq_pre), (': ' + ', '.join(
                    '%d -> %d at %d fps (%s) [%s]' % (a, b, f, w, t) for t, a, b, f, w in self.bq_pre[-3:])) if self.bq_pre else '',
                    self.bq_refill[0], self.bq_refill[1], self.bq_quick))
        if self.ots_cam or self.ots_rooms or self.ots_fade or self.ots_ini is not None:  # v3.56 PW356: After Shock's OTS fade
            dim = sum(x[2] for x in self.ots_rooms)
            least = min([x[3] for x in self.ots_rooms] or [100])
            say('  after shock over-the-shoulder: %s; camera on %d / off %d time(s); %d visit line(s) with the fade: %d copy draw(s) dimmed, the least light %d %%%s' % (
                ('a copy over her keeps %d %%, her box %d x %d u' % self.ots_fade) if self.ots_fade else
                ('AfterImageOtsFade=%d' % self.ots_ini if self.ots_ini is not None else 'the fade (no box line)'),
                self.ots_cam['on'], self.ots_cam['off'], len(self.ots_rooms), dim, least,
                ('; [Gameplay] AfterImageOtsFade=%d' % self.ots_ini) if self.ots_ini is not None else ''))
        if self.warms:
            w = max(self.warms, key=lambda t: t[2])
            say('  warm-up: %d time(s), %.1f ms in all; the longest %.1f ms in %s (%s)  [%s]' % (
                len(self.warms), sum(x[2] for x in self.warms), w[2], w[1],
                ', '.join('%s %.1f' % kv for kv in w[3].items()) or '-', w[0]))
        if self.dpg or self.dpg_shown[0]:  # v3.41 DP341: Death Presence's draw gate
            line = '  death presence hidden: %d time(s) (%s); shown again %d time(s), the longest hold %.1f s in %s [%s]' % (
                sum(self.dpg.values()), ', '.join('%s x%d' % kv for kv in self.dpg.most_common(6)) or '-',
                self.dpg_shown[0], self.dpg_shown[1], self.dpg_shown[2] or '-', self.dpg_shown[3] or '-')
            if self.dpg_long:
                line += '; past 20 s %d time(s) (the last: %s in %s [%s])' % ((len(self.dpg_long),) + self.dpg_long[-1][2:0:-1] +
                                                                              (self.dpg_long[-1][0],))
            if self.dpg_warp:
                line += '; %d pending move(s) no longer waited for' % self.dpg_warp
            say(line)
        if self.r302:  # v3.42 SM342
            say('  death presence in Room 302: %s' % ('off (the hauntings as stock)' if self.r302 == 'off' else
                                                       'on ([ScreenEffects] DeathPresenceRoom302=1)') +
                ('; %d entr%s' % (self.r302_n, 'y' if self.r302_n == 1 else 'ies') if self.r302_n else ''))
        if self.vg or self.vg_shown[0]:  # v3.42 SM342: Echo Doubles / After Shock hidden in views
            say('  effects hidden in views: %s (%s); shown again %d time(s), the longest hide %.1f s [%s]' % (
                ', '.join('%s %d' % kv for kv in sorted(self.vg.items())) or '-',
                ', '.join('%s x%d' % kv for kv in self.vg_why.most_common(4)) or '-', self.vg_shown[0], self.vg_shown[1],
                self.vg_shown[2] or '-'))
        # v3.36 TP336: the task / IK pools' use
        if self.pool_rooms or self.pool_n[0] or self.pool_n[1]:
            pr = self.pool_rooms
            head = '  pools: tasks %s, IK %s' % (self.pool_n[0] or '?', self.pool_n[1] or '?')
            if pr:
                t = max(pr, key=lambda x: x[2])
                high = [x for x in pr if x[5] > 0]
                ik = [x for x in pr if x[6] is not None]
                head += '; tasks at most %d of %d in use (%s, fewest free %d  [%s]); slots 300+ used in %d of %d room line(s)' % (
                    t[2], t[3], t[0], t[4], t[1], len(high), len(pr))
                if ik:
                    k = max(ik, key=lambda x: x[6])
                    head += '; IK at most %d of %d (%s)' % (k[6], k[8], k[0])
                head += '; actor records fewest free %d; heap at most %d units' % (min(x[9] for x in pr), max(x[10] for x in pr))
            say(head)
        # v3.36 HE336: the hit-shape pool and the enemy records
        if self.hits:
            low = min(self.hits, key=lambda h: h[1] if h[1] >= 0 else 1 << 30)
            say('  hit shapes: %d room line(s); lowest free %d of %d in %s; refusals %d, stomp guard %d  [%s]' % (
                len(self.hits), low[1], low[2], low[0], sum(h[3] for h in self.hits), sum(h[4] for h in self.hits), low[5]))
        if self.recs:
            top = max(self.recs, key=lambda r: r[1])
            say('  enemy records: %d room line(s); most in use %d of 64 (%s); evictions: %d copies\' record(s), %d real (%d for a '
                'copy\'s write); %d copies\' record(s) purged' % (len(self.recs), top[1], top[0], sum(r[2] for r in self.recs),
                                                                 sum(r[3] for r in self.recs), sum(r[4] for r in self.recs),
                                                                 sum(r[5] for r in self.recs)))
        if self.cp_recs['rooms'] or self.cp_recs['v']:  # v3.58 RZ358
            say('  enemy records for copies: %d cop(ies) not made in %d room load(s): the records would be full at the room\'s exit (a copy '
                'never takes a real record of this world)' % (self.cp_recs['not'], self.cp_recs['rooms']))
        # v3.37 MN337: Supply Drops against its caps
        if self.drops or self.drop_caps:
            d = self.drops
            caps = (' (caps %d anywhere / %d a room, %d records kept free)' % self.drop_caps) if self.drop_caps else ''
            say('  supply drops%s: %d given, %d faded, %d made way at the lying cap; refused: %d room cap, %d lying cap, %d too few '
                'records (most lying named %d)%s' % (caps, d['given'], d['faded'], d['made way'], d['room cap'], d['lying cap'],
                                                     d['records'], self.drop_lying,
                                                     '; numbers renumbered %d time(s)' % d['renumbered'] if d['renumbered'] else ''))
        # v3.38 DR338: the level (and its chances), its changes, the drops given per item group
        if self.drop_level or self.drop_changes or self.drop_items:
            say('  ' + self.drop_level_line())
        # v3.37 CP337: the performance overlay, the Direct3D chain, the pause Camera page
        if self.backend:
            say('  ' + self.backend)
        if self.rend or self.rend_sw or self.rend_fail:  # v3.39 PR339
            say('  renderer: %s%s%s' % (
                ('at the start %s (row %s, %s)' % self.rend) if self.rend else 'no start line',
                ('; replaced: ' + ', '.join('%s -> %s [%s]' % (o, n, t) for t, o, n in self.rend_sw)) if self.rend_sw else '',
                ('; NOT replaced %d time(s): %s [%s]' % (len(self.rend_fail), self.rend_fail[-1][2], self.rend_fail[-1][0]))
                if self.rend_fail else ''))
        if self.opening or self.open_saved:  # v3.39 PR339
            say('  opening: %s%s%s' % (self.opening or '?',
                                       ('; the boot screens skipped %.1f s after the start' % (self.open_ms / 1000.0))
                                       if self.open_ms is not None else '; the boot screens ran' if self.opening == 'Full' else
                                       '; no skip line (patched? see the install line)',
                                       ('; saved for the next start: ' + ', '.join('%s [%s]' % (l, t) for t, l in self.open_saved))
                                       if self.open_saved else ''))
        if self.perf:
            low = min(self.perf, key=lambda x: x[5] if x[5] > 0 else 1e9)
            worst = max(self.perf, key=lambda x: x[6])
            say('  perf overlay: %d summary line(s), %s; %.1f fps on average; the lowest 1%% low %.1f fps  [%s]; worst frame %.1f ms  [%s]; '
                'Refined at most %.2f ms a frame; address space at most %d of %d MB' % (
                    len(self.perf), hms(sum(x[2] for x in self.perf)),
                    sum(x[3] for x in self.perf) / max(1e-9, sum(x[3] / x[4] for x in self.perf if x[4] > 0)),
                    low[5], low[0], worst[6], worst[0], max(x[7] for x in self.perf), max(x[10] for x in self.perf), self.perf[-1][9]))
        if self.perf_place:  # v3.38 HB338
            say('  perf readout place: ' + '; '.join('%s %s the %s' % (k, v[0], v[1]) for k, v in self.perf_place.items()))
        if self.hb_hide or self.hb_ring:
            last = self.hb_hide[-1] if self.hb_hide else None
            shown = sum(n for k, n in self.hb_ring.items() if k.startswith('shown'))
            whys = Counter()
            for k, n in self.hb_ring.items():
                if k.startswith('shown'):
                    whys['the health gauge' if 'health gauge' in k else 'own wake'] += n
            say('  curse meter Hide Gauge: %s%s; faded out %d time(s), shown again %d (%s)' % (
                last[1] if last else '?', ' (since %s)' % last[0] if last else '', self.hb_ring['faded out'], shown,
                ', '.join('%s %d' % kv for kv in sorted(whys.items())) or 'none'))
        if (self.hb_ref_lines or self.hb_ref['rooms'] or self.hb_boss or self.en359['anchors'] or self.en359['edges'] or
                self.en359_wlong or self.en358_wsite):  # v3.52 HB352  # v3.59 EN359: + the anchors, Walter's sites
            say('  ' + self.hb_ref_line())
        line = self.ft_line()  # v3.59 FT359: the enemy bars' looks under the special filters
        if line:
            say('  ' + line)
        if self.hb_cm42:  # v3.42 SM342: Curse Meter Off / On; Refined never draws the player's ring
            t, who, why = self.hb_pe_ring or ('', '', '')
            line = "  player curse ring: never drawn by Refined (v3.42 Curse Meter Off / On: companion Eileen's ring only); %s" % (
                'Playable Eileen draws it (SH4PE_DrawsPlayerCurse)' if who == 'pe' else
                'nobody draws it (SH4PE_DrawsPlayerCurse %s)' % ('not exported: Playable Eileen up to v1.23.0' if why.startswith('is not')
                                                                else why[3:]) if who else 'Playable Eileen not seen')
            if self.hb_cm42_old:
                line += "; the ini's old CurseMeter=%s read as %s" % self.hb_cm42_old
            say(line + ('  [%s]' % t if t else ''))
        elif self.hb_pe_ring:  # v3.41 CM341
            t, who, why = self.hb_pe_ring
            say('  player curse ring: %s  [%s]' % (
                "drawn by Playable Eileen (SH4PE_DrawsPlayerCurse); Refined draws companion Eileen's ring only" if who == 'pe' else
                'drawn by Refined (SH4PE_DrawsPlayerCurse %s)' % (
                    'not exported: Playable Eileen up to v1.23.0' if why.startswith('is not') else why[3:]), t))
        if self.cam_page[0] or self.cam_page[1]:
            last = self.cam_page[1]
            say('  pause Camera page: opened %d time(s)%s' % (self.cam_page[0], ('; last closed with %s  [%s]' % (last[0][:200], last[1])) if last else ''))
        line = self.mu_line()  # v3.51 MU351: the sound in the Extra Menu's lists
        if line:
            say('  ' + line)
        line = self.ql_line()  # v3.52 QL352: Quick Load's paths
        if line:
            say('  ' + line)
        line = self.pv_line()  # v3.56 PV356: the Effects page's preview (the pause layers, the scene)
        if line:
            say('  ' + line)
        line = self.tv_line()  # v3.59 TV359: Thermal Vision (walls hide heat, the footprints)
        if line:
            say('  ' + line)
        line = self.snd_line()  # v3.52 SND352: Sound (the mode, pan law, reverb, Night Mode)
        if line:
            say('  ' + line)
        line = self.sd_line()  # v3.56 SD356: Background, keep-awake, focus, the sound device
        if line:
            say('  ' + line)
        line = self.fw_line()  # v3.59 SD359: the fault watch, the Direct3D stop
        if line:
            say('  ' + line)
        line = self.ly_line()  # v3.54 OL34: the Options layout (the pages, rows left out, each opening)
        if line:
            say('  ' + line)
        line = self.ol_line()  # v3.53 OL2: the Options look (the glyph budget per page)
        if line:
            say('  ' + line)
        line = self.mn_line()  # v3.56 MN356: value lines, folder clicks, the Extra Menu's text
        if line:
            say('  ' + line)
        line = self.vbx_line()  # v3.58 MN358: the Options value box
        if line:
            say(line)
        # v3.37 INV337: Extended Toolbelt / Stackable Items
        if self.inv or self.inv_sw or self.inv_counts:
            head = '  inventory: '
            if self.inv:
                head += 'at the start toolbelt %s, stackable %s, stack guards %s; ' % self.inv
            head += 'switches: %s' % (', '.join('%s x%d' % (k, n) for k, n in sorted(self.inv_sw.items())) or 'none')
            c = self.inv_counts
            if c:
                head += ('; last counters: flips %d, overflows %d, rounds from page 2 %d, cross-page reloads %d, re-equips %d; '
                         'stacks: merges %d, take-outs %d, stacked breaks %d, dropped %d' % (c[0], c[1], c[4], c[5], c[3], c[7], c[8], c[9], c[10]))
            say(head)
        # v3.43 GT343: Ghost Teleport
        if (self.gt or self.gt_warps or self.gt_no or self.gt_cool or self.gt_chord or self.gt_fades[0] or self.gt_off or
                self.gt_holds or self.gt_zones or self.gt_rooms or self.gt_maps or self.gt_free or self.gt_free_no or
                self.gt_lv or self.gt_lv_flat):  # v3.59 GT359: + levels  # v3.50 TP350: + rooms, maps  # v3.52 TP352: + frees
            line = '  ghost teleport: '
            if self.gt:
                line += '%s at the start (%s); ' % (self.gt[0], self.gt[1])
            n = sum(self.gt_warps.values())
            line += '%d warp(s)%s' % (n, (' (types %s)' % ', '.join('%s x%d' % (k, v) for k, v in sorted(self.gt_warps.items()))) if n else '')
            if self.gt_no:
                line += '; refused %d: %s' % (sum(self.gt_no.values()), ', '.join('%s x%d' % (k, v) for k, v in self.gt_no.most_common(6)))
            if self.gt_cool:
                line += '; %d press(es) waited for the cooldown' % self.gt_cool
            if self.gt_chord:  # v3.44 GT344
                ch = self.gt_chord
                line += '; %d chord(s): Action last %d (taken back %d%s), Action first %d%s' % (
                    sum(ch.values()) - ch['held'], ch['taken back'] + ch['NOT taken back'], ch['taken back'],
                    ', NOT taken back %d' % ch['NOT taken back'] if ch['NOT taken back'] else '', ch['Action first'],
                    ', %d held warp start(s)' % ch['held'] if ch['held'] else '')
            if self.gt_fades[0]:
                line += "; Richard's fade %d time(s)%s" % (self.gt_fades[0], ', %d NEVER DRAWN on her body' % self.gt_fades[1]
                                                          if self.gt_fades[1] else ', drawn each time')
            if self.gt_fdraw or self.gt_fpath:  # v3.58 GT358: the fade's draw path, by filter
                rows = self.gt_fpath
                tot = [sum(r[j] for r in rows.values()) for j in range(9)]
                line += "; the fade's draw %s%s" % (self.gt_fdraw or '?', ' (Clean NOT in: %s)' % self.gt_fnotin if self.gt_fnotin else '')
                if rows:
                    line += (': late %d frame(s) (%d with the depth pre-pass), in place %d (shader 4 %d), %s; by filter %s' % (
                        tot[1], tot[2], tot[3] + tot[4], tot[3], '%d frame(s) MISSED after the actor models' % tot[5] if tot[5]
                        else 'none missed', ', '.join('%s x%d%s' % (k, r[0], ''.join(
                            ', %s %d' % (w, r[j]) for j, w in ((6, 'heat faded'), (7, 'soul faded')) if r[j]))
                            for k, r in sorted(rows.items())), ))
                    if tot[8]:
                        line += ', her shadow hidden %d frame(s)' % tot[8]
            if self.gt_off:
                line += '; called off %d: %s' % (sum(self.gt_off.values()), ', '.join('%s x%d' % (k, v) for k, v in self.gt_off.most_common(4)))
            if self.gt_holds:  # v3.49 GT349
                line += '; %d hold(s) of Run + Action: %d warp(s), the longest %d in a row, %d try(ies) refused while held' % (
                    len(self.gt_holds), sum(w for w, _ in self.gt_holds), max(w for w, _ in self.gt_holds),
                    sum(r for _, r in self.gt_holds))
            if self.gt_zones or self.gt_zone_no:
                line += '; walk-in event zones kept out of: %s%s' % (
                    ', '.join('room %s x%d' % (k, v) for k, v in sorted(self.gt_zones.items()) if v) or 'none armed',
                    ' (%d refusal(s) had spots inside or past one)' % self.gt_zone_no if self.gt_zone_no else '')
            if self.gt_faced:  # v3.50 TP350
                line += '; %d behind the enemy she faces' % self.gt_faced
            if self.gt_rooms:
                rr = self.gt_rooms
                line += ('; %d room visit(s) with tries: %d warp(s) (%d through walls, %d behind the faced enemy, %d ahead), %d '
                         'refused%s, %d Action / Check press(es) taken back in fade-outs' % (
                             len(rr), sum(r[1] for r in rr), sum(r[2] for r in rr), sum(r[3] for r in rr), sum(r[4] for r in rr),
                             sum(r[5] for r in rr), ' (%s)' % ', '.join('%s x%d' % kv for kv in self.gt_room_no.most_common(7))
                             if self.gt_room_no else '', sum(r[6] for r in rr)))
                if self.gt_skip:  # v3.51 TP351
                    line += '; %d enemy(ies) in the cone skipped behind collision (in %d try(ies))' % (self.gt_skip, self.gt_skip_tries)
                if self.gt_room_freed:  # v3.52 TP352
                    line += '; %d time(s) freed from a wall' % self.gt_room_freed
                if self.gt_room_lv[0] or self.gt_room_lv[1]:  # v3.59 GT359
                    line += '; %d up a level, %d down a level (room lines)' % tuple(self.gt_room_lv)
            if self.gt_lv or self.gt_lv_flat:  # v3.59 GT359: across heights
                up = [h for d, h, _ in self.gt_lv if d == 'up']
                dn = [h for d, h, _ in self.gt_lv if d == 'down']
                line += '; across heights: %d up (%s), %d down (%s)%s' % (
                    len(up), 'at most %d u' % max(up) if up else 'none', len(dn), 'at most %d u' % max(dn) if dn else 'none',
                    '; no landing on another level proven in room(s) %s (the flat warp)' % ', '.join(
                        '%s (%s)' % kv for kv in sorted(self.gt_lv_flat.items())) if self.gt_lv_flat else '')
            if self.gt_free or self.gt_free_no:  # v3.52 TP352: the lines themselves
                line += '; freed from a wall: %d after a warp, %d at a warp from inside one%s' % (
                    self.gt_free['after warp'], self.gt_free['at the warp'],
                    ', %d time(s) LEFT inside collision (not moved)' % self.gt_free_no if self.gt_free_no else '')
            if self.gt_maps or self.gt_map_open:
                mp = self.gt_maps
                line += '; floor maps: %d finished%s%s' % (
                    len(mp), ' (%d cells at most, %.1f ms at most)' % (max(m[1] for m in mp), max(m[4] for m in mp)) if mp else '',
                    ', %d room visit(s) left with it NOT finished' % self.gt_map_open if self.gt_map_open else '')
            say(line)
        line = self.fadein_line()  # v3.50 FI350: Fade In
        if line:
            say('  ' + line)
        # v3.46 OM346: Stick Movement (v3.48 OM348: Omni-Directional Movement)
        line = self.omni_line()
        if line:
            say(line)
        line = self.latch_line()  # v3.59 CT359: the control-direction latch, Lock-On Walk
        if line:
            say(line)
        # v3.47 RG347: Regeneration
        line = self.regen_line()
        if line:
            say(line)
        # v3.48 RZ348: the Randomizer
        line = self.rando_line()
        if line:
            say(line)
        line = self.visits_line()  # v3.56 RI356: the world-visit rule (Reroll Visits, Enemies Return)
        if line:
            say(line)
        line = self.monsters_line()  # v3.49 RZ349: its Monsters
        if line:
            say(line)
        line = self.spawnsite_line()  # v3.56 RE356: Spawn Site and the renamed keys
        if line:
            say(line)
        line = self.rzl_line()  # v3.49 RZ349L: its looks
        if line:
            say(line)
        line = self.r302_line()  # v3.50 R302: Room 302's look and walk
        if line:
            say(line)
        line = self.frg_line()  # v3.56 FR356: the rate guard and the final room's sites
        if line:
            say(line)
        line = self.cr_line()  # v3.59 FR359: cut-scenes and Room 302's views at the gameplay rate
        if line:
            say(line)
        # v3.38 EN338: Enemies Follow
        if self.fol or self.fol_wait:
            f = self.fol
            line = ('  enemies follow: %d door(s), %d follower(s) in all (at most %d at one door), %d came through the door, %d '
                    'waited for a later door, %d pursuit(s) ended at doors + %d elsewhere; %d spotted-you mark(s); %d story-rule '
                    'refusal(s)' % (f['doors'], f['follow'], f['most'], f['came'], f['wait'], f['stopped'], f['stops'],
                                    f['spotted'], f['story']))
            if self.fol_wait:
                line += '; waits for space: ' + ', '.join('%s x%d' % (k, n) for k, n in self.fol_wait.most_common(6))
            if f['wheelchairs']:
                line += "; %d v3.37 Wheelchair stray(s) sent home" % f['wheelchairs']
            if f['fresh']:  # v3.41 EN341
                line += "; %d ghost copy(ies) made in another room without their record (the ghosts' way)" % f['fresh']
            g = self.en358  # v3.58 EN358: the ghosts through doors and what became of them
            if g['g_doors'] or g['g_rooms']:
                line += ('; ghosts: %d at %d door(s); in their rooms %d of %d came in (%d seen arriving: %d of them drawn by the '
                         'randomizer; nearest %s units), %d still arriving, %d never in the room (%d room line(s))' % (
                             g['g_door'], g['g_doors'], g['g_in'], g['g_tag'], g['g_first'], g['g_drawn'],
                             self.en358_near if self.en358_near is not None else '-', g['g_arr'], g['g_out'], g['g_rooms']))
            say(line)
        line = self.fol_mode_line()  # v3.49 EF349
        if line:
            say('  ' + line)
        line = self.ef_line()  # v3.50 EF350
        if line:
            say('  ' + line)
        line = self.er_line()  # v3.56 ER356
        if line:
            say('  ' + line)
        # v3.43 FN343: the HD menu font (nothing when MenuFont = Stock)
        fn = self.fn
        if fn['set'] or fn['stock'] or fn['switches']:  # v3.44 FN344: + the live switches
            if fn['set']:
                line = '  menu font: %s (fonts\\%s.sh4font, %d / %d glyphs)' % fn['set']
                line += '; HD glyph page live' if fn['live'] else '; the HD page was never made (no text drawn?)'
                if fn['match'] or fn['nomatch']:
                    line += '; fonts matching the sheet: %s' % (', '.join(str(f) for f in sorted(fn['match'])) or 'none')
                if fn['nomatch']:
                    line += '; NOT matching (stock glyphs): %s' % ', '.join(str(f) for f in sorted(fn['nomatch']))
                if fn['stock']:
                    line += '; then STOCK: %s' % fn['stock']
            elif fn['stock']:
                line = '  menu font: STOCK (%s)' % fn['stock']
            else:
                line = '  menu font: Stock at the start'
                if fn['live']:
                    line += '; HD glyph page live'
            if fn['switches']:  # v3.44 FN344
                line += '; switched live in Options %d time(s), last: %s' % (len(fn['switches']), fn['switches'][-1])
            say(line)
        # v3.47 SB347: the save list's Bank row (its first draw, the mouse's clicks)
        if self.sb_row or self.sb_clicks or self.sb_changes or self.sb_steps or self.sb_bar:  # v3.48 SB348: + the changes  # v3.49 SB349: + the bar
            line = '  bank row: '
            line += ('"%s" in Menu Font %s, %d units wide' % self.sb_row) if self.sb_row else '(no draw line)'
            if self.sb_clicks:
                line += '; %d mouse click(s): %s' % (sum(self.sb_clicks.values()),
                                                     ', '.join('%s x%d' % (k, n) for k, n in self.sb_clicks.most_common()))
                if self.sb_stuck:
                    line += ' (%d at the first / last bank: no change)' % self.sb_stuck
            # v3.48 SB348: the settled changes (one normal line each), the per-step lines
            ch = self.sb_changes
            if ch:
                tot = [sum(c[i] for c in ch) for i in range(4, 9)]
                line += '; %d bank change(s) in the lists: %s%s (%d step(s): keys %d, pad %d, mouse %d; by 10: %d)' % (
                    len(ch), ', '.join('%d -> %d [%s]' % (c[1], c[2], c[0]) for c in ch[:6]),
                    ' (+%d)' % (len(ch) - 6) if len(ch) > 6 else '', tot[0], tot[1], tot[2], tot[3], tot[4])
            if self.sb_steps['n']:
                line += ('; %d "save banks: bank N" line(s) at the normal level (Option 6, or a v3.47 or older list: one a '
                         'step)' % self.sb_steps['n'])
            if self.sb_steps['v']:
                line += '; %d verbose per-step line(s)' % self.sb_steps['v']
            if self.sb_bar:  # v3.49 SB349: the lists' highlight bar; a refused patch in capitals
                c, why = self.sb_bar
                bad = c == 'yellow' and ('not stock' in why or 'could not' in why)
                line += '; list bar %s (%s)' % ('YELLOW: NOT PATCHED' if bad else c, why)
            say(line)
        # v3.40 SH340: Shadow Edge
        if self.sh_edge or self.sh_not or self.sh['composites'] or self.sh_px:  # v3.44 CS344: + the pixel checks
            h = self.sh
            line = '  shadows: %s' % (('NOT installed (%s)' % self.sh_not) if self.sh_not else
                                       ('Edge ' + ', then '.join('%s [%s]' % (e, t) for t, e in self.sh_edge[:6])) if self.sh_edge
                                       else '(no Edge line)')  # v3.44 CS344
            if self.sh_old:
                line += ' (the ini\'s v3.40 Edge=%s read as Soft)' % self.sh_old
            if self.sh_dark:  # v3.42 SM342
                line += '; Darkness ' + ', then '.join('%d %% [%s]' % (d, t) for t, d in self.sh_dark[:6])
                if self.sh_dark_use:
                    line += ' (' + ', '.join('%s: inside %.3f of %.3f' % (k, v[0], v[1]) for k, v in list(self.sh_dark_use.items())[:4]) + ')'
            if self.sh_c1:  # v3.43 SH343: what the draws used (the device's c1.a), the page and play side by side
                parts = []
                for (path, k), (c1, light, rb, t) in list(self.sh_c1.items())[:6]:
                    other = self.sh_c1.get(('page' if path == 'play' else 'play', k))
                    parts.append('%s %s: %.3f%s, %d %% light [%s]%s' % ('in play' if path == 'play' else 'page', k, c1,
                                 '' if rb else ' (not read back)', light, t,
                                 ' MISMATCH with the %s %.3f' % ('page' if path == 'play' else 'play', other[0])
                                 if other and abs(other[0] - c1) > 0.0015 else ''))
                line += '; c1.a drawn: ' + ', '.join(parts)
            if self.sh_px:  # v3.44 CS344: what reached the pixels (the test draw read back)
                bad = [p for p in self.sh_px if p[3] != 'ok']
                line += '; pixel check %d ok of %d' % (len(self.sh_px) - len(bad), len(self.sh_px))
                line += ' (' + ', '.join('%s %s: %s' % (p[1], p[2], ('%.3f of %.3f' % (p[4], p[5])) if p[3] == 'ok' else
                                                        ('%s: %s' % (p[3], p[6]))) + ' [%s]' % p[0]
                                         for p in (bad + [p for p in self.sh_px if p[3] == 'ok'])[:6 if not full else 64]) + ')'
            if self.sh_plans:
                line += '; plans: ' + ', '.join('%s %s' % kv for kv in self.sh_plans.items())
            if h['composites']:
                line += ('; %d composite(s) in %d room line(s), %d soft, at most %d in a frame; ours %.3f ms at most (room %s, %.3f on average)'
                         % (h['composites'], h['rooms'], h['soft'], self.sh_worst[0], self.sh_worst[1], self.sh_worst[3], self.sh_worst[2]))
            if h['hard'] or h['nodark'] or h['unexpl']:  # v3.52 SH352: the rooms with a hard share, and why
                def rooms(i):
                    ks = sorted((k for k, v in self.sh_rooms.items() if v[i]), key=lambda k: -self.sh_rooms[k][i])
                    return ks if full else ks[:6]
                if h['hard']:
                    ks = rooms(2)
                    line += '; HARD share: %d composite(s) in %d room(s): %s' % (h['hard'], len([v for v in self.sh_rooms.values() if v[2]]), ', '.join(
                        '%s %d of %d (%d %%: %s)' % (k, self.sh_rooms[k][2], self.sh_rooms[k][0], 100 * self.sh_rooms[k][2] // max(1, self.sh_rooms[k][0]),
                                                     ', '.join('%s %d' % kv for kv in self.sh_rooms[k][5].most_common(3)) or '?') for k in ks))
                if h['nodark']:
                    line += ("; %d with no darkness (the game's own 0: no shadow shows, nothing to soften) in room(s) %s"
                             % (h['nodark'], ', '.join(rooms(3))))
                if h['unexpl']:
                    line += '; %d not soft with no reason given (a log before v3.52) in room(s) %s' % (
                        h['unexpl'], ', '.join('%s %d of %d%s' % (k, self.sh_rooms[k][4], self.sh_rooms[k][0],
                                                                 " (the game's own darkness is 0 there: no shadow shows)" if k == '4:1F' else '')
                                               for k in rooms(4)))
            if self.sh_ns:  # v3.52 SH352
                line += '; %d not-soft reason line(s) (%s)' % (len(self.sh_ns), ', '.join('%s [%s]: %s' % (r, t, w.split(':')[0].split(' (')[0])
                                                                                           for t, r, w in self.sh_ns[:3 if not full else 64]))
            if self.sh_sw:  # v3.58 GX358: the stock Realtime Shadow switch and what the game drew with it Off
                offs = [x for x in self.sh_sw if x[1] == 'Off']
                line += '; Shadows (Realtime Shadow): %d line(s), %s at the end%s' % (
                    len(self.sh_sw), self.sh_sw[-1][1], (', Off %d time(s) (%s)' % (len(offs), ', '.join(
                        '%s [%s]' % (x[2] or '?', x[0]) for x in offs[-3:]))) if offs else '')
            if self.sh_off_rooms:
                line += "; drawn with Shadows Off (the game's own Off list): %s" % ', '.join(
                    '%s %d' % kv for kv in sorted(self.sh_off_rooms.items())[:6 if not full else 64])
            if self.sh_pv_off:
                line += "; the page's preview showed the game's Off %d time(s)" % self.sh_pv_off
            if self.sh_vram:
                line += '; render targets at most %.1f MB' % self.sh_vram
            if h['kept'] or h['scene'] or h['figure']:
                line += '; preview: %d pause copy(ies), page shown the scene %d / the figure %d time(s)' % (h['kept'], h['scene'], h['figure'])
                if self.sh_copy[0] or self.sh_copy[1]:  # v3.41 SH341
                    line += ' (targets made %d time(s), kept %d; the slowest copy %.2f ms%s)' % (
                        self.sh_copy[0], self.sh_copy[1], self.sh_copy[2], '; freed %d time(s): low address space' % h['lowva'] if h['lowva'] else '')
            p = self.fx_pic
            if p[0] or p[1] or p[3]:  # v3.41 SH341: the pause hitch's source
                line += '; pause picture: %d on the GPU (target made %d, kept %d; at most %.2f ms), %d read back from the GPU (worst %.1f ms)' % (
                    p[0] + p[1], p[0], p[1], p[2], p[3], p[4])
            if self.sh_rib_state or h['rib']:  # v3.49 SH349
                line += '; Tremer wall shadows: %s' % (self.sh_rib_state or 'the Darkness')
                if h['rib']:
                    line += ', %d draw(s) in %d room line(s), %d at the Darkness' % (h['rib'], h['rib_rooms'], h['rib_dark'])
                if self.sh_rib_vals:
                    line += ' (alpha ' + ', '.join('%s: %d %%' % kv for kv in list(self.sh_rib_vals.items())[:4]) + ')'
            if self.sh_off:
                line += '; OFF from %s: %s (%s)' % self.sh_off
            say(line)
        # v3.43 DF343: Depth of Field
        if (self.dof_set or self.dof_not or self.dof['asked'] or self.dof_mig or self.dof_close_rooms or self.dof_note59 or
                self.dof_spirit):  # v3.45 QS345 / v3.52 FX352 / v3.59 DF359
            d = self.dof
            line = '  depth of field: %s' % (('NOT installed (%s)' % self.dof_not) if self.dof_not else
                                              ', then '.join('%s [%s]' % (e, t) for t, e in self.dof_set[:6]))
            if d['asked']:
                line += ('; drawn in %d of %d asked frame(s) over %d room line(s); ours %.3f ms at most (room %s, %.3f on average)'
                         % (d['drawn'], d['asked'], d['rooms'], self.dof_worst[0], self.dof_worst[2], self.dof_worst[1]))
            if self.dof_why:  # v3.44 DF344: why the asked frames were not drawn (the most frequent first)
                line += '; not drawn: ' + ', '.join('%s %d' % kv for kv in self.dof_why.most_common(4))
            if self.dof_design:
                line += '; skipped by design: ' + ', '.join('%s %d' % kv for kv in self.dof_design.most_common(3))
            if self.dof_probe:
                line += '; probes: ' + ', '.join('%s %d' % kv for kv in sorted(self.dof_probe.items()))
                if self.dof_skip:
                    line += ' (off in ' + ', '.join(self.dof_skip[:6]) + ')'
            if self.dof_noread:  # v3.45 RB345
                line += '; not probed (no read back) in %d room(s)' % self.dof_noread
            if d['kept']:
                line += '; %d pause copy(ies) for the page' % d['kept']
            if self.dof_first or self.dof_noblend:  # v3.47 DF347: eased in, or whole levels (no blend shader)
                line += ('; NOT eased in (no blend shader %d time(s): whole levels)' % self.dof_noblend if self.dof_noblend or
                         self.dof_first == 'whole levels' else '; eased in (%s slices)' % (self.dof_ramps or 'quarter-level'))
            if self.dof_close or self.dof_close_rooms:  # v3.52 FX352
                line += '; the player close to the camera: drawn in %d frame(s), first in %d room(s)' % (
                    self.dof_close, len(self.dof_close_rooms))
            if self.dof_off:
                line += '; OFF from %s: %s' % self.dof_off
            if self.dof_mig:  # v3.45 QS345
                line += ('; the ini\'s v3.44 DofStrength=%s read as %s (%s)'
                         % (self.dof_mig[0], self.dof_mig[1], 'rewritten' if self.dof_mig[2] else 'NOT rewritten'))
            if self.dof_note59:  # v3.59 DF359
                line += ('; the ini\'s DofStrength=%s kept (v3.59\'s %s is a bit gentler; Ultra = v3.58\'s High)' % self.dof_note59)
            if self.dof_spirit:  # v3.59 DF359
                line += '; kept out of Spirit Vision from %s (its world sharp)' % self.dof_spirit
            if self.dof_spirit_keep:  # v3.59 DF359
                line += '; DofSpirit=1: blurred under Spirit Vision too (v3.58\'s order)'
            say(line)
        # v3.43 PP343: Bloom, Colour Grading, Otherworld Lens
        if self.pp or self.pp_ini_off or self.pp_set or self.pp_mig or self.pp_lens_rooms:  # v3.52 FX352: + the migration, rooms
            if self.pp_ini_off:
                line = '  post effects: off ([ScreenEffects] PostEffects=0)'
            elif self.pp:
                line = '  post effects: at the start Bloom %s, Colour Grading %s, Otherworld Lens %s (device reset hook %s)' % (
                    self.pp[1], self.pp[2], self.pp[3], self.pp[0])
            else:
                line = '  post effects: (no install line)'
            if self.pp_set:
                line += '; changes: ' + ', '.join('%s %s [%s]' % (k, v, t) for t, k, v in self.pp_set[:8 if not full else 64])
            if self.pp_drawn:
                line += '; drawn: ' + ', '.join(sorted(self.pp_drawn))
            if self.pp_px:  # v3.44 CS344: the grade's pixel checks
                bad = [p for p in self.pp_px if p[2] != 'ok']
                line += '; grading pixel check %d ok of %d (' % (len(self.pp_px) - len(bad), len(self.pp_px)) + ', '.join(
                    ('%s ok, %d off, %.1f changed [%s]' % (p[1], p[3], p[4], p[0])) if p[2] == 'ok' else
                    ('%s %s: %s [%s]' % (p[1], p[2], p[6], p[0])) for p in (bad + [p for p in self.pp_px if p[2] == 'ok'])[:5]) + ')'
            if self.ps_lint:
                line += '; shaders: proven forms only'
            if self.pp_chain:
                line += '; bloom targets ' + ', '.join('%s %.1f MB' % kv for kv in self.pp_chain.items())
            if self.pp_lens:
                line += '; lens faded in %d / out %d time(s), at most %d %% (%s)' % (
                    self.pp_lens['in'], self.pp_lens['out'], self.pp_lens_max[0], self.pp_lens_max[1])
            if self.pp_lens_rooms:  # v3.52 FX352: when it shows
                shown = sum(r[1] for r in self.pp_lens_rooms)
                secs = sum(r[2] for r in self.pp_lens_rooms)
                line += '; lens shown %d of %d s in %d room line(s), at most %d %% strength' % (
                    shown, secs, len(self.pp_lens_rooms), max(r[3] for r in self.pp_lens_rooms))
            if self.pp_mig:  # v3.52 FX352
                line += ('; the ini\'s v3.51 Bloom=%s read as %s (%s)'
                         % (self.pp_mig[0], self.pp_mig[1], 'rewritten' if self.pp_mig[2] else 'NOT rewritten'))
            if self.pp_notf:
                line += '; not under a filter: ' + ', '.join('%s x%d' % kv for kv in sorted(self.pp_notf.items()))
            if self.pp_fail:
                line += '; OFF this session: ' + ', '.join('%s (%s) [%s]' % (e, w, t) for t, e, w in self.pp_fail)
            say(line[:700 if not full else 4000])  # v3.44 CS344: 700 (the pixel checks)
        if self.rb or self.rb_sw:  # v3.45 RB345: the GPU -> CPU read back (the pixel checks, the depth probe)
            if not self.rb:
                line = '  read back: (no path line)'
            elif self.rb[1] == 'ok':
                line = '  read back: through %s [%s] (first: %s)' % (self.rb[2], self.rb[0], self.rb[3])
            else:
                line = '  read back: NONE on this backend [%s]: pixel checks off, depth probe off (%s)' % (self.rb[0], self.rb[2])
            if self.rb_sw:
                line += '; %d switch(es) / failure(s) of a path that worked: ' % len(self.rb_sw) + ', '.join(
                    '%s %s [%s]' % (k, p, t) for t, k, p in self.rb_sw[:4])
            say(line)
        if self.px_align or self.px_src:  # v3.47 PX347: the pixel checks' pattern
            meas = [a for a in self.px_align if a[2] is not None]
            line = '  pixel check pattern: '
            if meas:
                lo, hi = min(a[2] for a in meas), max(a[2] for a in meas)
                line += 'the test draw samples %s texel(s) %s of the texel centres in %d check(s) (%s)' % (
                    ('%.2f' % abs(lo)) if lo == hi else '%+.2f .. %+.2f' % (lo, hi), 'left' if hi < 0 else 'right' if lo > 0 else 'around',
                    len(meas), ', '.join('%s %+.2f [%s]' % (a[1], a[2], a[0]) for a in meas[:3]))
            else:
                line += 'the offset not measured'
            if len(self.px_align) > len(meas):
                line += '; %d not measurable' % (len(self.px_align) - len(meas))
            if self.px_src:
                line += '; the source: %d as asked, %d with a Clear that did not land, %d not read back' % (
                    self.px_src['as asked'], self.px_src['lost'], self.px_src['not read back'])
            if self.px_lost:
                line += ' (' + ', '.join('%s: %d cell(s) [%s]' % (w, n, t) for t, w, n in self.px_lost[:3]) + ')'
            say(line[:500 if not full else 3000])
        # v3.41 VW341: Room 302's window views, the Borderless edges
        if self.vw or self.vw_views:
            line = '  window views: %s' % (self.vw or '(no install line)')
            if self.vw_views:
                line += '; %d view(s) with black sides (%s)' % (len(self.vw_views), ', '.join(sorted(set(v for _, v in self.vw_views))))
            say(line[:300 if not full else 2000])
        if self.edges:
            line = '  borderless edges covered %d time(s) by windows above the game (not the game\'s picture): ' % len(self.edges)
            line += ' | '.join('%s [%s, game %s]' % (what, t, front) for t, n, what, front in self.edges[:3 if not full else 24])
            if self.edges_free:
                line += '; free again %d time(s)' % self.edges_free
            say(line[:400 if not full else 4000])
        if self.e2d:  # v3.42 EG342
            parts = []
            for size, how in self.e2d:
                p = size + ('' if how == 'aligned' else ' (PixelAlign2D=0: the game\'s own mapping)')
                if size in self.e2d_sides:
                    p += ' (black sides %s)' % self.e2d_sides[size]
                parts.append(p)
            say(('  2D edges: canvases on pixel edges (column 0 / row 0 covered) at ' if any(h == 'aligned' for _, h in self.e2d)
                 else '  2D edges: ') + ', '.join(parts))
        # features
        if self.feats:
            never = [n for n, e in self.feats.items() if e[1] == 0]
            say('  features (acted in N of the rooms it was on):')
            for n, e in self.feats.items():
                vals = '/'.join(v for v, _ in e[3].most_common(3))
                say('    %-15s %s %d of %d room(s)%s (%s)' % (
                    n, 'NEVER acted in' if e[1] == 0 else 'acted in', e[1], e[0],
                    '' if e[1] == 0 else ', total %d' % e[2], vals))
            if never:
                say('  never acted while on: ' + ', '.join(never))
        elif self.end_feats:
            say('  features: ' + self.end_feats[:300])
        # spam
        noisy = [(n, k) for k, n in self.kinds.most_common(4) if n >= max(150, self.lines // 8)]
        if noisy:
            say('  noisy lines (rate-limit candidates):')
            for n, k in noisy:
                say('    %5dx %s' % (n, k))
        return out

    def crash_dump(self, folder):
        if not folder or self.t0 is None:
            return None
        for d in DUMPS:
            p = os.path.join(folder, d)
            if os.path.exists(p):
                import time
                lt = time.localtime(os.path.getmtime(p))
                t = lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec
                lo, hi = self.t0 % 86400, (self.t0 + self.span) % 86400
                inside = lo <= t <= hi + 120 if lo <= hi else (t >= lo or t <= hi + 120)
                if inside and time.time() - os.path.getmtime(p) < 3 * 86400:
                    return '%s (%s)' % (d, time.strftime('%Y-%m-%d %H:%M:%S', lt))
        return None


    # ------------------------------------------------------------ v3.40 LG340: one row of the comparison table
    def start_dt(self):
        """The session's start: its "log: started" line (v3.40), else a rotated log's file name (its minute) with the first
        line's seconds, else the file's last write date with the first line's time (a day earlier past midnight)."""
        if self.started:
            return self.started
        tod = self.t0 % 86400 if self.t0 is not None else None
        m = ROTATED.match(self.name)
        if m:
            y, mo, d, h, mi = (int(x) for x in m.groups()[:5])
            try:
                return datetime.datetime(y, mo, d, h, mi, int(tod % 60) if tod is not None else 0)
            except ValueError:
                pass
        try:
            last = datetime.datetime.fromtimestamp(os.path.getmtime(self.path))
        except OSError:
            return None
        if tod is None:
            return last
        day = last.date()
        if tod > last.hour * 3600 + last.minute * 60 + last.second:
            day -= datetime.timedelta(days=1)
        return datetime.datetime(day.year, day.month, day.day) + datetime.timedelta(seconds=int(tod))

    def renderer(self):
        """(label, variant, runtime): the Renderer row's dgVoodoo conf at the start (v3.39+; before it the header's d3d9.dll
        line), and the Direct3D runtime the display chain names (D3D12 / D3D11 = dgVoodoo's output, D3D9 = stock or
        passthru)."""
        label, var = '?', '?'
        why = self.rend[0] if self.rend else None
        if why:
            m = REND_CONF.match(why)
            k = REND_KNOWN.match(why)
            if m:
                label, var = m.group(1), m.group(2)
            elif k:
                label, var = k.group(1), k.group(1)
            elif why.startswith('d3d9.dll is missing'):
                label, var = 'no dgVoodoo', 'none'
            elif why.startswith('d3d9.dll has md5'):
                label, var = 'other d3d9.dll', 'other'
            elif 'is no manifest row' in why:
                label, var = 'custom conf', 'custom'
            elif why.startswith('dgVoodoo_variants'):
                label, var = 'dgVoodoo, no manifest', 'nomanifest'
        elif self.d3d_head == 'none':
            label, var = 'no dgVoodoo', 'none'
        elif self.d3d_head == 'dgvoodoo':
            label, var = 'dgVoodoo (conf not logged)', 'dgvoodoo'
        chain = (self.display or '').split('chain: ', 1)[-1].lower() if self.display else ''
        api = 'D3D12' if 'd3d12.dll' in chain else 'D3D11' if 'd3d11.dll' in chain else 'D3D9' if 'd3d9.dll' in chain else '?'
        return label, var, api

    def row(self):
        """The numbers of the comparison table (None = not in this log)."""
        r = dict(start=self.start_dt(), version=self.version or '?', name=self.name)
        r['label'], r['variant'], r['api'] = self.renderer()
        e = self.end or ''
        m = END_AFTER.match(e)
        r['ended'] = (m.group(1) if m else 'yes') if self.end else ('MISSING' if self.header else '?')
        r['secs'] = (int(m.group(2)) * 3600 + int(m.group(3)) * 60 + int(m.group(4))) if m else self.span
        r['visits'] = int(m.group(5)) if m else len(self.rooms)
        r['rooms'] = int(m.group(6)) if m and m.group(6) else len(set('%s:%s' % (s, x) for s, x, _ in self.rooms))
        r['play'] = sum(x['play'] for _, _, x in self.rooms)
        f = END_FRAMES.search(e)
        framed = [x for _, _, x in self.rooms if x.get('frames')]
        if f:
            r['frames'], r['mean'], r['p99'], r['worst'], r['approx'] = int(f.group(1)), float(f.group(2)), float(f.group(3)), \
                float(f.group(4)), False
        elif framed:  # no end line (a crash): the room lines', frame-weighted; p99 = the worst room's (an upper bound)
            n = sum(x['frames'] for x in framed)
            r['frames'], r['approx'] = n, True
            r['mean'] = sum(x['mean'] * x['frames'] for x in framed) / n
            r['p99'] = max(x['p99'] for x in framed)
            r['worst'] = max(x['worst'] for x in framed)
        else:
            r['frames'], r['mean'], r['p99'], r['worst'], r['approx'] = 0, None, None, None, False
        # the world only: stage 0 (the title and its menus, drawn at 30 fps) left out, so the renderers compare on gameplay
        world = [x for s, _, x in self.rooms if x.get('frames') and int(s, 16) != 0]
        r['wplay'] = sum(x['play'] for s, _, x in self.rooms if int(s, 16) != 0)
        r['wframes'] = sum(x['frames'] for x in world)
        r['wmean'] = sum(x['mean'] * x['frames'] for x in world) / r['wframes'] if r['wframes'] else None
        # rooms' p99s do not merge into one: their frame-weighted mean (a short entry room cannot dominate it)
        r['wp99'] = sum(x['p99'] * x['frames'] for x in world) / r['wframes'] if r['wframes'] else None
        sp = END_SPIKES.search(e)
        r['spikes'] = int(sp.group(1)) if sp else sum(x.get('spikes', 0) for x in framed)
        r['stalls'] = int(sp.group(2)) if sp else sum(x.get('stalls', 0) for _, _, x in self.rooms)
        va = END_VA.search(e)
        r['va'] = int(va.group(1)) if va else None
        a = END_ANOM.search(e)
        seen = sum(g[0] for g in self.anomalies.values())
        r['anomalies'] = max(seen, int(a.group(1)) - self.rz_nomodels_anom) if a else seen  # v3.58 MN358: not the by-design lines
        if self.perf:
            fr = sum(x[3] for x in self.perf)
            r['perf'] = (len(self.perf), fr / max(1e-9, sum(x[3] / x[4] for x in self.perf if x[4] > 0)),
                         min(x[5] for x in self.perf if x[5] > 0) if any(x[5] > 0 for x in self.perf) else 0.0)
        else:
            r['perf'] = None
        return r


SHORT_PLAY = 600  # v3.40 LG340: sessions with less play than this (s) are marked: too short to rank a renderer on


def _f(v, fmt='%.1f', none='-'):
    return none if v is None else fmt % v


def compare(sessions):
    """v3.40 LG340: the per-session comparison table (oldest first) and the per-renderer summary, as lines."""
    rows = [s.row() for s in sessions]
    rows.sort(key=lambda r: (r['start'] or datetime.datetime.min, r['name']))
    out = []
    say = out.append
    say('== %d session(s) compared, oldest first. world: the rooms outside the title (stage 0, 30 fps menus), w.p99 = its rooms\' '
        'p99, frame-weighted; all: every play frame (the session end line; ~ = from the room lines, no end line); * = under %d min of world '
        'play: too short to rank' % (len(rows), SHORT_PLAY // 60))
    head = ('%-3s %-16s %-8s %-22s %-17s %-5s %8s %7s %8s %6s %6s %6s %6s %6s %6s %6s %-17s %5s %s' % (
        '#', 'started', 'version', 'renderer', 'variant', 'API', 'length', 'rooms', 'world', 'w.mean', 'w.p99', 'mean', 'p99',
        'worst', 'spikes', 'stalls', 'perf 5 min: fps/1%', 'ANOM', 'end'))
    say(head)
    for i, r in enumerate(rows, 1):
        perf = '%d: %.1f / %.1f' % r['perf'] if r['perf'] else '-'
        say('%-3s %-16s %-8s %-22s %-17s %-5s %8s %7s %8s %6s %6s %6s %6s %6s %6d %6d %-17s %5d %s' % (
            '%d%s' % (i, '*' if r['wplay'] < SHORT_PLAY else ''), r['start'].strftime('%Y-%m-%d %H:%M') if r['start'] else '?',
            r['version'][:8], r['label'][:22], r['variant'][:17], r['api'], hms(r['secs']), '%d/%d' % (r['visits'], r['rooms']),
            hms(r['wplay']), _f(r['wmean']), _f(r['wp99']), _f(r['mean']), ('~' if r['approx'] else '') + _f(r['p99']),
            _f(r['worst']), r['spikes'], r['stalls'], perf, r['anomalies'], r['ended']))
    # per renderer (by its variant and the runtime it ran on): the world frames, frame-weighted
    groups = OrderedDict()
    for r in rows:
        groups.setdefault((r['variant'], r['api']), []).append(r)
    say('== by renderer (world frames, frame-weighted over its sessions; w.p99: the median of the sessions\' and the worst; '
        'spikes over every play frame)')
    say('%-22s %-17s %-5s %8s %9s %8s %6s %13s %9s %6s %7s' % ('renderer', 'variant', 'API', 'sessions', 'world', 'frames',
                                                             'w.mean', 'w.p99 med/max', 'spikes/h', 'ANOM', 'crashes'))
    for (var, api), rs in groups.items():
        fr = sum(r['wframes'] for r in rs)
        mean = sum(r['wmean'] * r['wframes'] for r in rs if r['wmean'] is not None) / fr if fr else None
        p99s = sorted(r['wp99'] for r in rs if r['wp99'] is not None)
        med = p99s[len(p99s) // 2] if len(p99s) % 2 else (p99s[len(p99s) // 2 - 1] + p99s[len(p99s) // 2]) / 2 if p99s else None
        wplay = sum(r['wplay'] for r in rs)
        play = sum(r['play'] for r in rs)
        say('%-22s %-17s %-5s %8s %9s %8d %6s %13s %9s %6d %7d' % (
            rs[-1]['label'][:22], var[:17], api, '%d%s' % (len(rs), '' if wplay >= SHORT_PLAY else '*'), hms(wplay), fr, _f(mean),
            '%s / %s' % (_f(med), _f(p99s[-1] if p99s else None)),
            _f(sum(r['spikes'] for r in rs) * 3600.0 / play if play > 0 else None), sum(r['anomalies'] for r in rs),
            sum(1 for r in rs if r['ended'] == 'MISSING')))
    ranked = [(k, rs) for k, rs in groups.items() if sum(r['wplay'] for r in rs) >= SHORT_PLAY]
    if len(ranked) < 2:
        say('  (fewer than two renderers with %d min of play or more: play a longer session on each to compare)' % (SHORT_PLAY // 60))
    return out


_DIGESTS = {}


def _digest(p):
    """v3.40 REV340: a file's sha256 (cached by path, size and mtime)."""
    st = os.stat(p)
    key = (os.path.abspath(p), st.st_size, st.st_mtime_ns)
    if key not in _DIGESTS:
        h = hashlib.sha256()
        with open(p, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        _DIGESTS[key] = h.digest()
    return _DIGESTS[key]


def same_file(a, b):
    """The same log twice (SH4Refined.prev.log and its copy in logs\\, or a file named twice). v3.40 REV340 (Codex
    LG340-001): by size, then by content; never by size and time alone (two same-size sessions written in one second
    stay two)."""
    try:
        if os.path.samefile(a, b):
            return True
        if os.path.getsize(a) != os.path.getsize(b):
            return False
        return _digest(a) == _digest(b)
    except OSError:
        return False


def folder_logs(folder):
    """v3.40 LG340: a game folder's session logs, newest first: SH4Refined.log, SH4Refined.prev.log (unless logs\\ holds
    the same file), then logs\\'s own rotated logs (newest first by name)."""
    files = []
    cur = os.path.join(folder, 'SH4Refined.log')
    prev = os.path.join(folder, 'SH4Refined.prev.log')
    logs = os.path.join(folder, 'logs')
    rot = []
    if os.path.isdir(logs):
        def newest(n):  # v3.40 REV340: by start, then " #n" (a plain name sorts after " #2" as text)
            g = ROTATED.match(n).groups()
            return tuple(int(x) for x in g[:5]) + (int(g[6][2:]) if g[6] else 1, n)
        rot = [os.path.join(logs, n) for n in sorted((n for n in os.listdir(logs) if ROTATED.match(n)), key=newest, reverse=True)
               if os.path.isfile(os.path.join(logs, n))]
    if os.path.exists(cur):
        files.append(cur)
    if os.path.exists(prev) and not any(same_file(prev, r) for r in rot):
        files.append(prev)
    files += [r for r in rot if not (os.path.exists(cur) and same_file(cur, r))]
    return files


def main(argv):
    full = '--full' in argv
    only_table = '--table' in argv
    every = '--all' in argv
    args = [a for a in argv if a not in ('--full', '--table', '--all')]
    files, folder = [], None  # (path, report it)
    if not args:
        args = [GAME_DIR]
    for a in args:
        if os.path.isdir(a):
            folder = a
            for i, p in enumerate(folder_logs(a)):  # a report for this session and the one before (--all: every one)
                if not any(same_file(p, f) for f, _ in files):
                    files.append((p, i < 2))
        elif os.path.exists(a):
            if not any(same_file(a, f) for f, _ in files):
                files.append((a, True))
            folder = folder or os.path.dirname(os.path.abspath(a))
        else:
            print('not found: ' + a)
    if not files:
        print('no SH4Refined.log found')
        return 2
    sessions = []
    for p, rep in files:
        s = Session(p)
        s.read()
        sessions.append(s)
        if not only_table and (rep or every):
            print('\n'.join(s.report(full, folder)))
    if len(sessions) > 1 or only_table:
        print('\n'.join(compare(sessions)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
