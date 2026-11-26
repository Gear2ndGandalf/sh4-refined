"""v3.33 LOG333: tests of tools/logcheck.py on synthetic logs (standard library only).

    python test\\logcheck_test.py

A v3.33 log (session header, room lines in the format loghealth.inc writes, ANOMALY lines, a missing end line), a
complete one with its end line, and a v3.32 log (no header: the fallbacks). Exit 0 when every check passed.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'tools'))
import logcheck  # noqa: E402

FAILS = []


def check(cond, msg):
    print(('ok    ' if cond else 'FAIL  ') + msg)
    if not cond:
        FAILS.append(msg)


V333 = """23:58:00.001 SH4 Refined v3.33.0 loading (winmm.dll proxy, game base 00400000)
23:58:00.002 log: level 2 (verbose; [Log] Level 1 normal, 2 verbose, 3 trace); grep "ANOMALY:" for states a feature should never reach
23:58:00.500 session: SH4 Refined v3.33.0 (file version 3.33.0.0), winmm.dll md5 0123456789abcdef0123456789abcdef, 1216512 bytes, built Nov 20 2026 12:00:00; log level 2
23:58:00.510 session: exe SILENT HILL 4.exe md5 15a85581eca3d0664089c440b0acada1 (GOG v7, the build every site is made for), 2793472 bytes, base 00400000
23:58:00.511 session: Playable Eileen loaded (dsound.dll v1.18.2.0)
23:58:00.512 settings not default: [Window] FrameRate=60; [Gameplay] Echo=Shadow Cast; [ScreenEffects] FilterStrength=Medium (hidden key)
23:58:00.513 patches: 181 code range(s) listed, 120 byte check(s) at the start, 0 did not match, 0 write(s) failed
23:58:00.514 [v] patches: 12 of 12 core features installed
23:59:09.000 [v] load: game time 35272 (first world frame; was 0), stage 2 room 4, difficulty 2, kills 12; the save's Supply Drops ledger 00 31000000000000 (this world visit 1 of 15), pickup records free 9 of 32 (0 of them drops)
23:59:09.001 ANOMALY: supply drops: the loaded game holds 23 of 32 pickup records (9 free, 0 of them drops): Supply Drops needs 17 free to place a drop, so none appears until records free up (stage 2 room 4, game time 35272)
23:59:10.000 display: Borderless, game size 2560x1440, window client 2560x1440, gameplay rate 60 (FrameMode 0); chain: d3d8.dll (game folder, v1.0.0.0) ori_d3d8.dll (game folder) d3d9.dll (game folder, v2.87.6.0)
00:00:30.000 [v] room: 2:04 left after 80.0 s (play 70.0, cutscene 0.0, paused 10.0); 4200 frames: mean 16.7 ms, p99 21.2, worst 153.7 at +0.4 s (entry; Refined 1.20 ms; echo draw 153.7 ms), spikes 2 (over 50 ms), stalls 0; VA 910 of 2048 MB (margin 1138)
00:00:30.001 [v] room: 2:04 features: Echo=Shadow Cast x3 acted 4000 f; After Shock=Long acted 0 f (stuns 0, walked 3000 u); Supply Drops=Generous acted 0 (drops) (kills 3 + copies 1, ledger here 0 of 15)
00:01:40.000 ANOMALY: frame: a 412 ms freeze in play, stage 2 room 2, 12.3 s into the room (target 16.7 ms; Refined's per-frame work 0.80 ms of it)
00:01:50.000 [v] room: 2:02 left after 70.0 s (play 60.0, cutscene 5.0, paused 5.0); 3600 frames: mean 16.9 ms, p99 25.0, worst 412.0 at +12.3 s (in play; Refined 0.80 ms), spikes 4 (over 50 ms), stalls 1; VA 920 of 2048 MB (margin 1128)
00:01:50.001 [v] room: 2:02 features: Echo=Shadow Cast x3 acted 3000 f; After Shock=Long acted 0 f (stuns 0, walked 2500 u); Supply Drops=Generous acted 1 (drops) (kills 4 + copies 0, ledger here 1 of 15)
00:02:00.000 ANOMALY: extra menu: the Quick Save row changed between grey and available 4 times in 3.2 s of one pause (it should hold still while paused; grey because: files are loading; stage 2 room 3)
00:02:30.000 ANOMALY: extra menu: the Quick Save row changed between grey and available 6 times in 5.0 s of one pause (it should hold still while paused; grey because: files are loading; stage 2 room 3)
00:03:00.000 [v] room: 2:03 left after 70.0 s (play 0.0, cutscene 70.0, paused 0.0); no play frames; stalls 0; VA 920 of 2048 MB (margin 1128)
00:03:10.000 ANOMALY: after shock: After Shock on for 3 rooms in a row in which she walked 1500 units or more (not Room 302) and no trail copy was drawn in any (last room 2:05, walked 2600 u)
00:03:11.000 echo: Echo faulted (code C0000005 at 12345678): the feature is off for this session
"""

V333_END = V333 + """00:04:00.000 session: end (process exit) after 0:06:00: 3 room visit(s) (3 rooms); frames 7800: mean 16.8 ms, p99 23.0, worst 412.0 in 2:02 at +12.3 s, spikes 6, stalls 1; VA peak 920 of 2048 MB (margin 1128); 4 ANOMALY line(s); 1 feature(s) faulted; ini fine
00:04:00.001 session: features: Echo acted in 2 of 2 room(s) on (7000 f); After Shock acted in 0 of 2 room(s) on (0 f)
"""

V332 = """22:31:52.674 SH4 Refined v3.32.0 loading (winmm.dll proxy, game base 00400000)
22:31:52.687 gameplay: MoveSpeed x2, MedallionDurability x4, OneHitKill Off, DeathPresence Off, AfterImage Off, Echo Shadow Cast x3, SupplyDrops Generous; Playable Eileen loaded (its regeneration x1)
22:31:52.700 items: 004EFBFD is not the stock HUD call (E9 00 00 00 00; another mod?): no item shine
22:45:23.449 enemies: room 2:4 copies x4: 1 placements (1 eligible, 1 in the room), 3 of 3 copies made
22:54:09.814 echo: stage 2 room 4: the shadows' slowest frame 10.79 ms (body passes 8.20, sprites 0.46; the tick 0.16)
""" + "".join("22:55:%02d.000 screen effects: Spirit Vision: a downed ghost (class 09, countdown %d.00 s, not running): its veins retract\n" % (i % 60, i)
              for i in range(200))


# v3.35 FX335: the room line's worst-frame context from the frame ledger, "after a load", the effect budget's lines, the
# warm-up line (the formats loghealth.inc, budget.inc and echo.inc write)
V335 = """10:00:00.001 SH4 Refined v3.35.0 loading (winmm.dll proxy, game base 00400000)
10:00:00.002 log: level 2 (verbose; [Log] Level 1 normal, 2 verbose, 3 trace); grep "ANOMALY:" for states a feature should never reach
10:00:00.500 session: SH4 Refined v3.35.0 (file version 3.35.0.0), winmm.dll md5 0123456789abcdef0123456789abcdef, 1216512 bytes, built Nov 20 2026 12:00:00; log level 2
10:00:01.000 budget: in (v3.35: one effect budget for Echo Doubles, After Shock and Death Presence; their draw work's target 6.0 ms of a 60 fps frame, 12.0 at 30 fps; a level down after 0.8 s of late frames (25 % over 1.25 x the frame) while they cost 50 % of it or more, back up after 3 s calm (to 30 s when a step up is taken back); 9 levels, each fades what it thins; [Graphics] EffectBudget=0 = v3.34's own guards)
10:00:05.000 [v] warm-up: stage 2 room 4: screen effects 12.3 ms (14 filter shaders, 16 textures); death presence 41.0 ms (fog, wisp, smoke, atlas and flame textures; 10 draw states); echo 97.1 ms (shaders, 3 textures, 5 body passes, 6 sprite batches); after shock 3.2 ms (her glow and heat passes: 2 drawn; 8 sprite / ribbon states; 2 new enemy type(s) for the shock pass: 0A 0C); 153.9 ms in all, under the room's fade-in
10:00:30.000 [v] budget: level 0 -> 5 (frames run over; 41 % of frames late, the effects 9.8 ms a frame against 6.0; stage 2 room 4): After Shock: fewer copies along the same path, 8 -> 5 and 5 -> 4
10:00:31.000 [v] budget: level 5 -> 6 (frames run over; 30 % of frames late, the effects 8.1 ms a frame against 6.0; stage 2 room 4): Echo: no weapon redraw, the ground discs fade
10:00:40.000 [v] budget: level 6 -> 5 (calm again; 0 % of frames late, the effects 3.0 ms a frame against 6.0; stage 2 room 4): Echo: no weapon redraw, the ground discs fade comes back
10:01:00.000 [v] room: 2:04 left after 60.0 s (play 58.0, cutscene 0.0, paused 2.0); 3480 frames: mean 16.9 ms, p99 24.0, worst 275.6 at +23.2 s (after a load; Refined 259.68 ms; after shock 259.2 ms, echo 0.4; budget level 0), spikes 2 (over 50 ms), stalls 0; VA 1057 of 2047 MB (margin 990)
10:01:00.001 [v] budget: room 2:04: level 5 now (After Shock: fewer copies along the same path, 8 -> 5 and 5 -> 4), at most 6 this room; 2 step(s) down, 1 up; 52 % of its play frames thinned; the effects 7.2 ms a frame on average, 21.4 at most (target 6.0); 6.3 % of the frames late
10:02:00.000 [v] room: 2:05 left after 60.0 s (play 60.0, cutscene 0.0, paused 0.0); 3600 frames: mean 16.7 ms, p99 18.0, worst 34.7 at +30.0 s (in play; Refined 34.71 ms; echo 21.3 ms, after shock 8.1, death presence 3.2; budget level 5), spikes 0 (over 50 ms), stalls 0; VA 1057 of 2047 MB (margin 990)
10:02:00.001 [v] budget: room 2:05: level 0 now (everything full), at most 0 this room; 0 step(s) down, 0 up; 0 % of its play frames thinned; the effects 2.1 ms a frame on average, 4.0 at most (target 6.0); 0.0 % of the frames late
"""


# v3.36 TP336: the task / IK pools' install lines and per-room lines (pools.inc)
V336 = """11:00:00.001 SH4 Refined v3.36.0 loading (winmm.dll proxy, game base 00400000)
11:00:00.100 task pool: 400 slots (was 300) at 011B6020..0121EB20, in place: the draw packet buffer behind it now starts at 0121EB20 with 87 KB (was 192 KB at 01204860; its use on PC is under 1 KB a frame). Slots 300 and up are taken only when the first 300 are all in use (a full pool crashes the game)
11:00:00.101 IK pool: 512 handles (was 160; 40 KB of the world arena) at the boot and every world reset (00414E30, 00414A2A), checked at every room change (004F0A5F); [Gameplay] IkPool=512
11:01:00.000 [v] pools: room 2:04 left: tasks 141 of 400 in use at most (fewest free 259; slots 300+ held 0 at most), IK handles 37 in use at most (fewest free 475 of 512), actor records fewest free 230 of 300, heap 9123 of 65536 units at most; this session at most tasks 141 (slots 300+ 0), IK 37
11:02:00.000 [v] pools: room 2:05 left: tasks 312 of 400 in use at most (fewest free 88; slots 300+ held 12 at most), IK handles 90 in use at most (fewest free 422 of 512), actor records fewest free 201 of 300, heap 9400 of 65536 units at most; this session at most tasks 312 (slots 300+ 12), IK 90
11:03:00.000 [v] pools: room 2:06 left: tasks 100 of 400 in use at most (fewest free 300; slots 300+ held 0 at most), IK list unreadable (3 samples), actor records fewest free 250 of 300, heap 9000 of 65536 units at most; this session at most tasks 312 (slots 300+ 12), IK 90
"""

# v3.36 HE336: the hit-shape pool's and the enemy records' room lines (hitpool.inc HpTick, recevict.inc EvTick)
V336H = V335 + """10:03:00.000 [v] hit shapes: room 2:5 left: lowest free 212 of 356 (144 in use at most), 0 refusal(s) here (0 this session), stomp guard 0 here; session lowest free 212; the game's 100 + this dll's block
10:03:00.001 [v] enemy records: room 2:5 left: 30 of 64 in use now (0 copies'), 64 at most at its exit's 44 write(s); 40 copies' record(s) purged; evictions: 20 copies' record(s), 1 real (1 of them for a copy's write); session 20 / 1 / 1, most in use 64 (copies first)
10:04:00.000 [v] hit shapes: room 2:4 left: lowest free 0 of 100 (100 in use at most), 7 refusal(s) here (7 this session), stomp guard 1 here; session lowest free 0; the stock pool
10:04:00.001 [v] enemy records: room 2:4 left: 12 of 64 in use now (0 copies'), 20 at most at its exit's 8 write(s); 0 copies' record(s) purged; evictions: 0 copies' record(s), 0 real (0 of them for a copy's write); session 20 / 1 / 1, most in use 64 (copies first)
"""

# v3.37 MN337: Supply Drops' lines against the caps 64 / 10 (the formats drops.inc writes)
V337 = """12:00:00.001 SH4 Refined v3.37.0 loading (winmm.dll proxy, game base 00400000)
12:00:00.100 supply drops: v3.37: ghosts drop (every full down: Normal 5 % / Generous 10 %, halved for the same ghost's 2nd down, a quarter from its 3rd, each world visit); anyone's kills roll; drops fade 120 s of gameplay after they land, in any room (no world budget) (lying caps 64 anywhere / 10 a room, 16 records kept free; a pickup or a fade frees its place at once)
12:00:00.101 supply drops: rolls seeded per session (a new seed at the game start, every New Game, load and quick load)
12:00:00.102 supply drops: in (calls 00463150 / 00459490 from the tick; ledger 00FD0B98, pickup records 10012345); level Generous
12:01:00.000 supply drops: Nutrition Drink from type 07/0 (weak, gauge 2.5, char 03020001, life 0) in stage 3 room 2 (record 40; world visit 1)
12:01:01.000 supply drops: Holy Candle from a copy of type 07/0 (strong, copy 1E000001, life 0) in stage 3 room 2 (record 41; copies this world visit: 1 / 30; 1 copy drops this session)
12:01:02.000 supply drops: Ampoule from Cynthia (stage 3, visit 0, top-level down 2) at 10 20 30 (record 42)
12:02:00.000 supply drops: no drop: Nutrition Drink won but the room's cap of drops lying here (room 3:2: 10 of ours lie here, cap 10; 40 lying, cap 64; pickup records free 70 of 128, a new drop needs 17)
12:02:30.000 supply drops: the cap of 64 lying drops: Nutrition Drink in stage 3 room 5 is taken away
12:03:00.000 supply drops: no drop: Ampoule won but the cap of drops lying (none may make way) (room 3:9: 0 of ours lie here, cap 10; 64 lying, cap 64; pickup records free 48 of 128, a new drop needs 17)
12:04:00.000 [v] supply drops: Nutrition Drink faded after 120 s (room 3:2, the room you are in; 1 faded this session)
12:05:00.000 [v] supply drops: drop numbers renumbered 1..64 (the highest was 3840 of 4095; their order is kept)
"""


# v3.38 DR338: Supply Drops' four levels and equal shares (the formats drops.inc / gameplay.inc write)
V338 = """12:00:00.001 SH4 Refined v3.38.0 loading (winmm.dll proxy, game base 00400000)
12:00:00.050 gameplay: SupplyDrops Generous (v3.29 - v3.37) reads as Abundant (Supply Drops: Off, Rare, Normal or Abundant; the same chances)
12:00:00.051 gameplay: MoveSpeed Normal, MedallionDurability Normal, OneHitKill Off, DeathPresence Off, AfterImage Off, Echo Off x1, SupplyDrops Abundant; Playable Eileen loaded (its regeneration x1)
12:00:00.100 supply drops: v3.38: ghosts drop (every full down: Rare 2.5 % / Normal 5 % / Abundant 10 %, halved for the same ghost's 2nd down, a quarter from its 3rd, each world visit); anyone's kills roll; drops fade 120 s of gameplay after they land, in any room (no world budget) (lying caps 64 anywhere / 10 a room, 16 records kept free; a pickup or a fade frees its place at once; the same for every level)
12:00:00.101 supply drops: levels Off / Rare / Normal / Abundant (Normal's chances x0.5 / x1 / x2): one roll a kill or full down, common / uncommon / rare on Normal: weak 11.45 / 0.5 / 0.05 %, strong 9.9 / 2 / 0.1 %, ghost 7.75 / 4 / 0.25 %, named ghost 5 / 4 / 3 %, Walter 7 / 4 / 1 % (any item 12 % for every class; every ghost down rolls the full chance)
12:00:00.102 supply drops: shares in each group: common Nutrition Drink 1 / Pistol Bullets 1 / Revolver Bullets 1, uncommon Portable Medical Kit 2 / Holy Candle 1, rare Saint Medallion 1 / Ampoule 1 / Silver Bullets 1 / Sword of Obedience 1; Pistol and Silver Bullets need the pistol, Revolver Bullets the revolver: a gun not owned gives its share to the others of its group
12:00:00.103 supply drops: rolls seeded per session (a new seed at the game start, every New Game, load and quick load)
12:00:00.104 supply drops: in (calls 00463150 / 00459490 from the tick; ledger 00FD0B98, pickup records 10012345); level Abundant
12:01:00.000 supply drops: Pistol Bullets from type 0A/0 (weak, gauge 2.2, char 02040001, life 0) in stage 2 room 4 (record 33; world visit 0: 1 / 15)
12:01:10.000 supply drops: Revolver Bullets from type 03/0 (weak, gauge 1.5, char 02040002, life 0) in stage 2 room 4 (record 34; world visit 0: 2 / 15)
12:01:20.000 supply drops: Silver Bullets from type 07/0 (strong, gauge 4.0, char 02040003, life 0) in stage 2 room 4 (record 35; world visit 0: 3 / 15)
12:01:30.000 supply drops: Revolver Bullets from ghost 0B/3 (char FFFF0007, its down 1 this world visit, common of the ghost chance 0.2400) in stage 2 room 5 at 1 2 3 (record 36; it fades in 120 s)
12:02:00.000 gameplay: SupplyDrops Rare
12:02:30.000 supply drops: Sword of Obedience from Jasper (stage 3, visit 0, top-level down 0; rare, the named ghost class) at 1 2 3 (record 37)
12:03:00.000 gameplay: [Gameplay] SupplyDrops read again: Normal
12:03:10.000 supply drops: Nutrition Drink from Cynthia (stage 3, visit 0, top-level down 1; common, the named ghost class) at 1 2 3 (record 38)
"""


# v3.37 CP337: the performance overlay's summary lines, the Direct3D chain, the pause Camera page (perfhud.inc, extramenu.inc)
V337P = V336H + """10:05:00.000 perf: Direct3D: GOG -> dgVoodoo2 D3D12 (Option 4's first row description; d3d9.dll from the game folder, dgVoodoo.conf present, OutputAPI "d3d12_fl12_0", d3d12.dll loaded, d3d11.dll not loaded)
10:06:00.000 [v] extra menu: Camera page opened (camera OTS; the view changes when play resumes)
10:06:10.000 [v] extra menu: Camera page closed: camera OTS, distance 600 side 25 height 330 tilt 17, turn 0.1 tilt 0.1, invert 0 0, wall zoom 1 margin 50, fixed cameras off
10:10:00.000 [v] perf: the last 5 minutes: 300 s, 18000 frames, 60.0 fps on average, the lowest 1% low 52.3 fps (now 58.1, p99 17.2 ms), worst frame 41.0 ms, Refined 1.20 ms a frame (17000 ledger frames), address space 812 of 2048 MB (most 840)
10:12:00.000 [v] perf: the overlay went off: 120 s, 3600 frames, 30.0 fps on average, the lowest 1% low 29.0 fps (now 29.5, p99 34.4 ms), worst frame 60.5 ms, Refined 2.05 ms a frame (3500 ledger frames), address space 900 of 2048 MB (most 910)
"""


# v3.41 CM341: who draws the player's curse ring (hudbars.inc HbPeOwnCheck)
V341C = V333_END + """10:00:01.000 hud bars: Playable Eileen draws the player's curse ring (SH4PE_DrawsPlayerCurse): only companion Eileen's ring here (CurseMeter Both; Player is grey in Options, the ini keeps the value)
10:00:02.000 [v] hud bars: CurseMeter Both with Playable Eileen drawing the player's ring (its ring can show): companion Eileen's small ring beside PE's ring
"""
V341R = V333_END + """10:00:01.000 hud bars: Playable Eileen's curse meter found (SH4PE_Curse, version 1)
10:00:02.000 hud bars: SH4PE_DrawsPlayerCurse is not exported (Playable Eileen up to v1.23.0): Refined draws the player's curse ring as before
"""
# v3.42 SM342: Curse Meter Off / On, Death Presence off in Room 302, Echo Doubles / After Shock hidden in views, the
# shadows' Darkness
V342S = V333_END + """10:00:00.500 hud bars: [Gameplay] CurseMeter=Both (v3.41 or older) reads as On: companion Eileen's ring (Both's player ring is Playable Eileen's own)
10:00:00.600 hud bars: Curse Meter Off / On (v3.42): On = companion Eileen's ring only; Refined never draws the player's curse ring (Playable Eileen v1.23.1+ draws it; with an older one or none, nobody does)
10:00:00.700 death presence: Room 302 (stages 1 and 0A): off (no fog there, the hauntings as stock; hidden [ScreenEffects] DeathPresenceRoom302=1 brings it back)
10:00:00.800 shadows: Darkness 85 % (of the game's inside darkness, after Playable Eileen's ShadowDarkness; Stock and Soft)
10:00:00.900 shadows: hooks in place (the composite's draw 0055FE43, the device reset 00402E0D); Edge Soft; Soft 12.0 px at 720 lines, scaled with the height (down, blur and up in 2x steps); preview on (the page backdrop 0x526A53)
10:00:01.000 hud bars: Playable Eileen draws the player's curse ring (SH4PE_DrawsPlayerCurse): only companion Eileen's ring here (CurseMeter On; her small ring beside PE's while PE's can show)
10:00:02.000 [v] death presence: Room 302 (stage 1 room 2): off here (no fog; the hauntings run as stock; [ScreenEffects] DeathPresenceRoom302=1 brings it back)
10:00:03.000 [v] echo: hidden in a view: the room is not drawn (a view, a load or a menu) (stage 2 room 5)
10:00:03.000 [v] after shock: hidden in a view: the room is not drawn (a view, a load or a menu) (stage 2 room 5)
10:00:04.500 [v] echo: shown again after 1.52 s hidden (the room is not drawn (a view, a load or a menu))
10:00:04.500 [v] after shock: shown again after 1.50 s hidden (the room is not drawn (a view, a load or a menu))
10:00:05.000 [v] echo: hidden in a view: the Room 302 peeping flag (stage 1 room 1)
10:00:06.000 [v] shadows: Darkness 85 % on Soft: darkness 4B000000, the composite's K 0.502 x 0.85: inside 0.426 of the game's 0.502
10:00:07.000 [v] shadows: Darkness 70 % (of the game's inside darkness, after Playable Eileen's ShadowDarkness; Stock and Soft)
10:00:08.000 [v] shadows: Darkness 70 % on Stock: darkness 32000000, the two copies' alpha x 172 / 255 (TFACTOR): inside 0.247 of the game's 0.354 (asked 0.248)
"""
V342R = V333_END + """10:00:00.600 hud bars: Curse Meter Off / On (v3.42): On = companion Eileen's ring only; Refined never draws the player's curse ring (Playable Eileen v1.23.1+ draws it; with an older one or none, nobody does)
10:00:00.700 death presence: Room 302 (stages 1 and 0A): on ([ScreenEffects] DeathPresenceRoom302=1: the fog clears the hauntings as in v3.41)
10:00:02.000 hud bars: SH4PE_DrawsPlayerCurse is not exported (Playable Eileen up to v1.23.0): no player curse ring (Refined no longer draws it; Playable Eileen v1.23.1+ does); companion Eileen's ring at the main place
"""
# v3.43 SH343: the c1.a the soft composite drew with (read back from the device): the page and play, one not read back,
# one mismatch (an anomaly)
V343S = V342S + """10:00:09.000 [v] shadows: the Shadows page's right half drew darkness 4B000000 at Darkness 55 %: c1.a 0.276 read back (asked 0.276): the inside lets 72 % of the light through (left of the line, the game's own: 50 %)
10:00:10.000 [v] shadows: in play the composite drew darkness 4B000000 at Darkness 55 %: c1.a 0.276 read back from the device before the draw (asked 0.276; the Shadows page's right half uses the same): the inside lets 72 % of the light through (the game's own 50 %)
10:00:11.000 [v] shadows: in play the composite drew darkness 44070707 at Darkness 55 % with c1.a 0.254 (not read back: the query failed): the inside lets 75 % of the light through (the game's own 54 %)
10:00:12.000 [v] shadows: the Shadows page's right half drew darkness 32000000 at Darkness 70 %: c1.a 0.248 read back (asked 0.248): the inside lets 75 % of the light through (left of the line, the game's own: 65 %)
10:00:13.000 [v] shadows: in play the composite drew darkness 32000000 at Darkness 70 %: c1.a 0.354 read back from the device before the draw (asked 0.354; the Shadows page's right half uses the same): the inside lets 65 % of the light through (the game's own 65 %)
10:00:14.000 ANOMALY: shadows: the in-play composite: c1.a read back 0.3540, asked 0.2480 (darkness 32000000, Darkness 70 %): the draw does not use the Darkness
"""
# v3.43 PP343: Bloom, Colour Grading, Otherworld Lens (post.inc)
V343P = V333_END + """10:00:00.500 post effects: in place (drawn in the scene pass before the filter; device reset 00402DF4 hooked); Bloom Low, Colour Grading Off, Otherworld Lens On
10:00:01.000 screen effects: Grading Apartment
10:00:01.500 [v] post effects: bloom chain at 2560x1440: 1280x720 down to 160x90 (4 levels), 2 blur passes, up in 2x steps: 10 draws, 4.7 MB of render targets
10:00:01.600 [v] post effects: bloom Low drawn over the Screen Effects page preview (before the filter None)
10:00:02.000 screen effects: Filter Thermal
10:00:02.100 [v] post effects: colour grading Apartment not drawn under the filter Thermal (it sets the colours itself)
10:00:03.000 [v] post effects: the Otherworld lens fades in (stage 3 room 5; haunting strength 1.20, nearness 0.43: 100 %)
10:00:09.000 [v] post effects: the Otherworld lens fades out (stage 3 room 5; haunting strength 0.00, nearness 0.00: 0 %)
10:00:10.000 [v] post effects: the Otherworld lens fades in (stage 1 room 2; haunting strength 0.80, nearness 0.00: 40 %)
10:00:11.000 post effects: bloom off for this session: its render targets could not be made 3 times in a row (hr 8007000E)
"""
# v3.44 CS344: Bloom Very High, the pixel checks (grade and shadow composite), the shader lint line
V344C = V333_END + """10:00:00.400 [v] post effects: every screen shader in use uses proven ps.1.1 forms only (PsLint; Noir unused)
10:00:00.500 post effects: in place (drawn in the scene pass before the filter; device reset 00402DF4 hooked); Bloom Very High, Colour Grading PS2, Otherworld Lens Off
10:00:01.000 screen effects: Bloom Very High
10:00:01.100 [v] post effects: bloom Very High drawn over the Screen Effects page preview (before the filter None)
10:00:01.200 [v] post effects: colour grading PS2: pixel check ok: a fixed test draw through the grading shader, read back: 1 level(s) off its sim at most, 7.4 levels changed on average (the sim 7.5): the grade reaches the pixels
10:00:01.200 [v] post effects: colour grading PS2: the test colours 38,38,38 -> 43,50,42 (sim 43,50,42); 128,128,128 -> 124,137,121 (sim 124,137,121)
10:00:02.000 [v] post effects: colour grading Bleach: pixel check FAILED: a fixed test draw through the grading shader, read back: 22 level(s) off its sim at most, 0.0 levels changed on average (the sim 14.0): the input came through unchanged: the grading shader did not reach the pixels
10:00:03.000 [v] post effects: colour grading Otherworld: pixel check not run (a test target could not be made)
10:00:04.000 [v] shadows: pixel check (the Shadows page, darkness 4B000000 at Darkness 55 %) ok: a fixed test draw through the composite, read back: full cover alpha 0.278, asked 0.276 (lets 72 % of the light through); 3/4 cover 0.260 (asked 0.259), half 0.208 (asked 0.208), none 0.000: the Darkness reaches the pixels
10:00:05.000 [v] shadows: pixel check (in play, darkness 4B000000 at Darkness 25 %) FAILED: a fixed test draw through the composite, read back: full cover alpha 1.000, asked 0.125 (lets 0 % of the light through); 3/4 cover 0.753 (asked 0.117), half 0.502 (asked 0.094), none 0.000: the coverage itself came through: the composite shader did not run (no Darkness)
10:00:05.000 ANOMALY: shadows: pixel check (in play, Darkness 25 %): the coverage itself came through: the composite shader did not run (no Darkness) (full cover alpha 1.000, asked 0.125)
10:00:06.000 [v] shadows: pixel check (in play, darkness 32000000 at Darkness 85 %) not run (the read back (CopyRects) failed)
"""
# v3.47 PX347: the pixel checks' new lines (the source as read back, the measured offset), as post.inc / shadows.inc write them
V347PX = V333_END + """10:00:01.000 read back: GPU -> CPU through a lockable render target on this backend (CopyRects, then LockRect; first used by a pixel check, 32x4): the pixel checks and the depth probe read back this way
10:00:01.000 [v] shadows: pixel check (in play, darkness 4B000000 at Darkness 70 %) ok: a fixed test draw through the composite, read back: full cover alpha 0.353, asked 0.351 (lets 65 % of the light through); 3/4 cover 0.330 (asked 0.330), half 0.263 (asked 0.264), none 0.000: the Darkness reaches the pixels
10:00:01.000 [v] shadows: pixel check (in play, Darkness 70 %): K 0.351, coverage asked 1.000 / 0.753 / 0.502 / 0.000, read at the cells' centres (green 165 / 171 / 188 / 255); the source read back as asked
10:00:01.000 [v] pixel check alignment (shadows in play): the test draw samples 0.50 texel(s) left of the texel centres (from 3 cell edge(s); each cell's first / centre / last column, green: 210 / 165 / 165 (top 165); 168 / 171 / 171 (top 171); 179 / 188 / 188 (top 188); 222 / 255 / 255 (top 255)): the backend's half-pixel rule (KB 194); the cells are read at their centres, which it does not reach
10:00:02.000 [v] post effects: colour grading PS2: pixel check ok: a fixed test draw through the grading shader, read back: 1 level(s) off its sim at most, 9.4 levels changed on average (the sim 9.3): the grade reaches the pixels
10:00:02.000 [v] post effects: colour grading PS2: the test colours (asked -> the source -> the cell's centre read back (sim)) 38,38,38 -> 38,38,38 -> 43,50,42 (sim 43,50,42); 128,128,128 -> 128,128,128 -> 124,137,121 (sim 124,137,121); 217,217,217 -> 217,217,217 -> 205,223,198 (sim 205,223,198); 153,77,51 -> 153,77,51 -> 140,90,59 (sim 140,90,59); the source read back as asked
10:00:02.000 [v] pixel check alignment (colour grading PS2): the test draw samples 0.50 texel(s) left of the texel centres (from 6 cell edge(s); each cell's first / centre / last column, green: 25 / 50 / 50 (top 50); 93 / 137 / 137 (top 137); 180 / 223 / 223 (top 223); 156 / 90 / 90 (top 90)): the backend's half-pixel rule (KB 194); the cells are read at their centres, which it does not reach
10:00:03.000 [v] shadows: pixel check (in play, darkness 32000000 at Darkness 70 %) ok: a fixed test draw through the composite, read back: full cover alpha 0.249, asked 0.248 (lets 75 % of the light through); 3/4 cover 0.232 (asked 0.232), half 0.000 (asked 0.000), none 0.000: the Darkness reaches the pixels
10:00:03.000 [v] shadows: pixel check (in play, Darkness 70 %): K 0.248, coverage asked 1.000 / 0.753 / 0.502 / 0.000, read at the cells' centres (green 191 / 196 / 255 / 255); the source held other values in 1 cell(s) (a Clear that did not land): 00000000 (asked 80000000); the "asked" values are the sim on the source as read back
10:00:03.000 [v] pixel check alignment (shadows in play): not measurable (no two neighbouring cells 16 levels apart; 191 / 191 / 191 (top 191); 196 / 196 / 196 (top 196); 255 / 255 / 255 (top 255); 255 / 255 / 255 (top 255))
"""
# v3.45 RB345: the read back: the path that worked (dgVoodoo2 D3D11), a switch; then a log with none on its backend
V345R = V333_END + """10:00:01.000 read back: GPU -> CPU through a lockable render target on this backend (CopyRects, then LockRect; first used by a pixel check, 8x2): the pixel checks and the depth probe read back this way
10:00:01.000 [v] shadows: pixel check (the Shadows page, darkness 4B000000 at Darkness 55 %) ok: a fixed test draw through the composite, read back: full cover alpha 0.278, asked 0.276 (lets 72 % of the light through); 3/4 cover 0.260 (asked 0.259), half 0.208 (asked 0.208), none 0.000: the composite reaches the pixels
10:00:02.000 [v] depth of field: stage 2 room 4: depth probe (2560x1440, 576 samples, 11.20 ms, read back ok via the lockable render target): codes 10 40 90 120 140 100 50 20 6 (the largest 24 %); her figure 9 sample(s), nearest code 2 (her depth 627 = code 2): the scene's depth: on
10:00:03.000 [v] read back: a lockable render target failed this time (the depth probe, 2560x1440): the copy (CopyRects) failed (hr 8876086C); it worked 2 time(s) before: tried again next time
10:00:03.000 [v] read back: now through a system-memory image surface (the depth probe, 2560x1440; a lockable render target: the copy (CopyRects) failed (hr 8876086C, 1 time(s)))
"""
V345N = V333_END + """10:00:00.100 depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to 10012340); DepthOfField Always, Strength Low; probe on (once per room); preview on (the Screen Effects page, 0x526A53)
10:00:02.000 [v] depth of field: stage 2 room 4: depth probe (2560x1440, 0 samples, 0.50 ms, no read back): codes 0 0 0 0 0 0 0 0 0 (the largest 0 %); her figure 0 sample(s), nearest code -1 (her depth 627 = code 2): not probed: no read back: on
10:00:03.000 read back: no GPU -> CPU read back on this backend (a lockable render target: the copy (CopyRects) failed (hr 8876086C, 2 time(s)); a system-memory image surface: the copy (CopyRects) failed (hr 8876086C, 2 time(s)); last by a pixel check, 8x2): the pixel checks are off and the depth probe does not run for this session (the effects draw as before; depth of field unprobed)
10:01:00.000 [v] depth of field: stage 2 room 4: drawn in 3000 of 3010 frames it was asked for (not drawn: the room's depth probe is pending 10; the cut-scene's own focus 0); skipped by design: none; focus 600 .. 700; ours 0.050 ms a frame on average, 0.100 at most; Always Low; probe: not probed: no read back: on; render targets 10.6 MB
10:02:00.000 [v] depth of field: stage 2 room 5: drawn in 2000 of 2000 frames it was asked for (not drawn: none; the cut-scene's own focus 0); skipped by design: none; focus 600 .. 900; ours 0.050 ms a frame on average, 0.100 at most; Always Low; probe: not probed: no read back: on; render targets 10.6 MB
"""
V343O = V333_END + """10:00:00.500 post effects: off ([ScreenEffects] PostEffects=0): no Bloom, Colour Grading or Otherworld Lens rows
"""

# v3.38 HB338: the curse meter under Hide Gauge, the Performance readout's place (hudbars.inc, perfhud.inc)
V338H = V333_END + """10:00:01.000 hud bars: Hide Gauge on: the curse meter fades with the health gauge (5 s without a change of its own, then 1 s; up while Possessed)
10:00:02.000 [v] perf: readout under Playable Eileen's HUD ([HUD] X 24, Y 24, Scale 1.00) (2048x1536, S 1.000; the HUD box 75..401 x 75..401 px): its box from 9, 429 px
10:00:03.000 [v] perf: readout at the top-left corner, clear of the stock health gauge (1920x1080, S 1.333; the HUD box 299..867 x 51..171 px): its box from 6, 6 px
10:00:10.000 [v] hud bars: Hide Gauge: the curse meter faded out: 5 s without a change, then 1 s (gauge fade 0.00)
10:00:20.000 [v] hud bars: Hide Gauge: companion Eileen's small ring shown: woken by the left-alone glyph (gauge fade 0.00)
10:00:30.000 [v] hud bars: Hide Gauge: the curse meter shown: with the health gauge (gauge fade 0.70)
10:00:40.000 [v] hud bars: Hide Gauge: the curse meter faded out: hidden with the HUD (pause, event) (gauge fade 0.00)
"""


V337I = V333_END + """10:00:00.100 inventory: Extended Toolbelt on ([Gameplay] ExtendedToolbelt On; 33 sites from 004F4A09 to 0053DED0), Stackable Items off (StackableItems Off; 10 sites), stack guards in (00463607 thief cap, 004F6A30 stacked break, 004F5DB2 use one: always)
10:05:00.000 [v] extended toolbelt: stays on: page 2 holds 7 item(s) for the Storage Chest and it has room for 3 (126 places): store or use some, then turn it off
10:06:00.000 [v] stackable items: on (10 exe sites; up to 99 a stack); 2 belt slot(s) show their count now
10:07:00.000 [v] inventory: toolbelt flips 12, overflows to page 2 3, page-2 story removals 1, re-equips from page 2 2, rounds fired from page 2 40, cross-page reloads 4, broken page-2 medallions 0; stacks: merges 9, chest take-outs 2, stacked breaks 1, broken ones dropped (no room) 0, full-belt pickups 1
10:08:00.000 [v] extended toolbelt: off: page 2's 3 item(s) moved: 1 to page 1 (the weapon in hand / the medallion in use), 2 into the Storage Chest (now 40 entries)
"""

# v3.39 PR339: the Renderer row and the Opening skip (presentation.inc, renderer.inc); the perf summary with the readout off
V339R = V333_END + """10:00:00.050 renderer: dgVoodoo.conf is Direct3D 11 (16-bit) (d3d11fd_rt16, C7E9253A95A07A1466306F417978204A); row open (offered); manifest read (4 offered, 4 recognised only); d3d9.dll CAA75C55D9B3121D17433BD3B38C7BB2 (dgVoodoo2 2.87.6)
10:00:00.060 opening: Skip Logos ([Window] Opening): 0042CD6A -> the skip (no KONAMI / KCET logos, no notice)
10:00:00.900 opening: the boot screens skipped (KONAMI, KCET, the notice; Skip Logos), 900 ms after the start: gamma 0 applied, the fade black, on to the title
10:00:01.000 perf: Direct3D: GOG -> dgVoodoo2, Direct3D 11 (16-bit) (Option 4's first row description; d3d9.dll from the game folder, dgVoodoo.conf present, OutputAPI "d3d11_fl11_0", d3d12.dll not loaded, d3d11.dll loaded)
10:01:00.000 renderer: Direct3D 12 (d3d12): not replaced: dgVoodoo.conf could not be replaced (error 5); dgVoodoo.conf left as it was
10:02:00.000 renderer: Direct3D 11 (16-bit) (d3d11fd_rt16, C7E9253A95A07A1466306F417978204A) -> Direct3D 12 (d3d12, 67FDE0CF1FA6E95EFB53F0F398540F4E): dgVoodoo.conf replaced (dgVoodoo.conf.new written and checked first); it applies at the next start (dgVoodoo reads dgVoodoo.conf when the game starts; running now: Direct3D 11 (16-bit))
10:02:00.001 opening: Fast saved ([Window] Opening); it applies at the next start (this start: Skip Logos)
10:05:00.000 [v] perf: the last 5 minutes: 300 s, 18000 frames, 60.0 fps on average, the lowest 1% low 52.3 fps (now 58.1, p99 17.2 ms), worst frame 41.0 ms, Refined 1.20 ms a frame (17000 ledger frames), address space 812 of 2048 MB (most 840)
"""

# v3.41 DP341: Death Presence's draw gate (deathpresence_draw.inc DpGateFrame); its room line still names the room
V341D = V333_END + """10:00:00.200 [v] death presence: stage 1 room 1: shown again after 0.32 s hidden (the start; still for 19 frames)
10:00:30.000 [v] death presence: stage 1 room 1: hidden: the room is not drawn (a view, a load or a menu)
10:00:52.000 [v] death presence: stage 1 room 1: still hidden after 20 s: the room is not drawn (a view, a load or a menu) (her model drawn no)
10:01:40.000 [v] death presence: stage 1 room 1: shown again after 70.25 s hidden (the room is not drawn (a view, a load or a menu); still for 18 frames)
10:02:00.000 [v] death presence: stage 1 room 1: hidden: the Room 302 peeping flag
10:02:05.000 [v] death presence: stage 1 room 1: shown again after 5.10 s hidden (the Room 302 peeping flag; still for 18 frames)
10:03:00.000 [v] death presence: stage 2 room 1: hidden: a room change
10:03:00.600 [v] death presence: stage 2 room 1: shown again after 0.61 s hidden (a jump of her position; still for 18 frames)
10:03:04.000 death presence: stage 2 room 1 (scene pass, look full): 4714 triangles, 447 sprites a frame (0 over the list, 0 ribbons over theirs), her model drawn yes, 0 being(s) shown, walls min 205 mean 3960 (R 400), viewport 3D, eye ok, resource misses 0, the slowest frame 2.11 ms; depth: exact, projection camera (m22 -0.00125156 m32 50.0626; per-model -0.00125156 50.0626), near 50 far 40000, camera 654 from her
10:04:00.000 [v] death presence: stage 2 room 1: a move of the player stayed pending for 2 s: no longer waited for
"""

# v3.38 EN338: Enemies Follow's lines (enemies.inc)
V338F = V333_END + """10:00:00.100 enemies: Wheelchair 85060002 (home room 12) was recorded in room 7 (state 01; it followed in v3.37): it goes home (Wheelchairs stay vanilla)
10:00:01.000 [v] enemies: monster 85040003 (type 03) spotted you in room 4 (came near you; 650 units away): it follows you through your next door, however far away it is then
10:00:02.000 [v] enemies: copy 1E001005 (type 0A) spotted you in room 4 (saw you; 2100 units away): it follows you through your next door, however far away it is then
10:00:03.000 enemies: through door 7 from room 4 to 5: 3 follow (2 monster(s), 1 of them copies, 1 ghost(s); 3 from that room, 0 pursuing from other rooms or on their way): 85040003 monster 03, 1E001005 copy 0A, FFFF0007 ghost 0B; 1 wait for a later door (0 FollowMax 32 full, 0 files, 1 story rule, 0 down in their room), 0 already here, 0 stopped, 2 in that room had not spotted you
10:00:03.001 [v] enemies: monster 8F030002 (type 11) does not come from room 3 into room 4: room 5:3 before story flag 3BF (30 s there set it: Richard's scripted exit) (it keeps pursuing you)
10:00:06.000 enemies: follower 85040003 (type 03) came through the door into room 5 after 3.0 s
10:00:07.000 [v] enemies: copy 1E001005 (type 0A) waits in front of room 5: hit shapes (24 kept free) (it comes once there is space, or through a later door)
10:00:08.000 [v] enemies: ghost FFFF0007 waits outside room 5: the room's enemy list is full (40) (it comes once there is space)
10:00:09.000 enemies: copy 1E001005 (type 0A) came through the door into room 5 after 7.5 s
10:00:20.000 enemies: through door 3 from room 5 to 6: 5 follow (4 monster(s), 2 of them copies, 1 ghost(s); 2 from that room, 3 pursuing from other rooms or on their way): a, b; 0 wait for a later door (0 FollowMax 32 full, 0 files, 0 story rule, 0 down in their room), 1 already here, 1 stopped, 0 in that room had not spotted you
10:00:21.000 [v] enemies: monster 85040003 (type 03 v0) stops pursuing you in room 6: killed
"""

# v3.41 SH341: the one Soft edge, the kept pause targets, the GPU pause picture, a read back (PreviewKey)
V341S = V333_END + """10:00:00.050 shadows: [Shadows] Edge=Softer (v3.40) reads as Soft: v3.41 has one soft edge, twice as wide as v3.40's Softer
10:00:00.100 shadows: hooks in place (the composite's draw 0055FE43, the device reset 00402E0D); Edge Soft; Soft 12.0 px at 720 lines, scaled with the height (down, blur and up in 2x steps); preview on (the page backdrop 0x526A53)
10:00:05.000 [v] shadows: Soft at 2560x1440: down to 320x180 (3 level(s)), 1 blur pass(es) (offsets 0.96), up in 2x steps: a 24.0 px edge (asked 24.0); 7 draws, at most a 2x stretch; 4.8 MB of render targets
10:00:05.001 [v] shadows: render target made: half size 1280x720 (3.5 MB; ours now 3.5 MB)
10:01:00.000 [v] shadows: render target made: the paused scene 2560x1440 (14.1 MB; ours now 18.9 MB)
10:01:00.001 [v] shadows: render target made: the paused mask 2560x1440 (14.1 MB; ours now 33.0 MB)
10:01:00.002 [v] shadows: the paused scene kept for the Shadows page: 2560x1440 picture + mask (28.1 MB; targets made 0.21 ms, copied 0.10 ms), darkness 4B000000 (frame 7000)
10:01:00.010 [v] screen effects: the pause picture kept on the GPU for the Screen Effects page: 2560x1440 (14.1 MB; target made 0.30 ms, copied 0.12 ms; nothing read back)
10:01:02.000 [v] shadows: the Shadows page's preview: the paused scene, split Stock | the chosen edge (the paused scene)
10:01:09.000 [v] shadows: the pause ended: its full-size targets (42.2 MB: the preview's copies and the pause picture) are kept for the next pause (the address space has 1300 MB left); ours now 47.0 MB
10:01:30.000 [v] shadows: the paused scene kept for the Shadows page: 2560x1440 picture + mask (28.1 MB; targets kept from an earlier pause: 0.00 ms, copied 0.09 ms), darkness 4B000000 (frame 8000)
10:01:30.010 [v] screen effects: the pause picture kept on the GPU for the Screen Effects page: 2560x1440 (14.1 MB; target kept from an earlier pause: 0.00 ms, copied 0.11 ms; nothing read back)
10:01:40.000 [v] screen effects: a frame read back from the GPU (2560x1440, route 2): copy 0.40 ms, read back 29.10 ms, shrink 2.00 ms
10:02:00.000 [v] shadows: room 1:02: 1200 composites over 1200 frames (at most 1 in a frame), 1200 soft; ours 0.050 ms a composite on average, 0.300 at most; edge Soft; render targets 47.0 MB
"""
# v3.41 EN341: ghost copies made in another room without their record (enemies.inc EnmFollowArrivals / EnmReplayCopyWanderers)
V341G = V333_END + """10:00:03.000 enemies: through door 37 from room 7 to 4: 2 follow (0 monster(s), 2 of them copies, 2 ghost(s); 2 from that room, 0 pursuing from other rooms or on their way): 1E001C09 ghost copy 0B, 1E001C05 ghost copy 0B; 0 wait for a later door (0 FollowMax 32 full, 0 files, 0 story rule, 0 down in their room), 0 already here, 0 stopped, 0 in that room had not spotted you
10:00:06.000 enemies: copy 1E001C09 (type 0B) came through the door into room 4 after 3.0 s
10:00:06.000 [v] enemies: ghost copy 1E001C09 (type 0B v3) came into room 4 as the game's own ghosts arrive: made without its record, standing at full HP (the room rule keeps ghosts out of room 4, and the game does not create a ghost whose record is there)
10:00:30.000 [v] enemies: ghost copy 1E001C05 (type 0B v0) is back in room 4 as the game's own ghosts arrive: made without its record, standing at full HP (the room rule keeps ghosts out of room 4, and the game does not create a ghost whose record is there)
"""

# v3.41 VW341: the window views' black sides and the Borderless edge-cover lines
V341W = V333_END + """10:00:00.100 window views: Room 302's window views keep the 4:3 view with black sides in wide shapes (0054CC10 hooked)
10:01:00.000 [v] window views: Room 302's window view: black sides 321 | 320 px beside the 4:3 view (2560x1440)
10:02:00.000 [v] window views: Room 302's window view: black sides 321 | 320 px beside the 4:3 view (2560x1440)
10:03:00.000 [v] window: borderless edges: 1 window(s) above the game cover its edges: right edge 2x1374 at 2558,15 (class Chrome_WidgetWin_1, its invisible border); the game is not in front (another program is active)
10:04:00.000 [v] window: borderless edges: nothing covers them now
10:05:00.000 [v] window: borderless edges: 1 window(s) above the game cover its edges: bottom edge 2560x2 at 0,1438 (class Shell_SecondaryTrayWnd, its frame); the game is in front
"""

# v3.42 EG342: the 2D edge decision lines (sh4windowmod.c SetConstants2D)
V342E = V333_END + """10:00:00.200 [v] 2D edges: 1024x768: the 2D canvases sit on pixel edges (half a pixel left / up: clip -0.000977, +0.001302), so full-screen pictures, fills and the grid cover column 0 / row 0 with half a pixel to spare; no black sides (4:3 or narrower)
10:00:00.300 [v] 2D edges: 2560x1440: the 2D canvases sit on pixel edges (half a pixel left / up: clip -0.000391, +0.000694), so full-screen pictures, fills and the grid cover column 0 / row 0 with half a pixel to spare
10:00:00.300 [v] 2D edges: 2560x1440: black sides 321 | 321 px (pixel columns 0..320 and 2239..2559), drawn as quads 1 px past the screen edges after the Clear
"""

# v3.43 DF343: Depth of Field's lines (dof.inc)
V343D = V333_END + """10:00:00.100 depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to 10012340); DepthOfField Always, Strength Low; probe on (once per room); preview on (the page backdrop 0x526A53, chained)
10:00:05.000 depth of field: first frame drawn (Always, Low): focus 1450 (the player), 3 slice(s), 3 blurred band(s), 0.41 ms; render targets 10.6 MB
10:00:05.100 [v] depth of field: stage 2 room 1: depth probe (2560x1440, 576 samples, 3.10 ms, read back ok): codes 10 40 90 120 140 100 50 20 6 (the largest 24 %); her figure 9 sample(s), nearest code 3 (her depth 1450 = code 3): the scene's depth: on
10:01:00.000 [v] depth of field: stage 2 room 1: drawn in 3500 of 3520 frames it was asked for (no focus 10, the probe 0; the cut-scene's own focus 0); focus 900 .. 2100; ours 0.420 ms a frame on average, 1.250 at most; Always Low; probe: the scene's depth: on; render targets 10.6 MB
10:01:00.100 [v] depth of field: stage 2 room 2: depth probe (2560x1440, 576 samples, 2.90 ms, read back ok): codes 0 0 0 0 0 0 0 570 6 (the largest 99 %); her figure 0 sample(s), nearest code -1 (her depth 1450 = code 3): flat (one depth almost everywhere: cleared or overwritten): off in this room
10:01:30.000 [v] depth of field: DepthOfField Cut-scenes (Strength Low)
10:01:31.000 [v] depth of field: Strength High (DepthOfField Cut-scenes)
10:01:40.000 [v] depth of field: the paused scene kept for the Depth of Field page: 2560x1440 copy + blur levels + its depth map (focus 1450, frame 9000)
10:02:00.000 [v] depth of field: stage 2 room 2: drawn in 0 of 300 frames it was asked for (no focus 0, the probe 290; the cut-scene's own focus 0); focus 0 .. 0; ours 0.000 ms a frame on average, 0.000 at most; Cut-scenes High; probe: flat (one depth almost everywhere: cleared or overwritten): off in this room; render targets 10.6 MB
10:03:00.000 depth of field: off for this session: the scene could not be copied (CopyRects) (hr 80004005): nothing is drawn from now on
"""

# v3.44 DF344: the room line's reasons (every asked frame not drawn has one; by-design skips apart)
V344D = V333_END + """10:00:00.100 depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to 10012340); DepthOfField Always, Strength Low; probe on (once per room); preview on (the Screen Effects page, 0x526A53)
10:00:04.000 [v] depth of field: stage 2 room 4: not drawn: the room's depth probe is pending (DepthOfField Always)
10:00:05.000 depth of field: first frame drawn (Always, Low): focus 1450 (the player), 3 slice(s), 3 blurred band(s), 0.41 ms; render targets 10.6 MB
10:01:00.000 [v] depth of field: stage 2 room 4: drawn in 3000 of 3146 frames it was asked for (not drawn: the player is behind the camera or too far 136, the room's depth probe is pending 10; the cut-scene's own focus 0); skipped by design: paused or an overlay 400; focus 900 .. 2100; ours 0.420 ms a frame on average, 1.250 at most; Always Low; probe: the scene's depth: on; render targets 10.6 MB
10:01:40.000 [v] depth of field: the paused scene kept for the Screen Effects preview: 2560x1440 copy + blur levels + its depth map (focus 1450, frame 9000)
10:02:00.000 [v] depth of field: stage 1 room 2: drawn in 0 of 0 frames it was asked for (not drawn: none; the cut-scene's own focus 0); skipped by design: Room 302's first-person view 900, paused or an overlay 50; focus 0 .. 0; ours 0.000 ms a frame on average, 0.000 at most; Always Low; probe: not probed: on; render targets 10.6 MB
10:03:00.000 [v] depth of field: stage 2 room 2: drawn in 0 of 946 frames it was asked for (not drawn: the player's position could not be projected 946; the cut-scene's own focus 0); skipped by design: none; focus 0 .. 0; ours 0.000 ms a frame on average, 0.000 at most; Always Low; probe: not probed: on; render targets 10.6 MB
"""


# v3.45 QS345: DoF Strength Low / Medium / High and the one-time migration of a v3.44 Low
V345D = V333_END + """10:00:00.090 depth of field: [ScreenEffects] DofStrength=Low (v3.43 / v3.44) is v3.45's Medium (the same look; v3.45 has a new, gentler Low): written as Medium (marker DofStrengthV=345)
10:00:00.100 depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to 10012340); DepthOfField Always, Strength Medium; probe on (once per room); preview on (the Screen Effects page, 0x526A53)
10:01:30.000 [v] depth of field: Strength Low (DepthOfField Always)
10:01:31.000 [v] depth of field: DepthOfField Cut-scenes (Strength Low)
10:01:32.000 [v] depth of field: Strength High (DepthOfField Cut-scenes)
"""


# v3.47 DF347: the depth of field eases in (the install's ramp line, the first frame), and the fallback without the blend shader
V347D = V333_END + """10:00:00.100 depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to 10012340); DepthOfField Always, Strength Medium; probe on (once per room); preview on (the Screen Effects page, 0x526A53)
10:00:00.101 [v] depth of field: the blur eases in: Low 4 far + 4 near slices, Medium 8 + 4, High 12 + 8 (a quarter level each, the ramps centred on v3.46's steps)
10:00:05.000 depth of field: first frame drawn (Always, Medium): focus 627 (the player), 12 slice(s), 12 blurred band(s), 0.41 ms; render targets 33.6 MB; eased in (a quarter level a step)
10:01:00.000 [v] depth of field: stage 2 room 4: drawn in 3000 of 3010 frames it was asked for (not drawn: the room's depth probe is pending 10; the cut-scene's own focus 0); skipped by design: none; focus 600 .. 700; ours 0.120 ms a frame on average, 0.300 at most; Always Medium; probe: the scene's depth: on; render targets 33.6 MB
"""
V347DN = V333_END + """10:00:00.100 depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to 10012340); DepthOfField Always, Strength High; probe on (once per room); preview on (the Screen Effects page, 0x526A53)
10:00:05.000 depth of field: the blend shader (bloom's up-step) could not be made: the blur steps by whole levels here (not eased in)
10:00:05.001 depth of field: first frame drawn (Always, High): focus 627 (the player), 20 slice(s), 5 blurred band(s), 0.30 ms; render targets 33.6 MB; whole levels (no blend shader: not eased in)
"""


# v3.46 OM346: Stick Movement's lines (omni.inc), in the formats the dll writes
OM_INSTALL = ("omni: %s (005543C0 after 00553D20: the stick floats; raw axes from GetDeviceState on the game's pad or the call "
              "sites 00407B15 / 00407B28, picked by a self-check); [Input] OmniMove %s, OmniDeadZone %s; gameplay only")
V346O = V333_END + "10:00:00.100 " + OM_INSTALL % ('in', 'Off', '0.30') + """
10:00:05.000 omni: OmniMove Direction (was Off; Options)
10:00:05.100 omni: bindings: TurnRight 0003, TurnLeft 0001, MoveForward 0401, MoveBackward 0403 = the left stick (stock)
10:00:06.000 [v] omni: the stick is pushed in game state 6 / 2, overlay 2, phase 2: not gameplay, the game's own input stays
10:00:06.500 [v] omni: the stick is pushed in game state 6 / 2, overlay 2, phase 2: not gameplay, the game's own input stays
10:00:09.000 omni: self-check (check 1): GetDeviceState 40 of 40 agree, call sites 40 of 40 agree: source GetDeviceState
10:01:00.000 omni: OmniMove Analog (was Direction; Options); with Playable Eileen her walk / run speed stays its own (WalkInput / RunInput)
10:01:00.001 [v] omni: Direction: since the last change 3000 frame(s) written (0 of them from the call sites), 0 stopped inside the dead zone
10:02:00.000 ANOMALY: omni: GetDeviceState agreed with the game's direction bits in only 500 of 600 frames: self-check again with the other source
10:02:02.000 omni: self-check (check 2): GetDeviceState 0 of 0 agree, call sites 40 of 40 agree: source call sites
10:02:30.000 omni: [Input] OmniDeadZone read again: 0.25 (was 0.30)
10:03:00.000 omni: [Input] OmniMove read again: Off (was Analog)
10:03:00.001 [v] omni: Analog: since the last change 2000 frame(s) written (1500 of them from the call sites), 12 stopped inside the dead zone
"""
V346N = V333_END + """10:00:00.100 omni: 005543C0 (call 00553D20) is not the stock code (another mod?): Stick Movement does nothing
10:00:00.101 """ + OM_INSTALL % ('OFF', 'Direction', '0.30') + """
10:00:00.102 ANOMALY: omni: [Input] OmniMove is Direction but the hook is not in: the stick stays 8-way
"""
V346B = V333_END + "10:00:00.100 " + OM_INSTALL % ('in', 'Analog', '0.25') + """
10:00:01.000 omni: bindings: TurnRight 3580, TurnLeft 3480, MoveForward 2001, MoveBackward 2003 = not a stick: Stick Movement does nothing until movement is on a stick (the left stick, or the sticks swapped)
10:00:02.000 omni: self-check (check 1): GetDeviceState 3 of 40 agree, call sites 2 of 40 agree: source none (neither matches the game's own direction bits: the game's 8 directions stay)
10:00:02.001 ANOMALY: omni: no raw stick source matches the game's direction bits (GetDeviceState 3 of 40, call sites 2 of 40 agree): Stick Movement does nothing this session
10:00:03.000 omni: the swapped-stick bindings need GetDeviceState, which is not used this session: the game's 8 directions stay
10:00:04.000 omni: faulted (code C0000005): Stick Movement is off for this session
"""
# v3.47 CT347: the row on Option 2, Playable Eileen's answer, the focus-loss lines (omni.inc OmRowInstall / OmPeProbe / OmFailTick)
V347O = V333_END + "10:00:00.100 " + OM_INSTALL % ('in', 'Off', '0.30') + """
10:00:00.200 omni: the Stick Movement row is on Option 2 (Control), row 7 of 8 above Exit: Off
10:00:05.000 omni: Playable Eileen does not read SH4R_StickAmount (no SH4PE_StickAmountOn): her speed stays her own
10:00:06.000 omni: OmniMove Analog (was Off; Options); with Playable Eileen her walk / run speed stays its own (WalkInput / RunInput)
10:01:00.000 [v] omni: GetDeviceState on the game's pad failed (hr 8007001E) while the game window is not in front (focus lost): the game's own input until it reads the pad again
10:01:05.000 [v] omni: GetDeviceState reads the game's pad again after 300 failed frame(s) (the game window was not in front)
10:02:00.000 [v] omni: GetDeviceState on the game's pad failed (hr 8007001E) for 4 frame(s) around a focus loss (the game window not in front): it reads the pad again
10:03:00.000 ANOMALY: omni: GetDeviceState on the game's pad failed (hr 8007001E) after the game's own read, 61 frame(s) with the game window in front so far: those frames the game's own input
10:03:02.000 omni: GetDeviceState reads the game's pad again after 120 failed frame(s)
10:04:00.000 omni: Playable Eileen follows Analog's push (SH4PE_StickAmountOn 1): her walk / run speed scales with it
"""
V347N = V333_END + "10:00:00.100 " + OM_INSTALL % ('in', 'Direction', '0.30') + """
10:00:00.200 omni: no Options pages (see above): no Stick Movement row; [Input] OmniMove still applies (also edited while the game runs)
"""
# v3.48 OM348: Omni-Directional Movement, Off / On: an old value written back at the start, the new names, an old value read
# again while the game runs, an unknown value, the faults (omni.inc OmReadIni / OmParseLogged / OmRowInstall / OmPeProbe)
V348O = V333_END + """10:00:00.050 omni: [Input] OmniMove=Analog (v3.46 / v3.47) is On now (Omni-Directional Movement: Off / On; On = v3.47's Analog): written as On
10:00:00.100 """ + OM_INSTALL % ('in', 'On', '0.30') + """
10:00:00.200 omni: the Omni-Directional Movement row is on Option 2 (Control), row 7 of 8 above Exit: On
10:00:05.000 omni: Playable Eileen follows the stick's push (SH4PE_StickAmountOn 1): her walk / run speed scales with it
10:00:06.000 omni: OmniMove Off (was On; Options)
10:00:07.000 omni: OmniMove On (was Off; Options)
10:00:09.000 omni: self-check (check 1): GetDeviceState 40 of 40 agree, call sites 40 of 40 agree: source GetDeviceState
10:01:00.000 omni: [Input] OmniMove=Direction (v3.46 / v3.47) is read as On (Omni-Directional Movement); the next start writes it as On
10:02:00.000 omni: [Input] OmniMove read again: Off (was On)
10:02:00.001 [v] omni: On: since the last change 900 frame(s) written (0 of them from the call sites), 3 stopped inside the dead zone
10:03:00.000 omni: [Input] OmniMove=Fast is not Off or On (0, 1): Off
"""
V348F = V333_END + """10:00:00.050 omni: [Input] OmniMove=2 (v3.46 / v3.47) is On now (Omni-Directional Movement: Off / On; On = v3.47's Analog): could not be written to SH4Refined.ini (error 5): On for this start, the next start tries again
10:00:00.100 """ + OM_INSTALL % ('in', 'On', '0.30') + """
10:00:00.200 omni: no Options pages (see above): no Omni-Directional Movement row; [Input] OmniMove still applies (also edited while the game runs)
10:00:01.000 omni: bindings: TurnRight 3580, TurnLeft 3480, MoveForward 2001, MoveBackward 2003 = not a stick: Omni-Directional Movement does nothing until movement is on a stick (the left stick, or the sticks swapped)
10:00:04.000 omni: faulted (code C0000005): Omni-Directional Movement is off for this session
10:00:04.001 ANOMALY: omni: the stick hook faulted: Omni-Directional Movement is off for this session
"""
# v3.49 LK349: the analog look ([Input] OmniLook; omni.inc OmInstall / OmReadLook / OmIniReread / OmLookBindNow /
# OmLookCheckLog / OmLookFlush), in the formats the dll writes
OL_START = ("omni: look: [Input] OmniLook %s (the analog look in Room 302's first-person view; while Omni-Directional Movement is "
            "On, with GetDeviceState the confirmed source and the look on a stick: the raw stick into stick[1] 01236C54 / "
            "01236C58, dead zone 0.20); Room 302's X-turns rule for the left stick's x: on")
OL_COUNT = ("[v] omni: look: %s, %d s (%s): the look stick pushed %d frame(s): %d analog (OmniLook Room302), the game's own %d "
            "(OmniLook not here %d, a look key %d, no pad read %d, look self-check %s %d, look not on a stick %d); the game's "
            "reversal latch %d frame(s) (x %d, y %d); a push the game's 8-way look missed %d frame(s); the right stick's turn "
            "kept from a small sideways push %d frame(s) (X turns)")
V349L = V333_END + "10:00:00.100 " + OM_INSTALL % ('in', 'On', '0.30') + """
10:00:00.101 """ + OL_START % 'Room302' + """
10:00:05.000 omni: look bindings: LookRight 1403, LookLeft 1401, LookUp 0801, LookDown 0803 = the right stick (stock)
10:00:06.000 omni: look self-check: GetDeviceState 40 of 40 agree with the game's look bits: the analog look is on (OmniLook Room302)
10:05:00.000 """ + OL_COUNT % ('Room 302', 294, 'left', 5000, 4900, 100, 0, 60, 30, 'GetDeviceState', 10, 0, 120, 80, 40, 700, 2000) + """
10:06:00.000 omni: [Input] OmniLook read again: All (was Room302)
10:07:00.000 """ + OL_COUNT % ('stage 2', 60, 'Omni-Directional Movement Off', 300, 0, 300, 300, 0, 0, 'pending', 0, 0, 5, 5, 0, 20, 0) + """
10:08:00.000 omni: [Input] OmniLook=Sometimes is not Off, Room302 or All (0, 1, 2): Room302
"""
V349LN = V333_END + "10:00:00.100 " + OM_INSTALL % ('in', 'On', '0.30') + """
10:00:00.101 """ + OL_START % 'Room302' + """
10:00:05.000 omni: look bindings: LookRight 3180, LookLeft 3280, LookUp 3380, LookDown 3480 = not a stick: the game's own look (no analog look)
10:00:06.000 omni: look self-check: GetDeviceState 3 of 40 agree with the game's look bits: the game's own 8-way look stays this session
10:00:06.001 ANOMALY: omni: the look stick's GetDeviceState axes agreed with the game's look bits in only 3 of 40 frames: no analog look this session
"""
V348W = V333_END + """10:00:00.050 omni: [Input] OmniMove=Direction (v3.46 / v3.47) is On now (Omni-Directional Movement: Off / On; On = v3.47's Analog): not written (SH4Refined.ini could not be made this start: nothing is written)
10:00:00.100 """ + OM_INSTALL % ('in', 'On', '0.30') + """
10:00:00.200 omni: Option 2's table is not the stock one (10001234, 9 rows): no Omni-Directional Movement row; [Input] OmniMove still applies
"""


# v3.47 RG347: Refined's own Regeneration (regen.inc), in the formats the dll writes
RG_INSTALL = ("regeneration: %s (no exe patch; tick 004EF982, heals through 0052D1D0 PlayerHealAmount, aura check [010A3320] / "
              "[010A2EE0], Room 302 after its own heal ends); [Gameplay] Regen %s (%s %% of max health a minute), RegenDelay %s s; "
              "who heals now: %s")
# v3.48 RZ348: the Randomizer's lines (randomizer.inc)
RZ_INSTALL = ('randomizer: [Randomizer] Items %s, KeepSpecials Off, Room302 In, PerVisit On, Seed 123456; self-check: %d plain '
              'pickup(s) and 27 weapon spot(s) in 17 script(s) (stock 137 / 27)%s; 19 weapons may move; applied at each room load '
              'through 004F0AF8 (patch-and-restore of the item and record words, no exe patch)')
V348RZ = V333_END + """10:00:00.040 randomizer: [Randomizer] Seed was "0" (missing, 0 or not a number): rolled 123456 and written
10:00:00.041 """ + RZ_INSTALL % ('Random', 137, ': the stock scripts') + """
10:00:05.000 randomizer: the first room load through 004F0AF8 (stage 2 room 1): Items Random
10:00:05.001 randomizer: stage 2 visit 0: Random, 12 of 12 pickup(s) in the pool (7 weapon spot(s)), 10 changed (seed 123456, Keep Specials Off, Room 302 In, Change Each Visit On, world visit 1); Holy Candle 1 (stock 0), Saint Medallion 0 (stock 0), Sword of Obedience 0 (stock 0), weapons 4 (stock 7)
10:00:09.000 randomizer: room 2:5 (visit 0, #1): 1 of 1 pickup(s) changed, 0 weapon(s) (Items Random)
10:00:09.001 [v] randomizer: room 2:5: bit 63 Pistol Bullets -> Holy Candle
10:00:40.000 randomizer: room 2:5 (visit 0, #2): 1 of 1 pickup(s) changed, 1 weapon(s) (Items Random)
10:01:00.000 randomizer: Items Shuffle (was Random; applies at the next room load)
10:01:01.000 randomizer: New Seed 777 (was 123456; saved; applies at the next room load)
10:01:02.000 randomizer: PerVisit Off (was On; applies at the next room load)
10:01:30.000 randomizer: stage 2 visit 1: Shuffle, 25 of 25 pickup(s) in the pool (7 weapon spot(s)), 20 changed (seed 777, Keep Specials Off, Room 302 In, Change Each Visit Off, world visit 0); Holy Candle 3 (stock 3), Saint Medallion 1 (stock 1), Sword of Obedience 1 (stock 1), weapons 7 (stock 7)
10:01:31.000 randomizer: room 2:7 (visit 1, #0): 2 of 3 pickup(s) changed, 1 weapon(s) (Items Shuffle); some words were not the stock value: left as found
10:01:31.001 ANOMALY: randomizer: room 2:7: 1 pickup word(s) did not hold the stock value (another mod?): left as found
10:02:00.000 randomizer: [Randomizer] KeepSpecials read again: On (applies at the next room load)
10:02:30.000 randomizer: stage 3 visit 1: Random, 12 of 18 pickup(s) in the pool (1 weapon spot(s)), 9 changed (seed 777, Keep Specials On, Room 302 In, Change Each Visit Off, world visit 0); Holy Candle 0 (stock 0), Saint Medallion 0 (stock 0), Sword of Obedience 0 (stock 1), weapons 1 (stock 1); fewer Swords of Obedience than the stock (no guarantee)
10:09:00.000 randomizer: session: Items Shuffle (seed 777, Keep Specials On, Room 302 In, Change Each Visit Off): 40 room load(s), 3 with pickups in the pool, 4 pickup(s) changed, 2 weapon(s) placed, 3 plan(s), 1 word(s) left as found
"""
# v3.49 EF349: Enemies Follow's value: the install line, the value's own lines, a change to Victims Only and back, the
# monsters sent home (enemies.inc EnmFollowSay / EnmPursueDropMonsters / EnmMonstersHomeAtLoad)
V349F = V333_END + """10:00:00.200 enemies: in (room load 004F0AF8, kill count 00450DB0, bone pool 00419830, victims 00486199, room change 004F0A5A, save 00465438); EnemyCopies x2, EnemyFollow Monsters + Victims, EnemyRevive Off, BonePool -1 (-1 Auto = 4096 while Copies or Follow is on), FollowMax 32, FollowDelay 3.0 s, FollowRange 800, CopyStoryGhosts 1, ReviveEventDogs 1, FollowCopies 1, FollowRangeVictims 1500 (v3.38: every ghost in the room follows), FollowKeepDoors -1 (-1 = Always: the whole world visit), LateCopies 1, DownKeep 0 (the game's own down countdown), FollowStoryGhosts 1
10:00:01.000 enemies: Enemies Follow Monsters + Victims: monsters and the victims' ghosts follow you through doors (the story ghosts too, and their copies)
10:00:05.000 gameplay: EnemyFollow Victims Only
10:00:05.100 enemies: Enemies Follow Victims Only: only the victims' ghosts follow you through doors (the story ghosts too, and their copies); monsters and their copies stay in their rooms (any left in other rooms go home at the next room load)
10:00:05.101 [v] enemies: 2 monster(s) and monster cop(ies) stop pursuing you: Enemies Follow takes no monsters now (Victims Only)
10:00:09.000 enemies: Enemies Follow is Victims Only: 2 monster(s) left in other rooms went home (0 of them killed there: they stay dead)
10:00:09.001 enemies: Enemies Follow is Victims Only: 1 monster copy that followed you is made in its own room again (ghost copies stay)
10:00:12.000 enemies: through door 7 from room 4 to 5: 1 follow (0 monster(s), 0 of them copies, 1 ghost(s); 1 from that room, 0 pursuing from other rooms or on their way): FFFF0007 ghost 0B; 0 wait for a later door (0 FollowMax 32 full, 0 files, 0 story rule, 0 down in their room), 0 already here, 0 stopped, 0 in that room had not spotted you
10:01:00.000 gameplay: [Gameplay] EnemyFollow read again: Monsters + Victims
10:01:00.100 enemies: Enemies Follow Monsters + Victims: monsters and the victims' ghosts follow you through doors (the story ghosts too, and their copies)
"""
# v3.49 EF349: Victims Only from the start, a menu value that could not be saved, Off with the copies' ledger cleared
V349F2 = V333_END + """10:00:00.200 enemies: in (room load 004F0AF8, kill count 00450DB0, bone pool 00419830); EnemyCopies Off, EnemyFollow Victims Only, EnemyRevive Off, BonePool -1 (-1 Auto = 4096 while Copies or Follow is on), FollowMax 32
10:00:01.000 enemies: Enemies Follow Victims Only: only the victims' ghosts follow you through doors (the story ghosts too, and their copies); but 00486199 is not in: no ghost can follow, so nobody does
10:00:30.000 gameplay: EnemyFollow Off applies now but could not be saved in SH4Refined.ini (error 5): it is lost at the next start
10:00:40.000 enemies: Enemies Follow is off: 3 copies that followed you are made in their own rooms again
"""
# v3.49 EF349: Off all along: no setting line
V349F3 = V333_END + """10:00:00.200 enemies: in (room load 004F0AF8, kill count 00450DB0, bone pool 00419830); EnemyCopies Off, EnemyFollow Off, EnemyRevive Off, BonePool -1
10:00:01.000 enemies: Enemies Follow Off: nobody follows you through doors
"""
# v3.50 EF350: the value renamed Victims (its lines), followers kept at two saves, a load that finds them and their room
# load's restore, a load with none, a restore refused, Enemy Pursuit's install line, a change, the lost-track door lines
V350EF = V333_END + """10:00:00.200 enemies: in (room load 004F0AF8, kill count 00450DB0, bone pool 00419830, victims 00486199, room change 004F0A5A, save 00465438); EnemyCopies x2, EnemyFollow Victims, EnemyRevive Off, BonePool -1 (-1 Auto = 4096 while Copies or Follow is on), FollowMax 32
10:00:00.201 enemies: followers across saves in (snapshot 004FC1A4, load 004FC6ED; side file G:\\Game\\save\\SH4Refined_follow.dat); Enemy Pursuit Normal (memory 10 s, doors away 2; Relentless = v3.49)
10:00:01.000 enemies: Enemies Follow Victims: only the victims' ghosts follow you through doors (the story ghosts too, and their copies); monsters and their copies stay in their rooms (any left in other rooms go home at the next room load)
10:00:02.000 enemies: Enemies Follow is Victims: 1 monster(s) left in other rooms went home (0 of them killed there: they stay dead)
10:00:02.001 enemies: Enemies Follow is Victims: 2 monster copies that followed you are made in their own rooms again (ghost copies stay)
10:00:03.000 gameplay: EnemyFollow Monsters + Victims
10:00:10.000 enemies: through door 4 from room 2 to 4: Enemy Pursuit Normal: 2 lost track of you (1 out of their senses, 1 down when you left, 0 did not catch up); 3 more had spotted you but forgot
10:00:20.000 enemies: through door 2 from room 4 to 2: Enemy Pursuit Normal: 0 lost track of you (0 out of their senses, 0 down when you left, 0 did not catch up); 1 more had spotted you but forgot
10:00:22.000 enemies: follow save: kept for this save (stage 2 room 2, game time 44565, key 1A2B3C4D): 3 pursuer(s) (2 here, 1 away), 1 on their way here, 2 ghost(s) let into this room, 0 cop(ies) that followed you; side file 1 entr(ies)
10:00:24.000 enemies: follow load: this save's followers are kept (stage 2 room 2, key 1A2B3C4D): 3 pursuer(s), 1 on their way, 2 ghost(s) let in, 0 cop(ies): they come back at its room load
10:00:25.000 enemies: follow restore: room 2:2 after a load: 3 pursuer(s) back (2 here with you, 1 away), 1 on their way here, 2 ghost(s) let in, 0 cop(ies) in the ledger; left out: 0 without their record (the save filter, or gone), 0 of a kind Enemies Follow Monsters + Victims takes no longer
10:00:30.000 gameplay: EnemyPursuit Forgetful
10:00:40.000 enemies: through door 4 from room 2 to 4: Enemy Pursuit Forgetful: 1 lost track of you (0 out of their senses, 0 down when you left, 1 did not catch up); 0 more had spotted you but forgot
10:00:50.000 enemies: follow save: kept for this save (stage 2 room 4, game time 47000, key 0000BEEF): 1 pursuer(s) (1 here, 0 away), 0 on their way here, 0 ghost(s) let into this room, 2 cop(ies) that followed you; side file 2 entr(ies) (not written: kept for this session)
10:00:55.000 [v] enemies: follow load: no followers kept for this save (stage 5 room 14, game time 820326, key 00000001): nothing to restore
10:01:00.000 enemies: follow restore: Enemies Follow is off: this save's followers are not restored
10:01:05.000 gameplay: [Gameplay] EnemyPursuit read again: Relentless
"""
# v3.51 EP351: Enemy Pursuit inside the room (pursuit_room.inc): the install line, the verbose give-ups, the rooms' lines
V351EP = V333_END + """10:00:00.201 enemies: followers across saves in (snapshot 004FC1A4, load 004FC6ED; side file G:\\Game\\save\\SH4Refined_follow.dat); Enemy Pursuit Forgetful (memory 4 s, doors away 1; Relentless = v3.49)
10:00:00.202 enemies: Enemy Pursuit in the room in (Normal / Forgetful: monsters give up after 10 / 4 s unsensed, ghosts after 12 / 6 s far or 25 / 12 s without touching you; Hummer, Gum Head, Sniffer Dog, Twin Victim, Wall Man, victims' ghosts; Relentless = stock)
10:00:10.000 [v] enemies: monster 03000005 (Hummer, type 03 v0) gave up the chase in room 4:5: it had not sensed you for 4 s (Enemy Pursuit Forgetful; AI state 6 -> 4, its own routine; it ignores you 3 s unless you hit it or come close)
10:00:11.000 [v] enemies: ghost FFFF0012 (type 0B v0) gave up the chase in room 4:5: farther than 1500 from you for 6 s (Enemy Pursuit Forgetful; AI state 2 -> 1; it drifts back towards its spot and rests 15 s)
10:00:12.000 [v] enemies: copy 1E000003 (Sniffer Dog, type 0A v0) gave up the chase in room 4:5: it was down when you walked off (Enemy Pursuit Forgetful; AI state 4: the dog's own give-up at its next step, then it wanders)
10:00:40.000 [v] enemies: ghost FFFF0012 (type 0B v0) gave up the chase in room 4:5: it chased you 12 s without touching you (Enemy Pursuit Forgetful; AI state 2 -> 1; it drifts back towards its spot and rests 15 s) (again)
10:00:50.000 enemies: room 4:5 (Enemy Pursuit Forgetful): 4 gave up the chase in the room (2 monster(s), 2 ghost(s); 1 of them again, 1 down when you walked off); 3 set back during their rest; 0 left to the game's own rules (you left the room)
10:00:50.001 enemies: through door 4 from room 5 to 6: Enemy Pursuit Forgetful: 2 lost track of you (2 out of their senses, 0 down when you left, 0 did not catch up); 1 more had spotted you but forgot
10:01:20.000 [v] enemies: monster 07000001 (Gum Head, type 07 v1) gave up the chase in room 4:6: it had not sensed you for 4 s (Enemy Pursuit Forgetful; AI state 11 -> 9, its own routine)
10:01:30.000 enemies: room 4:6 (Enemy Pursuit Forgetful): 1 gave up the chase in the room (1 monster(s), 0 ghost(s); 0 of them again, 0 down when you walked off); 0 set back during their rest; 1 left to the game's own rules (a save load)
"""
# v3.52 EP352: Forgetful's ghosts: out of their sight, the calm after the rest, dives set back (the room line's calm part)
V352EP = V333_END + """10:00:00.201 enemies: followers across saves in (snapshot 004FC1A4, load 004FC6ED; side file G:\\Game\\save\\SH4Refined_follow.dat); Enemy Pursuit Forgetful (memory 4 s, doors away 1; Relentless = v3.49)
10:00:00.202 enemies: Enemy Pursuit in the room in (Normal / Forgetful: monsters give up after 10 / 4 s unsensed, ghosts after 12 / 3 s far (2500 / 1200 u) or 25 / 8 s without touching you, Forgetful's also after 3 s out of their sight, then rest 8 / 15 s and stay calm until they sense you again (2000 / 1200 u, nothing between); Hummer, Gum Head, Sniffer Dog, Twin Victim, Wall Man, victims' ghosts; Relentless = stock)
10:00:10.000 [v] enemies: ghost FFFF0007 (type 0B v3) gave up the chase in room 2:1: farther than 1200 from you for 3 s (Enemy Pursuit Forgetful; AI state 2 -> 1; it drifts back towards its spot and rests 15 s, then stays calm until it senses you again)
10:00:10.020 [v] enemies: ghost FFFF0007 in room 2:1 began a dive into a wall towards you in its rest (4512 u away; AI state 7 step 0): set back to its idle float (Enemy Pursuit Forgetful)
10:00:12.000 [v] enemies: ghost FFFF000A (type 0B v0) gave up the chase in room 2:1: you were out of its sight for 3 s (Enemy Pursuit Forgetful; AI state 2 -> 1; it drifts back towards its spot and rests 15 s, then stays calm until it senses you again)
10:00:40.000 [v] enemies: ghost FFFF000A in room 2:1 sensed you again and took up the chase (900 u away; AI state 2): its calm after the give-up ends (Enemy Pursuit Forgetful)
10:00:48.000 [v] enemies: ghost FFFF000A (type 0B v0) gave up the chase in room 2:1: it chased you 8 s without touching you (Enemy Pursuit Forgetful; AI state 2 -> 1; it drifts back towards its spot and rests 15 s, then stays calm until it senses you again) (again)
10:00:50.000 enemies: room 2:1 (Enemy Pursuit Forgetful): 3 gave up the chase in the room (0 monster(s), 3 ghost(s); 1 of them again, 0 down when you walked off); 4 set back during their rest; 0 left to the game's own rules (you left the room); ghosts' calm: 1 ghost dive(s) into a wall stopped, 1 ghost(s) sensed you again
10:01:30.000 enemies: room 2:2 (Enemy Pursuit Forgetful): 1 gave up the chase in the room (0 monster(s), 1 ghost(s); 0 of them again, 0 down when you walked off); 0 set back during their rest; 0 left to the game's own rules (a save load); ghosts' calm: 2 ghost dive(s) into a wall stopped, 0 ghost(s) sensed you again
"""
V348RZB = V333_END + """10:00:00.040 randomizer: [Randomizer] Items "Randomize" is not Off / Shuffle / Random: Random (the default)
10:00:00.041 """ + RZ_INSTALL % ('Random', 136, ': some scripts are not the stock ones (their pickups are left alone)') + """
10:00:06.000 ANOMALY: randomizer: stage 2 visit 1: the script at 00683AA0 is not the stock one (17 plain pickup(s), 7 weapon spot(s)): its pickups are left alone
10:00:07.000 randomizer: stage 5 visit 1: Shuffle, 25 of 25 pickup(s) in the pool (5 weapon spot(s)), 20 changed (seed 1, Keep Specials Off, Room 302 In, Change Each Visit On, world visit 3); Holy Candle 4 (stock 4), Saint Medallion 1 (stock 1), Sword of Obedience 1 (stock 1), weapons 5 (stock 5); SOME ITEMS COULD NOT BE PLACED (they keep their spots)
"""
# v3.49 RZ349: the Randomizer's Monsters (rzmon.inc)
RZM_INSTALL = ('randomizer: [Randomizer] Monsters %s; self-check: %d source placement(s) of 783 enemy placement(s) in 17 script(s) (stock '
               '235 / 783)%s; targets Whitestool, Hummer, Gum Head, Gum Head (v1), Twin Victim, Twin Victim (v1), Tremer, Sniffer Dog, '
               'Sniffer Dog (v1) (Gum Heads only with a crash guard); applied at each room load through 004F0AF8 (patch-and-restore of '
               'the type / mode / y words); save filter 00465468 %s')
V349RZM = V333_END + """10:00:00.041 """ + RZ_INSTALL % ('Random', 137, ': the stock scripts') + """
10:00:00.042 """ + RZM_INSTALL % ('This World', 235, ': the stock scripts', 'in') + """
10:00:05.000 randomizer: monsters: Gum Heads may be rolled (Playable Eileen's crash guard of the Gum Head's group call: 0049B70A in, 0045C3C0 in)
10:00:05.001 randomizer: monsters: the first room with source placements (stage 2 room 4): Monsters This World
10:00:05.002 randomizer: monsters: Twin Victim is no monster of stage 2's scripts: its sounds may be missing there (allowed)
10:00:05.003 randomizer: monsters: room 2:4 (visit 0, #1): 1 of 1 placement(s) changed (This World): Tremer 1; 0 kept their record's type, 0 record(s) deleted; kept as placed: 0 for the bone budget, 0 for files, 0 without a floor; files loaded for 1 type(s) (612 KB)
10:00:05.004 [v] randomizer: monsters: room 2:4: n1 Sniffer Dog v0 -> Tremer v0
10:00:30.000 randomizer: monsters: room 2:5 (visit 0, #2): 1 of 3 placement(s) changed (This World): Gum Head 1, Sniffer Dog 2; 1 kept their record's type, 1 record(s) deleted; kept as placed: 1 for the bone budget, 0 for files, 0 without a floor; some words were not the stock value: left as found; files loaded for 0 type(s) (0 KB)
10:00:30.001 ANOMALY: randomizer: monsters: room 2:5: 1 placement word(s) did not hold the stock value (another mod?): left as found
10:01:00.000 randomizer: Monsters This Room (was This World; applies at the next room load)
10:01:10.000 randomizer: monsters: room 3:5: Twin Victim's files could not be loaded (16 extra files in this room already): its placements stay as placed
10:01:20.000 randomizer: monsters: the cheat table's Enemy Randomizer (entry 7700: 0052CAA9 / 0052CB2B) is on: Refined's Monsters stay off while it is (it retypes the placements itself)
10:01:40.000 randomizer: monsters: the cheat table's Enemy Randomizer (entry 7700: 0052CAA9 / 0052CB2B) is off again: Refined's Monsters apply again
10:02:00.000 randomizer: Monsters Off (was This Room; applies at the next room load)
10:02:10.000 randomizer: monsters: room 4:11: 2 record(s) of monsters made as another type before are deleted (they come back as placed)
10:02:20.000 randomizer: [Randomizer] Monsters read again: This World (applies at the next room load)
10:03:00.000 randomizer: monsters: a save: 1 record(s) of monsters made as another type are left out of it (they come back as placed after a load)
10:03:00.001 [v] randomizer: monsters: a save: 02050003 (made as Gum Head v0, placed as another type) is left out of it
10:09:00.000 randomizer: session: Items Random (seed 123456, Keep Specials Off, Room 302 In, Change Each Visit On): 40 room load(s), 0 with pickups in the pool, 0 pickup(s) changed, 0 weapon(s) placed, 1 plan(s), 0 word(s) left as found
10:09:00.001 randomizer: monsters session: This World (seed 123456, Change Each Visit On): 40 room load(s), 2 with source placements randomized, 2 placement(s) changed, 1 kept their record's type, 1 record(s) deleted (+2 with Monsters off); kept as placed: 1 for the bone budget, 1 for files, 0 without a floor; files loaded 1 time(s) (612 KB); 1 room load(s) with the cheat table's Enemy Randomizer on; 1 save(s), 1 record(s) left out; 1 word(s) left as found
"""
V349RZMB = V333_END + """10:00:00.040 randomizer: [Randomizer] Monsters "Everywhere" is not Off / This Room / This World: This World (the default)
10:00:00.042 """ + RZM_INSTALL % ('This World', 230, ': some scripts are not the stock ones (their monsters are left alone)',
                                   'NOT in (unexpected bytes: a save may hold randomized monsters)') + """
10:00:05.000 randomizer: monsters: Gum Heads are never rolled (Playable Eileen's crash guard of the Gum Head's group call: 0049B70A not in, 0045C3C0 not in)
10:00:05.001 randomizer: monsters: the hit-shape pool is the stock 100 ([Gameplay] HitPool): Monsters stay as placed (a new type may need more shapes than the room's own)
10:00:06.000 ANOMALY: randomizer: monsters: stage 2 visit 1: the script at 00683AA0 is not the stock one (20 source placement(s) of 53): its monsters are left alone
"""
# v3.50 RM350: the Monsters in a save-based game (the game's own records adopted), All Worlds, the reasons, in the dll's formats
RM350_TAIL = ("; the game's own records: %d adopted (%d made as another type); stock type kept: %d rolled it, %d by a stock record, %d "
              "recorded elsewhere, %d for another writer's record, %d with no target")
V350RZM = V333_END + """10:00:00.042 """ + RZM_INSTALL % ('All Worlds', 235, ': the stock scripts', 'in') + """
10:00:05.001 randomizer: monsters: the first room with source placements (stage 5 room 11): Monsters All Worlds
10:00:05.002 randomizer: monsters: Twin Victim is no monster of stage 5's scripts: its sounds may be missing there (allowed)
10:00:05.003 randomizer: monsters: room 5:11 (visit 1, #1): 7 of 8 placement(s) changed (All Worlds): Whitestool 1, Hummer 2, Twin Victim 3, Sniffer Dog 2; 0 kept their record's type, 7 record(s) deleted; kept as placed: 0 for the bone budget, 0 for files, 0 without a floor; files loaded for 1 type(s) (900 KB)""" + RM350_TAIL % (8, 7, 1, 0, 0, 0, 0) + """
10:00:05.004 [v] randomizer: monsters: room 5:11: n7 Whitestool v0 -> Hummer v0 [the game's record, adopted and deleted]; n8 Whitestool v0 kept as Whitestool v0 (rolled) [the game's record, adopted]
10:00:40.000 randomizer: monsters: room 5:4 (visit 1, #2): 0 of 2 placement(s) changed (All Worlds): Hummer 2; 0 kept their record's type, 0 record(s) deleted; kept as placed: 0 for the bone budget, 0 for files, 0 without a floor; files loaded for 0 type(s) (0 KB)""" + RM350_TAIL % (0, 0, 0, 1, 1, 0, 0) + """
10:00:50.000 randomizer: monsters: room 2:4 (visit 0, #3): 0 of 1 placement(s) changed (This World): Sniffer Dog 1; 0 kept their record's type, 0 record(s) deleted; kept as placed: 0 for the bone budget, 0 for files, 0 without a floor; files loaded for 0 type(s) (0 KB)
10:01:00.000 randomizer: Monsters All Worlds (was This World; applies at the next room load)
10:01:10.000 randomizer: [Randomizer] Monsters read again: All Worlds (applies at the next room load)
10:03:00.000 randomizer: monsters: a save: 7 record(s) of monsters made as another type are left out of it (they come back as placed after a load)
10:09:00.001 randomizer: monsters session: All Worlds (seed 794655925, Change Each Visit On): 40 room load(s), 3 with source placements randomized, 7 placement(s) changed, 0 kept their record's type, 7 record(s) deleted (+0 with Monsters off); kept as placed: 0 for the bone budget, 0 for files, 0 without a floor; files loaded 1 time(s) (900 KB); 0 room load(s) with the cheat table's Enemy Randomizer on; 1 save(s), 7 record(s) left out; 0 word(s) left as found
10:09:00.002 randomizer: monsters session: the game's own records: 8 adopted (7 made as another type); stock type kept: 1 rolled it, 1 by a stock record, 1 recorded elsewhere, 0 with no target
"""
V350RZMB = V333_END + """10:00:00.040 randomizer: [Randomizer] Monsters "Everywhere" is not Off / This Room / This World / All Worlds: This World (the default)
"""
# v3.51 SN351: the Subway's event dogs (room 2:2) and the second roll, in the dll's formats
SN351_EVIN = ("randomizer: monsters: %d of the source placements are event Sniffer Dogs (spawn mode 6 / 7, stock: Subway room 2:2's 3): "
              "rolled once story flag 130 is set (their cut-scene is over), else as placed; a roll of the placement's own type rolls "
              "once more: %s ([Randomizer] MonsterRollAgain)")
SN351_ROOM = ("randomizer: monsters: room 2:%d (visit 0, #%d): %d of %d placement(s) changed (All Worlds): %s; 0 kept their record's type, "
              "%d record(s) deleted; kept as placed: 0 for the bone budget, 0 for files, 0 without a floor; files loaded for 0 type(s) "
              "(0 KB)") + RM350_TAIL
V351SN = V333_END + """10:00:00.042 """ + RZM_INSTALL.replace('235 / 783', '238 / 783') % ('All Worlds', 238, ': the stock scripts', 'in') + """
10:00:00.043 """ + SN351_EVIN % (3, 'On') + """
10:00:05.001 randomizer: monsters: the first room with source placements (stage 2 room 2): Monsters All Worlds
10:00:05.003 """ + SN351_ROOM % (2, 1, 0, 3, 'Sniffer Dog 3', 0, 0, 0, 0, 0, 0, 0, 0) + """; event dogs: 3, story flag 130 clear (kept as placed for their event)
10:00:05.004 [v] randomizer: monsters: room 2:2: n1 Sniffer Dog v0 kept as Sniffer Dog v0 (asleep for its event); n2 Sniffer Dog v0 kept as Sniffer Dog v0 (asleep for its event); n3 Sniffer Dog v0 kept as Sniffer Dog v0 (asleep for its event)
10:01:05.003 """ + SN351_ROOM % (2, 2, 3, 3, 'Whitestool 1, Tremer 1, Twin Victim 1', 3, 3, 3, 0, 0, 0, 0, 0) + """; rolled again for their own type: 1; event dogs: 3, story flag 130 set (rolled as the others)
10:01:05.004 [v] randomizer: monsters: room 2:2: n1 Sniffer Dog v0 -> Whitestool v0 [the game's record, adopted and deleted]; n2 Sniffer Dog v0 -> Tremer v0 (rolled again) [the game's record, adopted and deleted]; n3 Sniffer Dog v0 -> Twin Victim v1 [the game's record, adopted and deleted]
10:01:20.000 """ + SN351_ROOM % (4, 12, 0, 1, 'Sniffer Dog 1', 0, 1, 0, 1, 0, 0, 0, 0) + """; rolled again for their own type: 1
10:01:20.001 [v] randomizer: monsters: room 2:4: n1 Sniffer Dog v0 kept as Sniffer Dog v0 (rolled it twice) [the game's record, adopted]
10:09:00.001 randomizer: monsters session: All Worlds (seed 1398963150, Change Each Visit On): 40 room load(s), 3 with source placements randomized, 3 placement(s) changed, 0 kept their record's type, 3 record(s) deleted (+0 with Monsters off); kept as placed: 0 for the bone budget, 0 for files, 0 without a floor; files loaded 0 time(s) (0 KB); 0 room load(s) with the cheat table's Enemy Randomizer on; 0 save(s), 0 record(s) left out; 0 word(s) left as found
10:09:00.002 randomizer: monsters session: the game's own records: 4 adopted (3 made as another type); stock type kept: 1 rolled it, 0 by a stock record, 0 recorded elsewhere, 0 with no target
10:09:00.003 randomizer: monsters session: 2 placement(s) rolled again for their own type (MonsterRollAgain On); event dogs: 3 planned with story flag 130 set, 3 kept as placed for their event (flag clear)
"""
# v3.49 RZ349L: the Randomizer's looks (rzlooks.inc), in the formats the dll writes
V349RZL = V333_END + """10:00:00.040 randomizer: [Randomizer] Ghosts "Rooms" is not Off / Spots: Spots (the default)
10:00:00.041 randomizer: looks: [Randomizer] EileenOutfit Random, Ghosts Spots (companion Eileen's outfit once per world visit among the unlocked ones; victim ghosts at another spot of their room: patch-and-restore of their position words, no exe patch)
10:00:04.000 randomizer: outfit: world visit 1 (stage 7, half 1, at her world's file list): Nurse (seed 123456, Change Each Visit On; unlocked: Dress, Nurse; the set outfit Default)
10:00:05.000 randomizer: looks: the first room load (stage 7 room 1): Eileen Outfit Random, the companion only: Playable Eileen dresses the player ([Variant] in SH4PlayableEileen.ini); Ghosts Spots, victim ghosts, never the story ghosts
10:00:20.000 randomizer: outfit: Dress chosen in the Extra Menu: kept until the next world visit
10:01:00.000 randomizer: outfit: back to the set outfit Default (a world without her)
10:02:00.000 randomizer: outfit: world visit 3 (stage 5, half 2, at a room load): only the Dress is unlocked in this save: her outfit stays Default
10:02:05.000 randomizer: ghosts: room 5:13 (#1): 2 of 2 victim(s) moved (seed 123456, Change Each Visit On): FFFF0006 0B v1 -1200 0 -1100 -> -400 0 -600 (12 of 30 fit); FFFF0001 0B v2 -3400 0 1900 -> -2600 0 1500 (9 of 27 fit)
10:02:40.000 randomizer: ghosts: room 5:13 (#2): 1 of 2 victim(s) moved (seed 123456, Change Each Visit On): FFFF0006 0B v1 -1200 0 -1100 -> -700 0 -300 (11 of 30 fit); FFFF0001 0B v2 -3400 0 1900 kept (no spot fits, 0 of 25)
10:02:50.000 ANOMALY: randomizer: ghosts: room 5:16: 1 victim(s) whose position words did not hold the stock value (another mod?): left as found
10:03:00.000 randomizer: Ghosts Off (was Spots; applies at the next room load)
10:03:01.000 randomizer: [Randomizer] EileenOutfit read again: Off (applies at the next room load)
10:03:02.000 randomizer: outfit: back to the set outfit Default (Eileen Outfit Off)
10:09:00.000 randomizer: looks session: Eileen Outfit Off: 3 world visit(s), 1 draw(s) (Dress 0, Nurse 1), 1 with only the Dress unlocked, 2 back to the set outfit, 1 chosen in the Extra Menu; Ghosts Off: 2 room load(s) with victims, 4 victim(s), 3 moved, 1 kept, 1 left as found
"""
V349RZLB = V333_END + """10:00:05.000 randomizer: looks: the first room load (stage 2 room 1): Eileen Outfit Random, NOT available: the outfit engine is off ([EileenOutfit] Enabled 0, or its sites are not stock); Ghosts Spots, the cheat table's Enemy Randomizer is on: ghosts left alone
10:00:05.001 randomizer: ghosts: the cheat table's Enemy Randomizer (0052CAA9 / 0052CB2B) is on: the ghosts start where the game puts them
"""
# v3.50 RL350: the outfit's room draws, the final fight's hold, the ghosts' distance and record (rzlooks.inc's v3.50 formats)
V350RL = V333_END + """10:00:00.041 randomizer: looks: [Randomizer] EileenOutfit Random, Ghosts Spots (companion Eileen's outfit among every outfit, Dress and Nurse: at every room load of her world with Change Each Visit On, else once per world visit; victim ghosts at another spot of their room, 800 + away when it allows, their records in the room moved too: patch-and-restore of their position words, no exe patch)
10:00:04.000 randomizer: outfit: world visit 2 (stage 5, half 2, at her world's file list): Nurse (seed 1211037167, Change Each Visit On; every outfit: Dress, Nurse; the set outfit Nurse)
10:00:06.000 randomizer: outfit: room 5:14 load (world visit 2, room draw 1): Nurse -> Dress (seed 1211037167)
10:00:30.000 randomizer: outfit: room 5:15 load (world visit 2, room draw 2): Dress -> Dress (seed 1211037167)
10:01:00.000 randomizer: outfit: room 9:1 load: no room draw in the final fight (stage 9): her outfit stays Nurse for this visit
10:02:00.000 randomizer: ghosts: room 2:7 (#2): 2 of 2 victim(s) moved (seed 1211037167, Change Each Visit On): FFFF000A 0B v0 -1000 900 -4400 -> 210 900 -3550 (d 1478, 9 of 31 fit, record moved); FFFF0007 0B v3 982 900 -4395 -> 1600 900 -3900 (d 792 near: the room allows no farther, 4 of 30 fit, record none)
10:03:00.000 randomizer: ghosts: room 5:4 (#1): 0 of 1 victim(s) moved (seed 1211037167, Change Each Visit On): FFFF0009 0D v1 2200 -800 -3432 kept (its record: pinned here)
10:09:00.000 randomizer: looks session: Eileen Outfit Random: 2 world visit(s), 1 world draw(s) (all draws: Dress 1, Nurse 2), 2 room draw(s) (1 changed her outfit), 1 back to the set outfit, 0 chosen in the Extra Menu; Ghosts Spots: 2 room load(s) with victims, 3 victim(s), 2 moved (1 of them 800 or more, 1 record(s) moved), 1 kept (1 by a pinned or down record), 0 left as found
"""
RG_WHO_PE = "Playable Eileen's own regeneration (Refined's row grey, Off)"
V347R = V333_END + """10:00:00.050 settings: [Gameplay] Regen was missing, added as "x1" (from Playable Eileen's [Defense] Regen 1 / RegenRate 15, as the v3.46 row showed it)
10:00:00.051 settings: [Gameplay] RegenDelay was missing, added as "10" (from Playable Eileen's [Defense] RegenDelay 10)
10:00:00.100 regeneration: Playable Eileen loaded with SH4PE_RegenActive (v1.23.2+): Refined heals Henry (SH4PE_Enabled 0) and Eileen while her own regeneration is off; while hers is in charge Refined's row is grey Off
10:00:00.101 """ + RG_INSTALL % ('in', 'x1', '15', '10', RG_WHO_PE) + """
10:00:01.000 regeneration: who heals: """ + RG_WHO_PE + """ (Refined's Regen x1, RegenDelay 10 s)
10:01:00.000 regeneration: who heals: SH4 Refined (Henry) (Refined's Regen x1, RegenDelay 10 s)
10:01:00.001 [v] regeneration: heals Henry from 120.0 / 200 HP (x1: 0.500 HP a second)
10:01:20.000 [v] regeneration: stops: hurt (-12.3 HP; 130.0 / 200 HP, +10.0 HP in 20.0 s): no healing for 10 s
10:01:20.500 life steal: heals go to 0052D1D0 directly (Playable Eileen's regeneration is not in charge)
10:01:30.000 [v] regeneration: heals Henry from 117.7 / 200 HP (x1: 0.500 HP a second)
10:01:40.000 [v] regeneration: stops: near a ghost aura (122.7 / 200 HP, +5.0 HP in 10.0 s)
10:01:50.000 [v] regeneration: heals Henry from 122.7 / 200 HP (x1: 0.500 HP a second)
10:04:35.000 [v] regeneration: stops: full (200.0 / 200 HP, +77.3 HP in 164.6 s)
10:05:00.000 regeneration: Regen x2 (was x1; Options)
10:05:10.000 regeneration: [Gameplay] RegenDelay read again: 20 s (was 10 s)
10:05:20.000 regeneration: [Gameplay] Regen read again: Off (was x2)
10:06:00.000 regeneration: who heals: nobody (Regeneration Off) (Refined's Regen Off, RegenDelay 20 s)
10:06:01.000 ANOMALY: regeneration: 0052D1D0 did not raise HP (50.000 -> 50.000 of 200, +0.0083 asked): another mod on PlayerHealAmount?
10:07:00.000 regeneration: who heals: SH4 Refined (Eileen: Playable Eileen's own regeneration is off) (Refined's Regen x1, RegenDelay 20 s)
10:07:00.001 [v] regeneration: heals Eileen from 90.0 / 200 HP (x1: 0.500 HP a second)
"""
V347RN = V333_END + """10:00:00.100 regeneration: 0052D1D0 (PlayerHealAmount) is not the stock code (another mod?): Refined's regeneration does nothing
10:00:00.101 """ + RG_INSTALL % ('OFF', 'x2', '30', '30', "nobody (Refined's regeneration is not installed)") + """
10:00:00.102 regeneration: Playable Eileen loaded without SH4PE_RegenActive (before v1.23.2): the Regeneration row stays its ini front end ([Defense] Regen / RegenRate in SH4PlayableEileen.ini) and Refined does not heal
10:00:05.000 regeneration: faulted (code C0000005): Refined's regeneration is off for this session (0.0 HP healed)
"""


# v3.40 SH340: Shadow Edge's lines (shadows.inc)
V340S = V333_END + """10:00:00.100 shadows: hooks in place (the composite's draw 0055FE43, the device reset 00402E0D); Edge Soft; Soft 3.0 px / Softer 6.0 px at 720 lines, scaled with the height; preview on (the page backdrop 0x526A53)
10:00:05.000 [v] shadows: Soft at 2560x1440: 1 pass(es) down to 1280x720, composite taps 0.85 texels: a 6.0 px edge (asked 6.0; stock's ramp 3.2 px at every size); 3.5 MB of render targets
10:00:05.001 [v] shadows: render target made: half size 1280x720 (3.5 MB; ours now 3.5 MB)
10:02:00.000 [v] shadows: room 1:02: 3600 composites over 3590 frames (at most 2 in a frame), 3600 soft; ours 0.041 ms a composite on average, 0.350 at most; edge Soft; render targets 3.5 MB
10:02:10.000 [v] shadows: the paused scene kept for the Shadows page: 2560x1440 picture + mask (28.1 MB), darkness 4B000000 (frame 7000)
10:02:12.000 [v] shadows: the Shadows page's preview: the paused scene, split Stock | the chosen edge (the paused scene)
10:02:14.000 [v] shadows: Edge Softer
10:02:14.001 [v] shadows: Softer at 2560x1440: 2 pass(es) down to 640x360, composite taps 0.82 texels: a 12.0 px edge (asked 12.0; stock's ramp 3.2 px at every size); 4.4 MB of render targets
10:04:00.000 [v] shadows: room 1:03: 1200 composites over 1200 frames (at most 1 in a frame), 1200 soft; ours 0.060 ms a composite on average, 0.900 at most; edge Softer; render targets 4.4 MB
10:05:00.000 shadows: off for this session: a render target could not be set (hr 80004005, the soft composite): the game's own shadow edge from now on
"""

# v3.49 SH349: the coverage line and the Tremers' wall shadows at the Darkness (hook 0x45F4CA); then the switch off
V349S = V333_END + """10:00:00.100 shadows: Darkness 85 % (of the game's inside darkness, after Playable Eileen's ShadowDarkness; Stock and Soft)
10:00:00.101 shadows: hooks in place (the composite's draw 0055FE43, the device reset 00402E0D); Edge Soft; Soft 12.0 px at 720 lines, scaled with the height (down, blur and up in 2x steps); preview on (the page backdrop 0x526A53)
10:00:00.102 shadows: the edge and the Darkness cover every shadow the game draws: the real-time shadows of every caster in one stencil mask a composite (the player and the weapon in hand, Eileen, Cynthia, enemies, ghosts, Room 302's hauntings, the boss room's spears, event and cut-scene figures), Edge and Darkness; the Tremers' wall shadows: the Darkness (hook 0045F4CA; their edge is their texture's); the shadows baked into the room textures (furniture, doors, items, Room 302) are not drawn as shadows (left as painted)
10:01:00.000 [v] shadows: a Tremer's wall shadow drawn at Darkness 85 %: its alpha 30 -> 29 (85 % of the game's darkening; subtractive ribbon 0x45F1E0, its edge is its texture's)
10:01:05.000 [v] shadows: a Tremer's wall shadow drawn at Darkness 85 %: its alpha 1A -> 16 (85 % of the game's darkening; subtractive ribbon 0x45F1E0, its edge is its texture's)
10:02:00.000 [v] shadows: room 4:0C: 640 Tremer wall shadow draw(s), 600 at the Darkness (85 %; alpha 08..30 asked)
10:02:00.001 [v] shadows: room 4:0C: 1200 composites over 1200 frames (at most 1 in a frame), 1200 soft; ours 0.050 ms a composite on average, 0.300 at most; edge Soft; render targets 4.8 MB; darkness 85 %
"""
# v3.52 SH352: the room line's tail (hard with reasons, no darkness), the once-a-room reason lines; an old 4:1F line
V352S = V333_END + """10:00:00.101 shadows: hooks in place (the composite's draw 0055FE43, the device reset 00402E0D); Edge Soft; Soft 12.0 px at 720 lines, scaled with the height (down, blur and up in 2x steps); preview on (the page backdrop 0x526A53)
10:00:00.102 shadows: the edge and the Darkness cover every shadow the game draws: the real-time shadows of every caster in one stencil mask a composite (the player and the weapon in hand, Eileen, Cynthia, enemies, ghosts, Room 302's hauntings, the boss room's spears, event and cut-scene figures), Edge and Darkness; the Tremers' wall shadows: the Darkness (hook 0045F4CA; their edge is their texture's); the shadows baked into the room textures (furniture, doors, items, Room 302) are not drawn as shadows (left as painted); where the game's darkness is 0 (stage 4 room 0x1F, cut-scene 0x43) no real-time shadow shows, with any edge: nothing to soften there
10:01:00.000 [v] shadows: room 4:22: 591 composites over 591 frames (at most 1 in a frame), 591 soft; ours 0.049 ms a composite on average, 0.086 at most; edge Soft; render targets 47.0 MB; darkness 100 %
10:01:00.010 [v] shadows: room 4:1F: a composite not soft: the game's darkness is 0 here (TEXTUREFACTOR alpha 0: the mask is empty, the game's own draw leaves the picture as it is: no shadow shows, so there is nothing to soften) (darkness 00000000, edge Soft, Darkness 100 %; once a room)
10:02:00.000 [v] shadows: room 4:1F: 2769 composites over 2769 frames (at most 1 in a frame), 0 soft; ours 0.004 ms a composite on average, 0.120 at most; edge Soft; render targets 47.0 MB; darkness 100 %; not soft: 0 hard, 2769 with no darkness (the game's own 0: nothing drawn)
10:02:01.000 shadows: our render targets could not be made (1 in a row): the stock edge this time, tried again
10:02:01.000 shadows: room 4:22: a composite not soft: our render targets could not be made this time: the game's own hard edge, soft again at the next composite (darkness 50000000, edge Soft, Darkness 100 %; once a room)
10:03:00.000 [v] shadows: room 4:22: 1000 composites over 1000 frames (at most 1 in a frame), 997 soft; ours 0.050 ms a composite on average, 0.300 at most; edge Soft; render targets 47.0 MB; darkness 100 %; not soft: 3 hard (render targets 1, failed 2), 0 with no darkness (the game's own 0: nothing drawn)
10:04:00.000 [v] shadows: room 4:1F: 1138 composites over 1138 frames (at most 1 in a frame), 0 soft; ours 0.004 ms a composite on average, 0.098 at most; edge Soft; render targets 47.0 MB; darkness 100 %
10:05:00.000 [v] shadows: room 2:01: 400 composites over 400 frames (at most 1 in a frame), 0 soft; ours 0.000 ms a composite on average, 0.000 at most; edge Stock; render targets 0.0 MB; darkness 85 %; not soft: 400 hard (Stock 400), 0 with no darkness (the game's own 0: nothing drawn)
"""
V349SOFF = V333_END + """10:00:00.101 shadows: hooks in place (the composite's draw 0055FE43, the device reset 00402E0D); Edge Stock; Soft 12.0 px at 720 lines, scaled with the height (down, blur and up in 2x steps); preview on (the page backdrop 0x526A53)
10:00:00.102 shadows: [Shadows] TremerShadows=0: the Tremers' wall shadows stay the game's own (the Darkness does not apply to them)
10:00:00.103 shadows: the edge and the Darkness cover every shadow the game draws: the real-time shadows of every caster in one stencil mask a composite (the player and the weapon in hand, Eileen, Cynthia, enemies, ghosts, Room 302's hauntings, the boss room's spears, event and cut-scene figures), Edge and Darkness; the Tremers' wall shadows: the game's own (no hook); the shadows baked into the room textures (furniture, doors, items, Room 302) are not drawn as shadows (left as painted)
"""



V344G = V333_END + """10:00:00.200 ghost teleport: in (no exe patch; 24 game functions checked), [Gameplay] GhostTeleport On; Run + Action together (within 80 ms): behind the nearest standing enemy within 2500 u, else 800 u ahead the way she faces, cooldown 3.0 s; Richard's transition: his warp sound 0xEC86 on, his see-through fade (0.5 s out, 0.5 s in) on her model in
10:01:00.000 [v] ghost teleport: Run + Action (Action last: its own use taken back): Richard's fade out (0.5 s), then behind type 03 640 u away
10:01:00.500 [v] ghost teleport: behind type 03 640 u away (enemy 1 of 1 tried, spot 0: 155 u behind its back, floor +0 u of hers): from (100, 200) to (700, 900), facing it; cooldown 3.0 s (warp 1; same room: 2 door line(s), box in)
10:01:01.000 [v] ghost teleport: Richard's fade ended (drawn see-through on 30 body and 30 weapon draw(s))
10:01:02.000 [v] ghost teleport: Run + Action during the cooldown (1.5 s left): no warp
10:01:10.000 [v] ghost teleport: Run + Action (Action first: its press reached the game): Richard's fade out (0.5 s), then 800 u ahead the way she faces
10:01:10.500 [v] ghost teleport: ahead 800 u the way she faces (no standing enemy within 2500 u; try 1 of 7, floor +0 u of hers): from (700, 900) to (700, 1700) (the fade began at (700, 880)); cooldown 3.0 s (warp 2; same room: 2 door line(s), box in)
10:01:11.000 [v] ghost teleport: Richard's fade ended (drawn see-through on 0 body and 0 weapon draw(s))
10:01:20.000 [v] ghost teleport: Run + Action (Action last: its own use taken back): Richard's fade out (0.5 s), then 600 u ahead the way she faces
10:01:20.200 [v] ghost teleport: called off during the fade: she is hit or held (hit phase 1, grab 0) (she fades back in)
10:01:20.400 [v] ghost teleport: Richard's fade ended (drawn see-through on 12 body and 0 weapon draw(s))
10:01:30.000 [v] ghost teleport: not now: no free floor ahead of her in this room (no standing enemy within 2500 u; 7 tries from 800 down to 200 u, 2 door line(s): a wall between 7)
10:01:40.000 [v] ghost teleport: not now: Room 302
"""

# v3.49 GT349: v3.48's chord lines (with "; a later call-off keeps it"), a hold, the room's event zones, a refusal for one
V349G = V333_END + """10:00:00.200 ghost teleport: in (no exe patch; 29 game functions checked), [Gameplay] GhostTeleport On; Run + Action together (within 80 ms): behind the nearest standing enemy within 2500 u, else 800 u ahead the way she faces, cooldown 0.0 s; held: the next warp once she has faded in; never across or into a walk-in event zone; Richard's transition: his warp sound 0xEC86 on, his see-through fade (0.5 s out, 0.5 s in) on her model in
10:01:00.000 [v] ghost teleport: room 2:2: 1 walk-in event zone(s) the game would still fire: no warp across or into them (key 00A box at (10, -2147) 1050 x 447) (0 other zone(s) need a press or a look)
10:01:00.001 [v] ghost teleport: Run + Action (Action last: its own use taken back; a later call-off keeps it): Richard's fade out (0.5 s), then 800 u ahead the way she faces
10:01:00.500 [v] ghost teleport: ahead 800 u the way she faces (no standing enemy within 2500 u; try 1 of 7, floor +0 u of hers): from (-31, -6024) to (46, -5228) (the fade began at (-57, -6322)); cooldown 0.0 s (warp 1; same room: 6 door line(s), box in)
10:01:01.000 [v] ghost teleport: Richard's fade ended (drawn see-through on 30 body and 30 weapon draw(s))
10:01:01.020 [v] ghost teleport: Run + Action (held: the next warp; a later call-off keeps it): Richard's fade out (0.5 s), then 800 u ahead the way she faces
10:01:01.520 [v] ghost teleport: ahead 800 u the way she faces (no standing enemy within 2500 u; try 1 of 7, floor +0 u of hers): from (46, -5228) to (46, -4428) (the fade began at (46, -5228)); cooldown 0.0 s (warp 2; same room: 6 door line(s), box in)
10:01:02.020 [v] ghost teleport: Richard's fade ended (drawn see-through on 30 body and 30 weapon draw(s))
10:01:02.040 [v] ghost teleport: not now: no free floor behind the nearest 3 enemy(ies) in this room (type 0A 1366 u first; 63 spots, 6 door line(s): an event's trigger zone on the way 63) (Action left to the game)
10:01:03.000 [v] ghost teleport: Run + Action let go after 2 warp(s) in a row (3 try(ies) refused or called off while held)
10:01:04.000 [v] ghost teleport: not now: a door is in use
10:01:05.000 [v] ghost teleport: not now: the screen is fading (a door or a scene; level 128, to 0)
10:01:05.500 [v] ghost teleport: not now: at the end of the fade-out: inside an event's trigger zone (key 00A, not fired yet): no warp out of it (she fades back in where she is)
10:01:06.000 [v] ghost teleport: room 2:3: 0 walk-in event zone(s) the game would still fire: no warp across or into them (5 other zone(s) need a press or a look)
"""

# v3.50 TP350: through a wall, behind the enemy she faces, the room's floor map, a room visit's counts, the new refusals
V350G = V333_END + """10:00:00.200 ghost teleport: in (no exe patch; 34 game functions checked), [Gameplay] GhostTeleport On; Run + Action together (within 80 ms): behind the standing enemy she faces (+-35 degrees) within 5000 u, else up to 800 u ahead the way she faces, through walls on (onto floor a route of the room reaches), cooldown 0.0 s; held: the next warp once she has faded in; never across or into a walk-in event zone; Richard's transition: his warp sound 0xEC86 on, his see-through fade (0.5 s out, 0.5 s in) on her model in
10:01:00.000 [v] ghost teleport: room 2:4: 0 walk-in event zone(s) the game would still fire: no warp across or into them (18 other zone(s) need a press or a look)
10:01:00.001 [v] ghost teleport: room 2:4: the floor map begins (a grid of 40 x 120 cells of 60 u on the room's box; 7 of 7 door(s) seeded, 0 without a floor there)
10:01:00.004 [v] ghost teleport: Run + Action (Action last: its own use taken back; a later call-off keeps it): Richard's fade out (0.5 s), then 440 u ahead the way she faces, through a wall (the room's floor map is not finished: the warp decides)
10:01:00.300 [v] ghost teleport: room 2:4: the floor map is finished: 4120 cell(s) a route of the room reaches, from 8 seed(s) (0 without a floor); 9.4 ms over 3 run(s)
10:01:00.500 [v] ghost teleport: through 440 u the way she faces (straight ahead; the obstacle from 320 u, 2 spot(s) past it refused first; on the room's floor map, floor -0 u of hers): from (0, 0) to (0, 440) (the fade began at (0, 0)); cooldown 0.0 s (warp 1; same room: 7 door line(s), box in)
10:01:01.000 [v] ghost teleport: Richard's fade ended (drawn see-through on 30 body and 30 weapon draw(s))
10:01:02.000 [v] ghost teleport: Run + Action (Action last: its own use taken back; a later call-off keeps it): Richard's fade out (0.5 s), then behind type 0A 3100 u away (the enemy she faces, 12 degrees off)
10:01:02.500 [v] ghost teleport: behind type 0A 3100 u away (the enemy she faces, 12 degrees off her facing, 1 in the +-35 degree cone; spot 0: 160 u behind its back, floor +0 u of hers): from (0, 440) to (100, 3500), facing it; cooldown 0.0 s (warp 2; same room: 7 door line(s), box in)
10:01:04.000 [v] ghost teleport: not now: no free floor behind the enemy she faces (type 0A 900 u, 20 degrees off her facing; 21 spots, 7 door line(s): a wall between 21) (Action left to the game)
10:01:05.000 [v] ghost teleport: not now: no free floor ahead of her in this room (no standing enemy she faces within 5000 u; 7 tries from 800 down to 200 u, 7 door line(s): a wall between 7; through walls: 5 of 5 ray(s) met an obstacle within 800 u, past it: no route of the room reaches it 25, in a wall 15; the room's floor map 4120 cell(s)) (Action left to the game)
10:01:06.000 [v] ghost teleport: room 2:4 left: 2 warp(s) (1 through walls, 1 behind the enemy she faces, 0 ahead), 2 refused (behind the faced enemy 1, ahead 1); 1 Action / Check press(es) taken back in fade-outs; floor map 4120 cell(s), finished, 9.4 ms over 3 run(s)
10:01:07.000 [v] ghost teleport: room 2:7 left: 0 warp(s) (0 through walls, 0 behind the enemy she faces, 0 ahead), 1 refused (her state 1); 0 Action / Check press(es) taken back in fade-outs; floor map 300 cell(s), not finished, 1.2 ms over 1 run(s)
"""

# v3.51 TP351: the enemy she faces behind collision is skipped (the warp lines say so; the room visit's line counts them)
V351G = V333_END + """10:00:00.200 ghost teleport: in (no exe patch; 34 game functions checked), [Gameplay] GhostTeleport On; Run + Action together (within 80 ms): behind the standing enemy she faces (+-35 degrees) within 5000 u in the open (none past collision), else up to 800 u ahead the way she faces, through walls on (onto floor a route of the room reaches), cooldown 0.0 s; held: the next warp once she has faded in; never across or into a walk-in event zone; Richard's transition: his warp sound 0xEC86 on, his see-through fade (0.5 s out, 0.5 s in) on her model in
10:01:00.500 [v] ghost teleport: through 440 u the way she faces (straight ahead; the obstacle from 320 u, 2 spot(s) past it refused first; on the room's floor map, floor -0 u of hers): from (0, 0) to (0, 440) (the fade began at (0, 0)); cooldown 0.0 s (warp 1; same room: 7 door line(s), box in; 1 enemy(ies) in the cone skipped: behind collision (the first: a wall between (her chest line); type 0B 900 u, 3 degrees off))
10:01:02.500 [v] ghost teleport: behind type 0A 1200 u away (the enemy she faces, 15 degrees off her facing, 1 in the +-35 degree cone; spot 0: 160 u behind its back, floor +0 u of hers): from (0, 440) to (300, 1600), facing it; cooldown 0.0 s (warp 2; same room: 7 door line(s), box in; 2 enemy(ies) in the cone skipped: behind collision (the first: a low barrier between (a turnstile, a counter, a fence); type 03 700 u, 1 degrees off))
10:01:04.000 [v] ghost teleport: not now: no free floor ahead of her in this room (no standing enemy she faces within 5000 u; 1 enemy(ies) in the cone skipped: behind collision (the first: a ghost in its wall (its portal open); type 0B 600 u, 8 degrees off); 7 tries from 800 down to 200 u, 7 door line(s): a wall between 7; through walls: 5 of 5 ray(s) met an obstacle within 800 u, past it: no route of the room reaches it 25; the room's floor map 4120 cell(s)) (Action left to the game)
10:01:06.000 [v] ghost teleport: room 2:4 left: 2 warp(s) (1 through walls, 1 behind the enemy she faces, 0 ahead), 1 refused (ahead 1); 0 Action / Check press(es) taken back in fade-outs; 4 enemy(ies) in the cone skipped behind collision in 3 try(ies); floor map 4120 cell(s), finished, 9.4 ms over 3 run(s)
10:01:07.000 [v] ghost teleport: room 2:7 left: 0 warp(s) (0 through walls, 0 behind the enemy she faces, 0 ahead), 1 refused (her state 1); 0 Action / Check press(es) taken back in fade-outs; 0 enemy(ies) in the cone skipped behind collision in 0 try(ies); floor map not built
"""

# v3.52 TP352: every landing proven; freed from a wall (after a warp, at a warp from inside one, left inside one)
V352G = V333_END + """10:00:00.200 ghost teleport: in (no exe patch; 34 game functions checked), [Gameplay] GhostTeleport On; Run + Action together (within 80 ms): behind the standing enemy she faces (+-35 degrees) within 5000 u in the open (none past collision), else up to 800 u ahead the way she faces, through walls on (onto floor a route of the room reaches), cooldown 0.0 s; every landing proven (her body free, on the room's floor map); inside a wall after a warp: moved to the nearest proven floor; held: the next warp once she has faded in; never across or into a walk-in event zone; Richard's transition: his warp sound 0xEC86 on, his see-through fade (0.5 s out, 0.5 s in) on her model in
10:01:00.000 [v] ghost teleport: room 4:17: the floor map begins (a grid of 69 x 69 cells of 60 u on the room's box; 9 of 9 door(s) seeded, 0 without a floor there, 1 inside collision)
10:01:00.010 [v] ghost teleport: room 4:17: the floor map is finished: 1624 cell(s) a route of the room reaches, from 9 seed(s) (0 without a floor); 12.6 ms over 1 run(s); 1 seed(s) inside collision refused
10:01:00.500 [v] ghost teleport: behind type 09 343 u away (the enemy she faces, 1 degrees off her facing, 2 in the +-35 degree cone; spot 4: 160 u on its far side (no usable facing), floor +0 u of hers, proven: her body free, on the room's floor map): from (-1752, 788) to (-1542, 1181), facing it; cooldown 0.0 s (warp 1; same room: 9 door line(s), box in)
10:01:02.000 [v] ghost teleport: not now: no free floor behind the enemy she faces (type 09 343 u, 1 degrees off her facing; 21 spots, 9 door line(s): a wall between 2, no room for her body 19) (Action left to the game)
10:01:03.500 [v] ghost teleport: ahead 700 u the way she faces (no standing enemy she faces within 5000 u; try 2 of 7, floor +0 u of hers, proven: her body free, on the room's floor map): from (0, 0) to (0, 700) (the fade began at (0, 0)); cooldown 0.0 s (warp 2; same room: 9 door line(s), box in)
10:01:03.700 ghost teleport: freed from a wall: after warp 2 (ahead) her body stood inside collision at (0, 700) for 0.15 s: moved 62 u to the nearest proven floor cell (0, 638), floor +0 u of hers (9 cell(s) tried)
10:01:05.500 ghost teleport: freed from a wall: at the warp she stood inside collision at (-1600, 1320) and no other landing was proven: to the nearest proven floor cell (-1620, 1194), 128 u away, floor +0 u of hers (warp 3)
10:01:07.700 [v] ghost teleport: inside collision after warp 4 at (100, 200): no proven floor cell within 480 u (12 tried; floor map 900 cell(s)): not moved (Ghost Teleport or No Clip moves her)
10:01:08.000 [v] ghost teleport: room 4:17 left: 2 warp(s) (0 through walls, 1 behind the enemy she faces, 1 ahead), 1 refused (behind the faced enemy 1); 0 Action / Check press(es) taken back in fade-outs; 0 enemy(ies) in the cone skipped behind collision in 0 try(ies); 2 time(s) freed from a wall; floor map 1624 cell(s), finished, 12.6 ms over 1 run(s)
"""

V343G = V333_END + """10:00:00.200 ghost teleport: in (no exe patch; 13 game functions checked), [Gameplay] GhostTeleport On; hold Run 0.4 s standing still: behind the nearest standing enemy within 2500 u, cooldown 3.0 s, Richard's warp sound 0xEC86 on, Echo Doubles' teleport smoke
10:01:00.000 [v] ghost teleport: behind type 03 640 u away (enemy 1 of 2 tried, spot 0: 155 u behind its back, floor +0 u of hers): from (100, 200) to (700, 900), facing it; cooldown 3.0 s (warp 1)
10:01:01.000 [v] ghost teleport: Run held, waiting for the cooldown (2.0 s left)
10:01:04.000 [v] ghost teleport: behind type 0B 900 u away (enemy 2 of 2 tried, spot 4: 180 u behind its back, floor -10 u of hers): from (700, 900) to (0, 0), facing it; cooldown 3.0 s (warp 2)
10:01:10.000 [v] ghost teleport: not now: no standing enemy within 2500 u (0 enemies in the room)
10:01:20.000 [v] ghost teleport: not now: Room 302
10:01:30.000 [v] ghost teleport: not now: no standing enemy within 2500 u (2 enemies in the room)
"""

# v3.40 LG340: a game folder with logs\ (logrotate.inc): four sessions on three renderers, one a crash, plus the copies
# SH4Refined.prev.log = its rotated twin and a file that is not Refined's
# v3.43 FN343: the HD menu font's lines (font.inc)
V343F = V333_END + r'''10:00:00.100 [v] settings: [Textures] MenuFont EB Garamond: fonts\EBGaramond.sh4font (EB Garamond, 4x; fonts 0 / 1: 212 / 212 glyphs), hook 005567F0
10:00:05.000 font: EB Garamond (fonts\EBGaramond.sh4font): the glyph page is now 2048 x 2048 (4x, MANAGED A8R8G8B8, 16 MB); the stock page 0A1B2C3D stays alive
10:00:05.000 [v] font: font 0 (struct 05F10020) matches EB Garamond's metrics (crc 1A2B3C4D): its Latin glyphs draw from the sheet
10:00:05.000 font: font 1 (struct 05F40380, metrics crc 00000001) is not the font EB Garamond was made for (crc 5E6F7A8B): its glyphs stay the stock ones (upscaled)
'''
V343FS = V333_END + r'''10:00:00.100 font: [Textures] MenuFont Fancy: fonts\Fancy.sh4font not found (error 2): the stock font
'''
# v3.44 FN344: Stock at the start, then the Menu Font row (Options > Option 1) switched live twice
V344FN = V333_END + r'''10:00:00.100 settings: [Textures] MenuFont Stock: the stock font
10:00:00.200 [v] font: Options > Option 1 has a Menu Font row (row 7 of 8, above Exit; 4 values; it switches live): Stock
10:01:00.000 font: Menu Font -> EB Garamond (Options, live): fonts\EBGaramond.sh4font (EB Garamond, 4x; fonts 0 / 1: 212 / 212 glyphs); every glyph is drawn again at the next frame with text (the 2048 x 2048 page is made then)
10:01:00.050 font: EB Garamond (fonts\EBGaramond.sh4font): the glyph page is now 2048 x 2048 (4x, MANAGED A8R8G8B8, 16 MB); the stock page 0A1B2C3D stays alive
10:01:00.050 [v] font: font 0 (struct 05F10020) matches EB Garamond's metrics (crc 1A2B3C4D): its Latin glyphs draw from the sheet
10:01:05.000 font: Menu Font -> Crimson Text (Options, live): fonts\CrimsonText.sh4font (Crimson Text, 4x; fonts 0 / 1: 211 / 211 glyphs); every glyph is drawn again at the next frame with text
'''

# v3.47 SB347: the save list's Bank row: its first draw and mouse clicks on the arrows (savebanks.inc)
V347SB = V333_END + r'''10:00:00.100 [v] save banks: the Bank row draws "Bank 2" in the list's own font (Menu Font EB Garamond), 58 units wide, centred at x 256; arrows at x 158..178 / 334..354 click to the previous / next bank
10:00:01.000 save banks: bank 1
10:00:01.000 [v] save banks: mouse click on the left arrow: bank 2 -> 1
10:00:01.500 [v] save banks: mouse click on the left arrow: bank 1 -> 1
10:00:02.000 save banks: bank 2
10:00:02.000 [v] save banks: mouse click on the right arrow: bank 1 -> 2
'''
# v3.48 SB348: the per-step lines verbose, one normal line per settled change (savebanks.inc)
V348SB = V333_END + r'''10:00:00.050 save banks: load list on bank 3
10:00:00.100 [v] save banks: the Bank row draws "Bank 3" in the list's own font (Menu Font EB Garamond), 55 units wide, centred at x 256; arrows at x 158..178 / 334..354 click to the previous / next bank
10:00:01.100 [v] save banks: bank 13
10:00:01.600 [v] save banks: bank 23
10:00:02.000 [v] save banks: bank 24
10:00:02.000 [v] save banks: mouse click on the right arrow: bank 23 -> 24
10:00:02.500 [v] save banks: bank 25
10:00:03.500 save banks: bank 3 -> 25 in the load list (4 steps: keys 1, pad 1, mouse 2; by 10: 2)
10:00:09.000 save banks: bank 25 -> 24 in the load list (1 step: keys 1, pad 0, mouse 0; by 10: 0)
10:01:00.000 save banks: bank 7
'''
# v3.49 SB349: the lists' highlight bar (savebanks.inc SbListBarInstall)
V349SB = V333_END + r'''10:00:00.010 save banks: list highlight bar: blue (the game's blue menu bar, 56FCC0's colour; 56FDCF)
10:00:00.050 save banks: load list on bank 3
'''
V349SBY = V333_END + r'''10:00:00.010 save banks: list highlight bar: yellow (the game's own list bar; [SaveBanks] ListBar=Yellow)
'''
V349SBX = V333_END + r'''10:00:00.010 save banks: 0056FDCF is not stock: expected 81 CE 00 00 80 FF C1 E6 08 0B F0 
10:00:00.010 save banks: list highlight bar: yellow (56FDCF is not stock: nothing written)
'''


# v3.50 FI350: Fade In (fadein.inc): the install line, verbose starts, a portal drawn, two room lines, a menu change, an
# ini re-read, a type with a draw state of its own
FI_INSTALL = ("fade in: in (no exe patch: the model draw 0052AEB5 and shadow 0052AF95 hooks; the game's see-through enemy draw "
              "0045C880; the portal %s); [Gameplay] FadeIn %s, FadeInSeconds 0.80; never bosses, Wall Men, Walter, event or "
              "cut-scene figures; a fading enemy acts as usual")
V350FI = V333_END + """10:00:00.300 """ + FI_INSTALL % ('from the scene pass 004EFBFD', 'Fade + Portal') + """
10:00:05.000 [v] fade in: room load 02040001 (type 0A) fades in over 0.80 s at -79 -150 -1597 (room 2:4, 0.0 s into the visit)
10:00:09.000 [v] fade in: follower FFFF0007 (type 0B) fades in over 0.80 s from a blood portal at 10 0 -200 (room 2:2, 4.1 s into the visit)
10:00:09.020 fade in: a blood portal drawn (viewport 3D 2560x1440, the exact depth of 012E3ED0)
10:00:20.000 fade in: room 2:2: 4 fade(s) (room load 2, follower 1, wanderer 0, copy 1, revive 0, ghost 0, monster 0), 1 portal(s); 2 not faded (stage 0, cut-scene 0, type 1, boss 0, event 0, dead 1, own draw 0, own portal 0, own fade 0)
10:00:30.000 fade in: FadeIn Fade
10:00:40.000 fade in: room 2:4: 1 fade(s) (room load 0, follower 0, wanderer 0, copy 0, revive 1, ghost 0, monster 0), 0 portal(s); 1 not faded (stage 0, cut-scene 0, type 0, boss 0, event 0, dead 0, own draw 0, own portal 1, own fade 0)
10:00:50.000 fade in: [Gameplay] FadeIn read again: Fade + Portal
10:00:51.000 [v] fade in: type 0A is drawn with a draw state of its own (pre-draw 0045C840): drawn as the game draws it
"""
# v3.50 FI350: an unknown ini value and Fade In off (0x45C880 not stock: a warning too)
V350FI2 = V333_END + """10:00:00.290 fade in: [Gameplay] FadeIn "Dissolve" is not Off / Fade / Fade + Portal: Fade + Portal (the default)
10:00:00.300 fade in: OFF (0045C880, the game's see-through enemy draw, is not the stock code: another mod?); enemies appear as the game makes them
"""
# v3.50 FI350: a portal that was never drawn, a fault, a menu value that could not be saved
V350FI3 = V333_END + """10:00:00.300 """ + FI_INSTALL % ('from the scene pass 004EFBFD', 'Fade + Portal') + """
10:00:40.000 fade in: room 3:1: 2 fade(s) (room load 0, follower 0, wanderer 0, copy 0, revive 0, ghost 1, monster 1), 1 portal(s); 0 not faded (stage 0, cut-scene 0, type 0, boss 0, event 0, dead 0, own draw 0, own portal 0, own fade 0)
10:01:00.000 fade in: a fault in the model draw (code C0000005 at 0045C8F2): Fade In is off for this session (enemies are drawn as the game draws them)
10:01:00.001 ANOMALY: fade in: a fault in the model draw (code C0000005 at 0045C8F2): off for the session
10:01:05.000 fade in: FadeIn Off applies now but could not be saved in SH4Refined.ini (error 5)
"""

# v3.51 MU351: the sound in the Extra Menu's lists (extramenu.inc MuLogSound): the install line, the load list cancelled and
# then used, the save menu, a chest whose music was not paused (+ its ANOMALY line)
MU_S = 'extra menu: sound in the '
V351MU = V333_END + """10:00:00.010 extra menu: sound in the Storage Chest / Save Game / Load Game: paused as in the pause menu ([ExtraMenu] PauseSound=1)
10:00:05.000 extra menu: load list opened
10:00:05.000 """ + MU_S + """load list: opened from the pause: pause mode 2, master 0.00, fade 0: the level music stays paused
10:00:09.000 extra menu: back in the pause menu on the Extra Menu, the world kept frozen (1)
10:00:09.000 extra menu: load list cancelled
10:00:09.000 """ + MU_S + """load list: closed, back in the pause: pause mode 0, master 0.00, fade 4: the level music stays paused
10:00:12.000 """ + MU_S + """load list: opened from the pause: pause mode 2, master 0.00, fade 0: the level music stays paused
10:00:20.000 extra menu: load game: slot read (stage 2 room 1)
10:00:20.000 """ + MU_S + """load list: closed, a save loads (the room change takes the music over): pause mode -1, master 0.00, fade 1: the level music comes back
10:00:30.000 """ + MU_S + """save menu: opened from the pause: pause mode 2, master 0.00, fade 0: the level music stays paused
10:00:35.000 """ + MU_S + """save menu: closed, back in the pause: pause mode 0, master 0.00, fade 4: the level music stays paused
10:00:40.000 """ + MU_S + """storage chest: opened from the pause: pause mode 2, master 0.20, fade 0: the level music PLAYS quietly
10:00:40.001 ANOMALY: extra menu: the level music is not paused in the storage chest (opened from the pause; pause mode 2, master 0.20, fade 0)
"""
# v3.52 SND352: Sound (sound.inc): the install line, the start line, changes on Option 1 and the Sound page, a discard, a save's
# Sound put back, the engine re-applied, two minute lines
SND_IN_OK = ('sound: in: pan law Smooth (004090BD / 0040B617), volume hooks in (Night Mode), Room Reverb in (the sound reads of '
             '00ED4F40), Mono fold in (streams) / in (music); the mode at the first frame (Stereo)')
SND_START = ("sound: Mode %s (the game's Sound %d, engine %d), pan law %s, Stereo Width %d %%, Headphones floor -9 dB, Room Reverb %s "
             "(%s), Night Mode %s, Voice volume not in this build, Mono fold on; speakers stereo (00140004)")
SND_MIN = ('[v] sound: minute: volumes 2D %d, 3D %d, streams %d (Night Mode %s: %d changed); pans 2D %d (one ear only %d; the stock '
           'law %d), streams %d (one ear only %d; the stock law %d); Mono: %d stream block(s) folded, %d music fold(s); Mode %s, engine %d')
V352SND = V333_END + "10:00:00.010 " + SND_IN_OK + """
10:00:01.000 """ + SND_START % ('Stereo', 1, 1, 'Smooth', 100, 'On', 'probe ok', 'Off') + """
10:00:01.000 [v] sound: reverb probe: 3D buffer hr 0878000A (DS_NO_VIRTUALIZATION: no HRTF, plain 3D), SetFX hr 00000000 result 2, reverb object hr 00000000; 2D buffer hr 00000000, SetFX hr 00000000 result 2, reverb object hr 00000000
10:01:01.000 """ + SND_MIN % (1200, 40, 300, 'Off', 0, 1000, 5, 400, 200, 0, 60, 0, 0, 'Stereo', 1) + """
10:01:30.000 sound: Mode Stereo -> Headphones (Options): the game's Sound 1 (Stereo), the other ear at most 9 dB down
10:01:31.000 sound: Stereo Width 100 -> 80 % (Options)
10:01:32.000 sound: Room Reverb On -> Off (Options): the sound reads of the CPU class see 0
10:01:33.000 sound: Night Mode Off -> Low (Options)
10:01:40.000 sound: Mode Headphones -> Stereo (Options discarded): the game's Sound 1 (Stereo)
10:01:40.000 sound: Options discarded: the engine back to Sound 1 (it kept 0, the last value passed over)
10:01:41.000 sound: the engine was told Sound 0 but the Options say 1 (the game's discard skips Sound): applied
10:01:50.000 sound: the Options block's Sound came in as 2 (a save's or the memory card's copy): 1 for Mode Stereo
10:02:01.000 """ + SND_MIN % (800, 10, 100, 'Low', 700, 500, 0, 100, 100, 0, 40, 0, 0, 'Stereo', 1) + """
"""
# v3.52 SND352: the first start (Mode from the save's byte), PanLaw=Stock, the volume sites not stock, the reverb probe failed
V352SND1 = V333_END + """10:00:00.005 sound: volume (004090A6 / 00409285 / 0040B5E7): 004090A6 is not the GOG v7 code (E8 11 22 33 ...; another mod?): not installed
10:00:00.010 sound: in: pan law Stock ([Sound] PanLaw=Stock: not patched), volume hooks NOT IN (Night Mode), Room Reverb in (the sound reads of 00ED4F40), Mono fold off (MonoFold=0) (streams) / off (music); the mode at the first frame (first start: from the save's Sound)
10:00:01.000 sound: first start: [Sound] Mode=Mono, from the save's Option 1 Sound 0 (Monaural)
10:00:01.000 ANOMALY: sound: Room Reverb: DirectSound would not make a buffer with the Waves Reverb (3D buffer hr 88780078, SetFX hr 80004005 result 0, reverb object hr 80004005; -): no reverb this session
10:00:01.000 """ + SND_START % ('Mono', 0, 0, 'Stock', 100, 'On', 'PROBE FAILED: none', 'Off') + """
"""
# v3.51 MU351: PauseSound=0 (v3.50's sound): quiet music is no fault; and a load list opened from play (Seamless=0)
V351MU0 = V333_END + """10:00:00.010 extra menu: sound in the Storage Chest / Save Game / Load Game: plays quietly as in v3.50 ([ExtraMenu] PauseSound=0)
10:00:05.000 """ + MU_S + """load list: opened from the pause: pause mode 2, master 0.20, fade 0: the level music PLAYS quietly (PauseSound=0: v3.50)
10:00:09.000 """ + MU_S + """load list: closed, back in the pause: pause mode 0, master 0.00, fade 4: the level music stays paused (PauseSound=0: v3.50)
10:00:15.000 """ + MU_S + """load list: opened from play: pause mode 2, master 0.84, fade 2: the level music PLAYS quietly
10:00:19.000 """ + MU_S + """load list: closed, back to play: pause mode -1, master 0.20, fade 1: the level music comes back
"""

# v3.52 QL352: Quick Load's paths (extramenu.inc ql_go / QlSoundTick, quicksave.inc QsLoad): the install line, two quick
# loads inside the pause (one handed to a cut-scene), one by the key, one v3.51 fallback (a message was up), a refusal at
# black, a late black with the music not paused (+ its ANOMALY line), a room never seen running
QL_IN = 'quick load: slot 1 (sequence 25), stage 2 room 4, game time 40974; path: the Extra Menu inside the pause (black in '
V352QL = V333_END + """10:00:00.010 extra menu: Quick Load runs inside the pause (the world frozen and the level music paused until the loaded room starts) ([ExtraMenu] QuickLoadInPause=1)
10:00:05.000 """ + QL_IN + """16 frames; pause mode 2, master 0.00, fade 0: the level music stays paused)
10:00:06.800 quick load: the loaded room runs after 1800 ms (stage 2 room 4): the level music fades in (0x56E960(-1) from pause mode 2)
10:00:20.000 quick load: slot 2 (sequence 26), stage 3 room 1, game time 50000; path: the Quick Load key in play
10:00:30.000 quick load: slot 2 (sequence 26), stage 3 room 1, game time 50000; path: the Extra Menu after the pause closed (v3.51: a message was up)
10:00:40.000 quick load: refused at black: every catalog entry is clear data; back in the pause (path: the Extra Menu inside the pause)
10:00:50.000 """ + QL_IN + """16 frames; pause mode 2, master 0.00, fade 0: the level music stays paused)
10:00:52.400 quick load: the loaded room opened a menu or a cut-scene first (sub-mode 0, pending 0, phase 1) after 2400 ms: its own end gives the level music back
"""
V352QL2 = V333_END + """10:00:00.010 extra menu: Quick Load closes the pause first (v3.51) ([ExtraMenu] QuickLoadInPause=1: 0055A160 is not stock)
10:00:05.000 """ + QL_IN + """90 frames, NOT black: loaded anyway; pause mode 2, master 0.20, fade 0: the level music PLAYS quietly)
10:00:05.001 ANOMALY: extra menu: Quick Load: the level music is not paused at black (pause mode 2, master 0.20, fade 0)
10:00:35.000 quick load: the loaded room runs after 30000 ms (stage 2 room 4): the level music fades in (0x56E960(-1) from pause mode 2): NOT seen running
10:00:35.001 ANOMALY: extra menu: Quick Load: no loaded room seen running within 30000 ms: the sound given back anyway (start mode 1, next stage 2 room 4, fade target 128)
"""


# v3.51 PT351: the tar portal and the hold (fadein.inc): the new install line, tar starts (the game's draw on a wall,
# Refined's in the air for a placeholder), a placeholder turned real, the first tar portal drawn, two let-go lines, a portal
# nobody came out of, a room line with the portal part
FI_INSTALL351 = ("fade in: in (no code patch: the model draw 0052AEB5 and shadow 0052AF95 hooks; the game's see-through enemy draw "
                 "0045C880; the portal from the scene pass 004EFBFD; the tar portal %s); [Gameplay] FadeIn Fade + Portal, "
                 "FadeInSeconds 0.80, FadeInPortalDraw %s, FadeInHold %s; never bosses, Wall Men, Walter, event or cut-scene "
                 "figures; a fading enemy acts as usual; a held ghost cannot grab")
FI_TAR_OK = ("after the game's pass 2 (its step list 0062BC98): the game's own wall-portal draw 004868B0 / 00486E80 (textures "
             "F000F020 / F001F020), else Refined's; strands: the ghost's own 00478440 / 00482CF0, else Refined's; the hold from "
             "the tick 004EF982 (0045CF20 and the action latch)")
V351PT = V333_END + """10:00:00.300 """ + FI_INSTALL351 % (FI_TAR_OK, 'Auto', '1') + """
10:00:09.000 [v] fade in: follower FFFF0007 (type 0B) fades in over 0.80 s from a tar portal (the game's own draw, on a wall 85 u behind; held 2.50 s, 40 % for 1.50 s) at 10 0 -200 (room 2:2, 4.1 s into the visit)
10:00:09.020 fade in: a tar portal drawn (the game's own draw, viewport 3D 2560x1440, after the game's pass 2 004EF900)
10:00:10.000 [v] fade in: follower 1E001C06 (type 0B) fades in over 0.80 s from a tar portal (Refined's draw, in the air 70 u behind; held 2.50 s, the arrival's placeholder) at 40 0 -210 (room 2:2, 5.1 s into the visit)
10:00:10.400 [v] fade in: 1E001C06 (type 0B) is real now, 0.40 s after its tar portal opened: its hold goes on
10:00:11.500 [v] fade in: FFFF0007 (type 0B) let go by its tar portal (the game's own draw) after 2.50 s (held to the end): strands: the ghost's own 32; moved 41 of 96 u; no attack for 2.50 s; portal draw 0.052 ms a frame (max 0.140, 150 frames)
10:00:11.600 [v] fade in: 1E001C06 (type 0B) let go by its tar portal (Refined's draw) after 1.20 s (it is gone): strands: Refined's 12; moved 10 of 25 u; no attack for 1.20 s; portal draw 0.080 ms a frame (max 0.200, 72 frames)
10:00:12.000 [v] fade in: follower 1E001C09 (type 0B) fades in over 0.80 s from a tar portal (Refined's draw, on a wall 120 u behind; held 2.50 s, the arrival's placeholder) at 30 0 -190 (room 2:2, 7.1 s into the visit)
10:00:16.100 [v] fade in: 1E001C09 (type 0B): its tar portal closed after 4.1 s: it never came out (still the arrival's placeholder)
10:00:20.000 fade in: room 2:2: 4 fade(s) (room load 2, follower 2, wanderer 0, copy 0, revive 0, ghost 0, monster 0), 3 portal(s) (the game's own draw 1, Refined's 2, vortex 0; 2 held; portal draw 0.061 ms a frame, max 0.200); 1 not faded (stage 0, cut-scene 0, type 0, boss 0, event 0, dead 0, own draw 0, own portal 1, own fade 0)
"""
# v3.51 PT351: an unknown FadeInPortalDraw, a changed bone trail, a fault in the game's own draw (not Fade In's fault)
V351PT2 = V333_END + """10:00:00.290 fade in: [Gameplay] FadeInPortalDraw "Splash" is not Auto / Refined / Vortex: Auto (the default)
10:00:00.295 fade in: 00478440 (the bone trail's start) is not the stock code (another mod?): the tar portal does without it
10:00:00.300 """ + FI_INSTALL351 % ("after the game's pass 2 (its step list 0062BC98): the game's own wall-portal draw 004868B0 / 00486E80 "
                                   "(textures F000F020 / F001F020), else Refined's; strands: Refined's (the bone trail is not stock); the "
                                   "hold from the tick 004EF982 (0045CF20 and the action latch)", 'Auto', '1') + """
10:01:00.000 fade in: a fault in the game's own portal draw (code C0000005 at 0045E9A1): Refined's own tar portal from now on
10:01:00.001 ANOMALY: fade in: a fault in the game's own portal draw (code C0000005 at 0045E9A1): Refined's own from now on
"""
# v3.51 PT351: FadeInPortalDraw=Vortex (v3.50's look, no hold)
V351PT3 = V333_END + """10:00:00.300 """ + FI_INSTALL351 % ("NOT available (FadeInPortalDraw=Vortex): v3.50's blood vortex", 'Vortex', '1') + """
10:00:09.000 [v] fade in: follower FFFF0007 (type 0B) fades in over 0.80 s from a blood portal at 10 0 -200 (room 2:2, 4.1 s into the visit)
10:00:09.020 fade in: a blood portal drawn (viewport 3D 2560x1440, the exact depth of 012E3ED0)
10:00:20.000 fade in: room 2:2: 1 fade(s) (room load 0, follower 1, wanderer 0, copy 0, revive 0, ghost 0, monster 0), 1 portal(s) (the game's own draw 0, Refined's 0, vortex 1; 0 held; portal draw 0.000 ms a frame, max 0.000); 0 not faded (stage 0, cut-scene 0, type 0, boss 0, event 0, dead 0, own draw 0, own portal 0, own fade 0)
"""


def sess(t0, ver, rend, chain, rooms, end=True, started=None, anomalies=0, perf=None, title=None):
    """a synthetic session log: t0 'HH:MM:SS', rend the Renderer start line's why, chain the display chain, rooms
    [(play s, frames, mean, p99, worst, spikes)] in stage 2; title: one more such room first, the title (stage 0)"""
    h, m, s = (int(x) for x in t0.split(':'))
    t = h * 3600 + m * 60 + s

    def ts(dt):
        x = t + dt
        return '%02d:%02d:%02d.000' % (x // 3600 % 24, x // 60 % 60, x % 60)
    out = ['%s SH4 Refined v%s loading (winmm.dll proxy, game base 00400000)' % (ts(0), ver),
           '%s log: level 2 (verbose; [Log] Level 1 normal, 2 verbose, 3 trace); grep "ANOMALY:" for states a feature should never reach' % ts(0)]
    if started:
        out.append('%s log: started %s (local time)' % (ts(0), started))
    out.append('%s session: SH4 Refined v%s (file version %s.0), winmm.dll md5 0123456789abcdef0123456789abcdef, 1 bytes, built x; log level 2' % (ts(1), ver, ver))
    out.append('%s renderer: %s; row open (offered); manifest read (4 offered, 4 recognised only, 1 d3d9.dll pin(s)); d3d9.dll CAA75C55D9B3121D17433BD3B38C7BB2 (allowed)' % (ts(1), rend))
    out.append('%s display: Borderless, game size 2560x1440, window client 800x627, gameplay rate 60 (FrameMode 0); chain: %s' % (ts(2), chain))
    dt, n, fr, sm, worst, sp = 10, 0, 0, 0.0, 0.0, 0
    p99 = 0.0
    for i, (play, frames, mean, p, w, spk) in enumerate(([title] if title else []) + rooms):
        dt += int(play) + 5
        room = '0:00' if title and i == 0 else '2:%02d' % (i + 1)
        out.append('%s [v] room: %s left after %.1f s (play %.1f, cutscene 0.0, paused 5.0); %d frames: mean %.1f ms, p99 %.1f, '
                   'worst %.1f at +1.0 s (in play; Refined 0.10 ms), spikes %d (over 50 ms), stalls 0; VA 700 of 2047 MB (margin 1347)'
                   % (ts(dt), room, play + 5, play, frames, mean, p, w, spk))
        n, fr, sm, worst, sp, p99 = n + 1, fr + frames, sm + mean * frames, max(worst, w), sp + spk, max(p99, p)
    for i in range(anomalies):
        out.append('%s ANOMALY: frame: a 412 ms freeze in play, stage 2 room %d' % (ts(dt), i))
    if perf:
        out.append('%s [v] perf: the last 5 minutes: 300 s, %d frames, %.1f fps on average, the lowest 1%% low %.1f fps (now 58.1, '
                   'p99 17.2 ms), worst frame 41.0 ms, Refined 1.20 ms a frame (17000 ledger frames), address space 812 of 2048 MB '
                   '(most 840)' % (ts(dt), perf[0] * 300, perf[0], perf[1]))
    if end:
        out.append('%s session: end (process exit) after %s: %d room visit(s) (%d rooms); frames %d: mean %.1f ms, p99 %.1f, worst %.1f '
                   'in 2:01 at +1.0 s (warm-up 37.1 ms; budget level 0, spikes %d, stalls 0; VA peak 805 of 2047 MB (margin 1242); '
                   '%d ANOMALY line(s); 0 feature(s) faulted; ini fine' % (ts(dt + 5), logcheck.hms(dt + 5), n, n, fr, sm / fr, p99 + 0.5,
                                                                             worst, sp, anomalies))
    return '\n'.join(out) + '\n'


CH_D3D9 = 'd3d8.dll (game folder) ori_d3d8.dll (game folder, v1.0.0.0) d3d9.dll (game folder, v4.9.0.904) dxgi.dll (C:\\WINDOWS\\SYSTEM32\\)'
CH_D3D11 = CH_D3D9 + ' d3d11.dll (C:\\WINDOWS\\SYSTEM32\\, v10.0)'
CH_D3D12 = CH_D3D9 + ' d3d12.dll (C:\\WINDOWS\\SYSTEM32\\, v10.0)'
R_STOCK = 'dgVoodoo.conf is Stock (passthru, C500F5C9E089D915288AD20FF89CD68A)'
R_D3D12 = 'dgVoodoo.conf is Direct3D 12 (d3d12, 67FDE0CF1FA6E95EFB53F0F398540F4E)'
R_D3D11 = 'dgVoodoo.conf is Direct3D 11 (d3d11_flipdiscard, 702E9E0AF46385A7B59091D5ACA3C404)'


def put(path, text, mtime):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    os.utime(path, (mtime, mtime))


def test_compare():
    import contextlib
    import io
    import shutil
    import time
    d = tempfile.mkdtemp(prefix='logcheck_cmp_')
    logs = os.path.join(d, 'logs')
    os.makedirs(logs)
    mt = lambda *a: time.mktime(a + (0, 0, -1))  # noqa: E731
    put(os.path.join(logs, 'SH4Refined 2026-11-20 2300 v3.39.0.log'),  # past midnight, no "log: started" line (v3.39)
        sess('23:00:07', '3.39.0', R_STOCK, CH_D3D9, [(400, 24000, 16.9, 25.0, 80.0, 1), (300, 18000, 17.1, 30.0, 120.0, 2)],
             perf=(59.5, 48.0), title=(60, 1800, 33.4, 35.8, 70.6, 0)), mt(2026, 11, 21, 0, 15, 0))
    put(os.path.join(logs, 'SH4Refined 2026-11-21 0900 v3.39.0.log'),  # a crash: no end line, two ANOMALY lines
        sess('09:00:30', '3.39.0', R_D3D12, CH_D3D12, [(200, 12000, 17.0, 40.0, 300.0, 3), (100, 6000, 18.0, 60.0, 412.0, 1)],
             end=False, anomalies=2), mt(2026, 11, 21, 9, 6, 0))
    rot3 = os.path.join(logs, 'SH4Refined 2026-11-22 0724 v3.40.0.log')
    put(rot3, sess('07:24:21', '3.40.0', R_D3D11, CH_D3D11, [(500, 30000, 16.7, 18.0, 36.0, 0), (400, 24000, 16.7, 19.0, 40.0, 0)],
                   started='2026-11-22 07:24:21'), mt(2026, 11, 22, 7, 40, 0))
    shutil.copy2(rot3, os.path.join(d, 'SH4Refined.prev.log'))  # the session before: the same file as its rotated copy
    put(os.path.join(logs, 'notes.txt'), 'not a log of Refined\'s', mt(2026, 11, 22, 8, 0, 0))
    put(os.path.join(d, 'SH4Refined.log'), sess('08:00:00', '3.40.0', R_D3D11, CH_D3D11, [(30, 1800, 16.7, 17.5, 20.0, 0)],
                                                started='2026-11-22 08:00:00'), mt(2026, 11, 22, 8, 1, 0))
    files = logcheck.folder_logs(d)
    check([os.path.basename(f) for f in files] == ['SH4Refined.log', 'SH4Refined 2026-11-22 0724 v3.40.0.log',
                                                   'SH4Refined 2026-11-21 0900 v3.39.0.log', 'SH4Refined 2026-11-20 2300 v3.39.0.log'],
          'v3.40: a folder\'s logs: SH4Refined.log, then logs\\ newest first; .prev.log left out as logs\\ holds it; notes.txt not read')
    ss = []
    for p in files:
        s = logcheck.Session(p)
        s.read()
        ss.append(s)
    r0 = ss[3].row()
    check(r0['start'].strftime('%Y-%m-%d %H:%M:%S') == '2026-11-20 23:00:07' and r0['label'] == 'Stock' and r0['variant'] == 'passthru'
          and r0['api'] == 'D3D9', 'v3.40: a v3.39 rotated log: its start from the name (+ the first line\'s seconds), Stock / D3D9 (%r)' % r0)
    check(r0['frames'] == 43800 and r0['spikes'] == 3 and not r0['approx'] and r0['perf'][0] == 1 and r0['ended'] == 'process exit',
          'v3.40: the end line\'s frames and spikes, one perf summary')
    check(r0['wframes'] == 42000 and r0['wplay'] == 700 and abs(r0['wmean'] - (16.9 * 24000 + 17.1 * 18000) / 42000) < 1e-6 and
          abs(r0['wp99'] - (25.0 * 24000 + 30.0 * 18000) / 42000) < 1e-6 and r0['p99'] == 36.3 and r0['mean'] > 17.5,
          'v3.40: the world numbers leave the title (stage 0, 30 fps) out; "all" keeps it (%r)' % r0)
    r1 = ss[2].row()
    check(r1['ended'] == 'MISSING' and r1['approx'] and r1['p99'] == 60.0 and r1['spikes'] == 4 and r1['anomalies'] == 2 and
          r1['api'] == 'D3D12' and abs(r1['mean'] - (17.0 * 12000 + 18.0 * 6000) / 18000) < 1e-6,
          'v3.40: a crash: room lines frame-weighted, p99 the worst room\'s (~), 2 ANOMALY lines, D3D12 (%r)' % r1)
    r2 = ss[0].row()
    check(r2['start'] == logcheck.datetime.datetime(2026, 11, 22, 8, 0, 0) and r2['variant'] == 'd3d11_flipdiscard' and
          r2['label'] == 'Direct3D 11' and r2['api'] == 'D3D11', 'v3.40: the "log: started" line dates the session; Direct3D 11')
    k = logcheck.Session(os.path.join(logs, 'SH4Refined 2026-11-20 2300 v3.39.0.log'))  # its last write 2026-11-21 00:15
    k.t0, k.name = 23 * 3600 + 58 * 60, 'SH4Refined.prev.log'  # an older log's start: the last write's day, a day earlier
    check(k.start_dt() == logcheck.datetime.datetime(2026, 11, 20, 23, 58, 0), 'v3.40: no start line, no rotated name: the last '
          'write\'s date, a day earlier past midnight (%r)' % k.start_dt())
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        logcheck.main([d, '--table'])
    out = buf.getvalue()
    print(out)
    lines = out.splitlines()
    rows = [l for l in lines if re.match(r'^\d+\*? +2026-', l)]
    check(len(rows) == 4 and rows[0].startswith('1  ') and '2026-11-20 23:00' in rows[0] and '2026-11-22 08:00' in rows[3],
          'v3.40: the table: 4 sessions, oldest first')
    check(rows[3].startswith('4* ') and not rows[0].startswith('1*'), 'v3.40: a session under 10 min of play is marked *')
    check('~60.0' in rows[1] and 'MISSING' in rows[1] and rows[1].split()[-2] == '2', 'v3.40: the crash row: ~p99, MISSING, 2 ANOMALY')
    check('1: 59.5 / 48.0' in rows[0], 'v3.40: the perf summary column (count: fps / lowest 1% low)')
    byr = lines[lines.index([l for l in lines if l.startswith('== by renderer')][0]) + 2:]
    d11 = [l for l in byr if 'd3d11_flipdiscard' in l]
    check(len(d11) == 1 and re.search(r'D3D11 +2 ', d11[0]) and ' 55800 ' in d11[0], 'v3.40: by renderer: Direct3D 11\'s two '
          'sessions in one line (%r)' % d11)
    d12 = [l for l in byr if 'd3d12' in l and 'flip' not in l]
    check(len(d12) == 1 and d12[0].rstrip().endswith(' 1') and re.search(r' 2 +1$', d12[0]), 'v3.40: by renderer: D3D12 crashed once, '
          '2 ANOMALY')
    check(not [l for l in lines if 'fewer than two renderers' in l], 'v3.40: two renderers with 10 min of play: no "too short" note')
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):  # a log named twice (prev + its rotated twin): one session
        logcheck.main([os.path.join(d, 'SH4Refined.prev.log'), rot3, os.path.join(d, 'SH4Refined.log')])
    out = buf.getvalue()
    check('== 2 session(s) compared' in out and out.count('\n== SH4Refined') + out.startswith('== SH4Refined') == 2,
          'v3.40: files given: a report each (the twin once) and the table')
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):  # the folder: reports for this session and the one before, then the table
        logcheck.main([d])
    out = buf.getvalue()
    check(out.count('  version: SH4 Refined') == 2 and '== 4 session(s) compared' in out, 'v3.40: a folder: 2 reports + the table')
    shutil.rmtree(d, ignore_errors=True)
    # v3.40 REV340 (Codex LG340-001): two different sessions of the same size written in the same second stay two; the
    # same bytes under two names are one
    d = tempfile.mkdtemp(prefix='logcheck_same_')
    logs = os.path.join(d, 'logs')
    os.makedirs(logs)
    t = mt(2026, 11, 23, 9, 5, 0)
    a = sess('09:00:00', '3.40.0', R_D3D11, CH_D3D11, [(30, 1800, 16.7, 17.5, 20.0, 0)], started='2026-11-23 09:00:00')
    b = a.replace('09:00:00', '09:00:01')  # another session, the same length
    put(os.path.join(d, 'SH4Refined.log'), b, t)
    put(os.path.join(d, 'SH4Refined.prev.log'), a, t)
    put(os.path.join(logs, 'SH4Refined 2026-11-23 0900 v3.40.0.log'), a, t)
    put(os.path.join(logs, 'SH4Refined 2026-11-23 0900 v3.40.0 #2.log'), b.replace('BBBB', 'CCCC').replace('09:00:01', '09:00:02'), t)
    check(len(a) == len(b) and not logcheck.same_file(os.path.join(d, 'SH4Refined.log'), os.path.join(d, 'SH4Refined.prev.log')),
          'v3.40 REV340: same size, same second, different bytes: not the same file')
    check(logcheck.same_file(os.path.join(d, 'SH4Refined.prev.log'), os.path.join(logs, 'SH4Refined 2026-11-23 0900 v3.40.0.log')),
          'v3.40 REV340: the same bytes under two names: the same file')
    names = [os.path.basename(f) for f in logcheck.folder_logs(d)]
    check(names == ['SH4Refined.log', 'SH4Refined 2026-11-23 0900 v3.40.0 #2.log', 'SH4Refined 2026-11-23 0900 v3.40.0.log'],
          'v3.40 REV340: a folder keeps distinct same-size logs, drops only .prev.log\'s byte-equal copy (%r)' % names)
    shutil.rmtree(d, ignore_errors=True)


# v3.50 R302: Room 302's look and walk (room302.inc): the user's game today (Playable Eileen v1.24.4 without the yield
# export: inert) and a game where Refined owns both groups (no Playable Eileen, or one that yields)
R3_OWNERS = ("room 302: owners (%s): look %s, walk %s; Playable Eileen %s; [Room302] LookDownLimit %s, LookUpLimit %s, "
             "LookFeel %s, WalkSpeed %s%s")
R3_PE124 = ("v1.24 loaded without SH4PE_Room302Yield (older than the hand-over): its Room 302 look and walk stay its own (Eileen "
            "only); Refined's Room 302 rows are greyed, nothing hooked")
V350R = V333_END + """10:00:00.100 settings: [Room302] LookDownLimit was missing, added as 85 (from SH4PlayableEileen.ini [Room302])
10:00:00.101 settings: [Room302] LookUpLimit was missing, added as 85 (from SH4PlayableEileen.ini [Room302])
10:00:00.102 settings: [Room302] LookFeel was missing, added as 1 (from SH4PlayableEileen.ini [Room302])
10:00:00.103 settings: [Room302] WalkSpeed was missing, added as 0.65 (from SH4PlayableEileen.ini [Room302])
10:00:00.104 """ + R3_OWNERS % ('at start', 'Playable Eileen', 'Playable Eileen', R3_PE124, '85', '85', '1', '0.65', '') + """
"""
V350RA = V333_END + """10:00:00.100 settings: [Room302] LookDownLimit was missing, added as 85
10:00:00.101 room 302: [Room302] LookFeel=0.8 is not one of the row's values: 1 used
10:00:00.102 room 302: look hooks in (at start): 0052DF74 call, 0052DCC6 / 0052DD14 operands
10:00:00.103 """ + R3_OWNERS % ('at start', 'SH4 Refined', 'SH4 Refined', 'not loaded', '85', '85', '1', '0.65', '') + """
10:03:00.000 [v] room 302: look: the view was rolled or past a limit (pitch 91.2 deg, roll 0.00): rebuilt at pitch 85.0, no roll
10:05:00.000 [v] room 302: visit 120 s (left): look 7200 tick(s) (eased 3000, clamped 40: up 25, down 15; view rebuilt 1), walk x0.65 7100 tick(s)
10:06:00.000 room 302: [Room302] WalkSpeed 0.65 -> 0.8 (Options)
10:09:00.000 [v] room 302: visit 300 s (5 minutes): look 18000 tick(s) (eased 100, clamped 0: up 0, down 0; view rebuilt 0), walk x0.8 17000 tick(s)
10:09:30.000 ANOMALY: room 302: look: nobody (Playable Eileen not loaded, its patch list does not cover the group's sites, the sites another module's): nobody's hook is in
"""
V350RW = V333_END + """10:00:00.104 """ + R3_OWNERS % ('at start', 'waiting for Playable Eileen', 'waiting for Playable Eileen',
                                                        'v1.25 loaded, SH4PE_Room302Yield not decided yet', '85', '85', '1', '0.65',
                                                        '; decided again on the first game tick') + """
10:00:01.000 """ + R3_OWNERS % ('first game tick', 'SH4 Refined', 'Playable Eileen', 'v1.25 yields look (SH4PE_Room302Yield)',
                               '85', '85', '1', '0.65', '') + """
10:00:01.001 room 302: look hooks in (first game tick): 0052DF74 call, 0052DCC6 / 0052DD14 operands
"""

# v3.52 HB352: the enemy bars refused (an arriving ghost in an elevator) and a boss's first bar (the lines hudbars.inc writes)
V352HB = V333_END + """10:00:01.000 [v] hud bars: no bar for FFFF0007 (type 0B) in room 5:C: not in the room yet (the game does not draw it: hidden, arriving or a placeholder) (state D, at -1590 -160 1248, 12 from you)
10:00:01.100 [v] hud bars: no bar for FFFF000A (type 0B) in room 5:C: not in the room yet (the game does not draw it: hidden, arriving or a placeholder) (state E, at -1580 -160 1250, 20 from you)
10:00:01.200 [v] hud bars: no bar for 05020003 (type 07) in room 5:C: not at a place in the room (the origin, parked away or not a number) (state 1, at 0 0 0, 1900 from you)
10:00:30.000 [v] hud bars: room 5:C: no bar for 2 enemy(ies) not in the room yet (hidden, arriving or a placeholder), 1 not at a place in the room; this session 2 / 1
10:01:00.000 [v] hud bars: boss Walter 841F0001 (type 10) in room 4:1F: its bar is up 0.2 s after it became active (state 3, 2480 from you, hp 3000 / 3000)
10:02:00.000 [v] hud bars: boss Walter 841F0001 (type 10) in room 4:22: active 5 s and no bar yet: walled off from the camera (state 3, 900 from you, hp 3000 / 3000)
10:03:00.000 [v] hud bars: room 4:22: no bar for 1 enemy(ies) not in the room yet (hidden, arriving or a placeholder), 0 not at a place in the room; this session 3 / 1
"""


def run(text, name='SH4Refined.log'):
    d = tempfile.mkdtemp(prefix='logcheck_')
    p = os.path.join(d, name)
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    s = logcheck.Session(p)
    s.read()
    return s, '\n'.join(s.report(False, None))


# v3.52 FX352: the depth of field with a close camera (a focus, the far side only), Bloom's five levels and the one-time
# migration of a v3.51 value, the Otherworld lens' room lines
V352FX = V333_END + """10:00:00.050 post effects: [ScreenEffects] Bloom=Very High (v3.43 .. v3.51) is v3.52's High (v3.52: Off, Low, Medium, High, Ultra; the old High is the new Medium, the old Very High the new High): written as High (marker BloomV=352)
10:00:00.060 post effects: in place (drawn in the scene pass before the filter; device reset 00402DF4 hooked); Bloom High, Colour Grading Off, Otherworld Lens On (the lens at 70 % while a ghost or haunting is in the room, full halfway into its aura)
10:00:00.100 depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to 10012340); DepthOfField Always, Strength Medium; probe on (once per room); preview on (the Screen Effects page, 0x526A53)
10:00:05.000 depth of field: first frame drawn (Always, Medium): focus 627 (the player), 12 slice(s), 12 blurred band(s), 0.41 ms; render targets 33.6 MB; eased in (a quarter level a step)
10:00:20.000 [v] depth of field: stage 2 room 1: the player is close to the camera (view depth 120): the focus on the player (200), the far side only (v3.51 took this for a first-person view and drew nothing)
10:01:00.000 [v] depth of field: stage 2 room 1: drawn in 17660 of 17668 frames it was asked for (not drawn: the room's depth probe is pending 8; the cut-scene's own focus 0); skipped by design: none; focus 200 .. 873 (the player close to the camera in 7733 frame(s)); ours 0.120 ms a frame on average, 0.300 at most; Always Medium; probe: the scene's depth: on; render targets 33.6 MB
10:01:00.500 [v] post effects: the Otherworld lens in stage 2 room 1: shown 40 of 60 s (67 %), at most 100 % strength (haunting strength up to 2.00, nearness up to 0.62; the base 70 %)
10:01:30.000 screen effects: Bloom Ultra
10:02:00.000 [v] depth of field: stage 2 room 2: drawn in 900 of 900 frames it was asked for (not drawn: none; the cut-scene's own focus 0); skipped by design: none; focus 300 .. 700; ours 0.110 ms a frame on average, 0.200 at most; Always Medium; probe: the scene's depth: on; render targets 33.6 MB
10:02:00.500 [v] post effects: the Otherworld lens in stage 2 room 2: shown 15 of 15 s (100 %), at most 70 % strength (haunting strength up to 1.00, nearness up to 0.00; the base 70 %)
"""
V352FXN = V333_END + """10:00:00.050 post effects: [ScreenEffects] Bloom=High (v3.43 .. v3.51) is v3.52's Medium (v3.52: Off, Low, Medium, High, Ultra; the old High is the new Medium, the old Very High the new High): could not be written to SH4Refined.ini (used for this start; tried again at the next): the value
10:00:00.060 post effects: in place (drawn in the scene pass before the filter; device reset 00402DF4 hooked); Bloom Medium, Colour Grading Off, Otherworld Lens Off (the lens at 70 % while a ghost or haunting is in the room, full halfway into its aura)
"""


def main():
    s, r = run(V333)
    print(r)
    check(s.version == '3.33.0' and s.level == 2, 'version and log level read')
    check('15a85581' in r and 'Playable Eileen loaded' in r, 'the header lines are shown')
    check('FilterStrength=Medium (hidden key)' in r, 'the settings that are not default are shown')
    check('end: MISSING' in r, 'a v3.33 log without its end line: MISSING (crash or killed)')
    check('rooms: 3 visits, 3 rooms' in r, 'rooms counted from the room lines')
    check('loads: 1: 23:59:09 2:04 (game time 35272, drops 1/15, pickup free 9/32)' in r, 'the load line: the save\'s drops ledger and pickup pool')
    check('ANOMALY: 5 line(s), 4 kind(s)' in r, 'anomalies counted and grouped by kind (numbers ignored)')
    check('  2x extra menu' in r, 'the two Quick Save row anomalies are one group of 2')
    check('faults: 1' in r, 'the faulted line is listed')
    check('2:02 412.0 ms at +12.3 s' in r, 'the worst room first, with its frame')
    check('After Shock     NEVER acted in 0 of 2 room(s)' in r, 'After Shock never acted while on')
    check('Echo            acted in 2 of 2 room(s), total 7000' in r, 'Echo acted in both rooms, its total')
    check('Supply Drops    acted in 1 of 2 room(s)' in r, 'Supply Drops acted in one room')
    check('never acted while on: After Shock' in r, 'the never-acted summary line')
    check(s.span > 300 and s.span < 400, 'the session spans midnight correctly (%.0f s)' % s.span)
    check(len(r.splitlines()) <= 40, 'the report stays short (%d lines)' % len(r.splitlines()))
    s, r = run(V333_END)
    check('end: end (process exit)' in r and 'MISSING' not in r, 'the end line is shown, no MISSING')
    s, r = run(V335)  # v3.35 FX335
    print(r)
    check(len(s.rooms) == 2 and s.rooms[0][2]['why'].startswith('after a load; Refined 259.68 ms; after shock 259.2 ms'),
          'v3.35: the room line still parses, its worst frame named from the ledger (%r)' % (s.rooms[0][2].get('why') if s.rooms else None))
    check('2:05 34.7 ms at +30.0 s (in play; Refined 34.71 ms; echo 21.3 ms, after shock 8.1, death presence 3.2; budget level 5)' in r,
          'v3.35: a slow frame in play names its top costs and the budget level')
    check('effect budget: in; 2 step(s) down, 1 up, deepest level 6; thinned in 1 of 2 room line(s)' in r, 'v3.35: the effect budget summary')
    check('2:04 at most level 6, 52 % of the frames thinned, effects 7.2 ms mean / 21.4 max (target 6.0), 6.3 % late' in r, 'v3.35: the thinned room')
    check('warm-up: 1 time(s), 153.9 ms in all; the longest 153.9 ms in 2:04 (screen effects 12.3, death presence 41.0, echo 97.1, after shock 3.2)' in r,
          'v3.35: the warm-up line and its parts')
    s, r = run(V336)  # v3.36 TP336
    print(r)
    check(s.pool_n == [400, 512] and len(s.pool_rooms) == 3, 'v3.36: the pools\' install lines and 3 room lines read')
    check('pools: tasks 400, IK 512; tasks at most 312 of 400 in use (2:05, fewest free 88  [11:02:00]); slots 300+ used in 1 of 3 '
          'room line(s); IK at most 90 of 512 (2:05); actor records fewest free 201; heap at most 9400 units' in r,
          'v3.36: the pools summary line')
    s, r = run(V336H)  # v3.36 HE336 (MERGE: V336H)
    print(r)
    check(len(s.hits) == 2 and len(s.recs) == 2, 'v3.36: the hit-shape and enemy-record room lines parse')
    check('hit shapes: 2 room line(s); lowest free 0 of 100 in 2:4; refusals 7, stomp guard 1' in r, 'v3.36: the hit-shape summary')
    check("enemy records: 2 room line(s); most in use 64 of 64 (2:5); evictions: 20 copies' record(s), 1 real (1 for a copy's write); "
          "40 copies' record(s) purged" in r, 'v3.36: the enemy-record summary')
    s, r = run(V337)  # v3.37 MN337
    print(r)
    check(s.drop_caps == (64, 10, 16), 'v3.37: the caps read from the install line (%r)' % (s.drop_caps,))
    check('supply drops (caps 64 anywhere / 10 a room, 16 records kept free): 3 given, 1 faded, 1 made way at the lying cap; '
          'refused: 1 room cap, 1 lying cap, 0 too few records (most lying named 64); numbers renumbered 1 time(s)' in r,
          'v3.37: the Supply Drops summary line')
    check('supply drops level: Generous (= Abundant since v3.38); given by group: common Drink 1 / Pistol 0 / Revolver 0, '
          'uncommon Kit 0 / Candle 1, rare Medallion 0 / Ampoule 1 / Silver 0 / Sword 0' in r,
          'v3.38 DR338: a v3.37 log: its Generous named as Abundant, the drops per group')
    s, r = run(V338)  # v3.38 DR338
    print(r)
    check(s.drop_level == 'Abundant' and s.drop_alias == 1 and s.drop_equal and
          s.drop_chances == ({'Rare': 0.5, 'Normal': 1.0, 'Abundant': 2.0}, [('weak', 11.45, 0.5, 0.05), ('strong', 9.9, 2.0, 0.1),
                                                                              ('ghost', 7.75, 4.0, 0.25), ('named ghost', 5.0, 4.0, 3.0),
                                                                              ('Walter', 7.0, 4.0, 1.0)], 12.0) and
          s.drop_equal == 'common 1:1:1, uncommon 2:1, rare 1:1:1:1',  # v3.38 DR338c
          'v3.38 DR338: the level, the alias, the levels line and the shares line read (%r)' % (s.drop_chances,))
    check(s.drop_changes == [('12:02:00', 'Rare'), ('12:03:00', 'Normal')], 'v3.38 DR338: the menu change and the ini re-read (%r)' % (s.drop_changes,))
    check('supply drops level: Abundant (x2 of Normal: any item 24 % a roll; common / uncommon / rare: weak 22.9 / 1 / 0.1 %, strong 19.8 / 4 / '
          '0.2 %, ghost 15.5 / 8 / 0.5 %, named ghost 10 / 8 / 6 %, Walter 14 / 8 / 2 %); shares common 1:1:1, uncommon 2:1, rare 1:1:1:1; the ini\'s '
          'old "Generous" read as Abundant; changed: Rare at 12:02:00, Normal at 12:03:00; given by group: common Drink 1 / Pistol 1 / '
          'Revolver 2, uncommon Kit 0 / Candle 0, rare Medallion 0 / Ampoule 0 / Silver 1 / Sword 1' in r,
          'v3.38 DR338: the Supply Drops level line')
    check('supply drops (caps 64 anywhere / 10 a room, 16 records kept free): 6 given' in r, 'v3.38 DR338: the caps line still reads the v3.38 install line')
    s, r = run(V337P)  # v3.37 CP337
    print(r)
    check(len(s.perf) == 2 and s.backend == 'Direct3D: GOG -> dgVoodoo2 D3D12', 'v3.37: the perf summary lines and the Direct3D line parse')
    check('perf overlay: 2 summary line(s), 0:07:00; 51.4 fps on average; the lowest 1% low 29.0 fps  [10:12:00]; worst frame 60.5 ms  '
          '[10:12:00]; Refined at most 2.05 ms a frame; address space at most 910 of 2048 MB' in r, 'v3.37: the perf overlay summary')
    check('pause Camera page: opened 1 time(s); last closed with camera OTS, distance 600 side 25' in r, 'v3.37: the pause Camera page line')
    s, r = run(V338H)  # v3.38 HB338
    print(r)
    check(len(s.hb_hide) == 1 and s.hb_ring['faded out'] == 2 and len(s.perf_place) == 2, 'v3.38: the Hide Gauge and readout lines parse')
    check('perf readout place: 2048x1536 under the PE HUD; 1920x1080 corner the stock gauge' in r, 'v3.38: the readout place summary')
    check('curse meter Hide Gauge: on (since 10:00:01); faded out 2 time(s), shown again 2 (own wake 1, the health gauge 1)' in r,
          'v3.38: the Hide Gauge summary')
    s, r = run(V352HB)  # v3.52 HB352
    print(r)
    check(s.hb_ref_lines == {'hidden': 2, 'place': 1} and s.hb_ref['rooms'] == 2 and s.hb_ref['hidden'] == 3 and s.hb_ref['place'] == 1 and
          s.hb_ref_tot == (3, 1) and len(s.hb_boss) == 2 and s.hb_boss[0][3] == 0.2 and s.hb_boss[1][4] == 'walled off from the camera',
          'v3.52: the refused-bar and boss lines parse (%r %r %r %r)' % (dict(s.hb_ref_lines), dict(s.hb_ref), s.hb_ref_tot, s.hb_boss))
    check('enemy health bars: no bar for 3 enemy(ies) the game does not draw yet (hidden, arriving or a placeholder) and 1 with no place '
          'in the room (2 room line(s), 3 verbose line(s); first FFFF0007 type 0B in room 5:C [10:00:01]); bosses: bar up 1 time(s) (at '
          'most 0.2 s after it became active; first Walter in room 4:1F [10:01:00]), no bar 5 s after it became active 1 time(s) (last: '
          'walled off from the camera, Walter in room 4:22 [10:02:00])' in r, 'v3.52: the enemy health bars summary')
    s, r = run(V333_END)
    check(not s.hb_ref_lines and not s.hb_boss and 'enemy health bars:' not in r, 'v3.52: no refused-bar lines, no summary line')
    s, r = run(V341C)  # v3.41 CM341
    print(r)
    check(s.hb_pe_ring and s.hb_pe_ring[1] == 'pe', 'v3.41: the player curse ring owner line parses')
    check("player curse ring: drawn by Playable Eileen (SH4PE_DrawsPlayerCurse); Refined draws companion Eileen's ring only  "
          "[10:00:01]" in r, 'v3.41: the player curse ring summary (Playable Eileen)')
    s, r = run(V341R)
    check(s.hb_pe_ring and s.hb_pe_ring[1] == 'refined' and
          'player curse ring: drawn by Refined (SH4PE_DrawsPlayerCurse not exported: Playable Eileen up to v1.23.0)  [10:00:02]' in r,
          'v3.41: the player curse ring summary (Refined, old Playable Eileen)')
    s, r = run(V333_END)
    check(s.hb_pe_ring is None and 'player curse ring' not in r, 'v3.41: no owner line, no summary line')
    s, r = run(V342S)  # v3.42 SM342
    print(r)
    check(s.hb_cm42 and s.hb_cm42_old == ('Both', 'On') and s.r302 == 'off' and s.r302_n == 1 and s.vg['echo'] == 2 and
          s.vg['after shock'] == 1 and s.vg_shown[0] == 2 and len(s.sh_dark) == 2 and len(s.sh_dark_use) == 2,
          'v3.42: the Curse Meter, Room 302, view gate and Darkness lines parse')
    check("player curse ring: never drawn by Refined (v3.42 Curse Meter Off / On: companion Eileen's ring only); Playable Eileen "
          "draws it (SH4PE_DrawsPlayerCurse); the ini's old CurseMeter=Both read as On  [10:00:01]" in r, 'v3.42: the curse ring summary (PE)')
    check('death presence in Room 302: off (the hauntings as stock); 1 entry' in r, 'v3.42: the Room 302 summary')
    check('effects hidden in views: after shock 1, echo 2 (the room is not drawn (a view, a load or a menu) x2, the Room 302 peeping '
          'flag x1); shown again 2 time(s), the longest hide 1.5 s [10:00:04]' in r, 'v3.42: the view gate summary')
    check('Darkness 85 % [10:00:00], then 70 % [10:00:07] (Soft 4B000000 85 %: inside 0.426 of 0.502, Stock 32000000 70 %: inside '
          '0.247 of 0.354)' in r, 'v3.42: the Darkness in the shadows summary')
    s, r = run(V342R)
    check("player curse ring: never drawn by Refined (v3.42 Curse Meter Off / On: companion Eileen's ring only); nobody draws it "
          "(SH4PE_DrawsPlayerCurse not exported: Playable Eileen up to v1.23.0)  [10:00:02]" in r and
          'death presence in Room 302: on ([ScreenEffects] DeathPresenceRoom302=1)' in r and 'effects hidden in views' not in r,
          'v3.42: the curse ring summary (old PE), Room 302 on, no view line')
    s, r = run(V343S)  # v3.43 SH343
    print(r)
    check(len(s.sh_c1) == 5 and s.sh_c1[('play', '4B000000 55 %')] == (0.276, 72, True, '10:00:10') and
          s.sh_c1[('play', '44070707 55 %')][2] is False and s.sh_c1[('page', '4B000000 55 %')][0] == 0.276,
          'v3.43: the c1 read-back lines parse (play, page, not read back)')
    check('; c1.a drawn: page 4B000000 55 %: 0.276, 72 % light [10:00:09], in play 4B000000 55 %: 0.276, 72 % light [10:00:10], '
          'in play 44070707 55 %: 0.254 (not read back), 75 % light [10:00:11], page 32000000 70 %: 0.248, 75 % light [10:00:12] '
          'MISMATCH with the play 0.354, in play 32000000 70 %: 0.354, 65 % light [10:00:13] MISMATCH with the page 0.248' in r,
          'v3.43: the c1 summary (side by side, a mismatch named)')
    check('the in-play composite: c1.a read back' in r, 'v3.43: the read-back anomaly is listed')
    s, r = run(V342S)
    check(not s.sh_c1 and 'c1.a drawn' not in r, 'v3.43: no c1 lines, no c1 summary')
    s, r = run(V343G)  # v3.43 GT343
    print(r)
    check(s.gt == ('in', 'On') and s.gt_warps['03'] == 1 and s.gt_warps['0B'] == 1 and s.gt_cool == 1 and
          s.gt_no['no standing enemy within 2500 u'] == 2 and s.gt_no['Room 302'] == 1, 'v3.43: the ghost teleport lines read')
    check('ghost teleport: in at the start (On); 2 warp(s) (types 03 x1, 0B x1); refused 3: no standing enemy within 2500 u x2, '
          'Room 302 x1; 1 press(es) waited for the cooldown' in r, 'v3.43: the ghost teleport summary line')
    s, r = run(V344G)  # v3.44 GT344
    print(r)
    check(s.gt == ('in', 'On') and s.gt_warps['03'] == 1 and s.gt_warps['ahead'] == 1 and s.gt_cool == 1 and
          s.gt_chord['taken back'] == 2 and s.gt_chord['Action first'] == 1 and s.gt_fades == [3, 1] and
          s.gt_off['she is hit or held'] == 1 and s.gt_no['no free floor ahead of her in this room'] == 1 and
          s.gt_no['Room 302'] == 1, 'v3.44: the ghost teleport v3.44 lines read')
    check('ghost teleport: in at the start (On); 2 warp(s) (types 03 x1, ahead x1); refused 2: no free floor ahead of her in this '
          'room x1, Room 302 x1; 1 press(es) waited for the cooldown; 3 chord(s): Action last 2 (taken back 2), Action first 1; '
          "Richard's fade 3 time(s), 1 NEVER DRAWN on her body; called off 1: she is hit or held x1" in r,
          'v3.44: the ghost teleport summary line (chords, fades, called off)')
    s, r = run(V349G)  # v3.49 GT349
    print(r)
    check(s.gt == ('in', 'On') and s.gt_warps['ahead'] == 2 and s.gt_chord['taken back'] == 1 and s.gt_chord['held'] == 1 and
          s.gt_holds == [(2, 3)] and s.gt_zones == {'2:2': 1, '2:3': 0} and s.gt_zone_no == 2 and
          s.gt_no['a door is in use'] == 1 and s.gt_no['the screen is fading'] == 1 and
          s.gt_no['no free floor behind the nearest 3 enemy(ies) in this room'] == 1, 'v3.49: the ghost teleport v3.49 lines read')
    check('1 chord(s): Action last 1 (taken back 1), Action first 0, 1 held warp start(s)' in r and
          '1 hold(s) of Run + Action: 2 warp(s), the longest 2 in a row, 3 try(ies) refused while held' in r and
          'walk-in event zones kept out of: room 2:2 x1 (2 refusal(s) had spots inside or past one)' in r,
          'v3.49: the ghost teleport summary (v3.48 chord lines counted, the hold, the zones)')
    s, r = run(V350G)  # v3.50 TP350
    print(r)
    check(s.gt == ('in', 'On') and s.gt_warps['through'] == 1 and s.gt_warps['0A'] == 1 and s.gt_faced == 1 and
          s.gt_no['no free floor behind the enemy she faces'] == 1 and s.gt_no['no free floor ahead of her in this room'] == 1 and
          s.gt_rooms == [('2:4', 2, 1, 1, 0, 2, 1), ('2:7', 0, 0, 0, 0, 1, 0)] and
          dict(s.gt_room_no) == {'behind the faced enemy': 1, 'ahead': 1, 'her state': 1} and
          s.gt_maps == [('2:4', 4120, 8, 0, 9.4)] and s.gt_map_open == 1, 'v3.50: the ghost teleport v3.50 lines read')
    check('2 warp(s) (types 0A x1, through x1)' in r and '; 1 behind the enemy she faces' in r and
          '2 room visit(s) with tries: 2 warp(s) (1 through walls, 1 behind the faced enemy, 0 ahead), 3 refused (behind the faced '
          'enemy x1, ahead x1, her state x1), 1 Action / Check press(es) taken back in fade-outs' in r and
          'floor maps: 1 finished (4120 cells at most, 9.4 ms at most), 1 room visit(s) left with it NOT finished' in r,
          'v3.50: the ghost teleport summary (through walls, the faced enemy, room visits, floor maps)')
    s, r = run(V349G)  # (v3.50 TP350: a v3.49 log reads as before: no room / map parts)
    check('room visit(s) with tries' not in r and 'floor maps:' not in r and s.gt_faced == 0, 'v3.50: a v3.49 log: no v3.50 parts')
    s, r = run(V350G)  # v3.51 TP351: a v3.50 log reads as before (no skip part)
    check(s.gt_skip == 0 and s.gt_skip_tries == 0 and 'skipped behind collision' not in r and
          s.gt_rooms == [('2:4', 2, 1, 1, 0, 2, 1), ('2:7', 0, 0, 0, 0, 1, 0)] and s.gt_map_open == 1,
          'v3.51: a v3.50 log: the room lines read as before, no skips')
    s, r = run(V351G)
    print(r)
    check(s.gt == ('in', 'On') and s.gt_warps['through'] == 1 and s.gt_warps['0A'] == 1 and s.gt_faced == 1 and
          s.gt_no['no free floor ahead of her in this room'] == 1 and s.gt_skip == 4 and s.gt_skip_tries == 3 and
          s.gt_rooms == [('2:4', 2, 1, 1, 0, 1, 0), ('2:7', 0, 0, 0, 0, 1, 0)] and
          dict(s.gt_room_no) == {'ahead': 1, 'her state': 1} and s.gt_map_open == 0,
          'v3.51: the skips behind collision read (%r %r %r)' % (s.gt_skip, s.gt_skip_tries, s.gt_rooms))
    check('2 room visit(s) with tries: 2 warp(s) (1 through walls, 1 behind the faced enemy, 0 ahead), 2 refused (ahead x1, her '
          'state x1), 0 Action / Check press(es) taken back in fade-outs; 4 enemy(ies) in the cone skipped behind collision (in 3 '
          'try(ies))' in r, 'v3.51: the ghost teleport summary counts the skips behind collision')
    s, r = run(V352G)  # v3.52 TP352
    print(r)
    check(s.gt == ('in', 'On') and s.gt_warps['09'] == 1 and s.gt_warps['ahead'] == 1 and s.gt_faced == 1 and
          dict(s.gt_free) == {'after warp': 1, 'at the warp': 1} and s.gt_free_no == 1 and s.gt_room_freed == 2 and
          s.gt_rooms == [('4:17', 2, 0, 1, 1, 1, 0)] and s.gt_maps == [('4:17', 1624, 9, 0, 12.6)] and s.gt_map_open == 0 and
          s.gt_no['no free floor behind the enemy she faces'] == 1,
          'v3.52: the landing proof and freed-from-a-wall lines read (%r %r %r)' % (dict(s.gt_free), s.gt_free_no, s.gt_rooms))
    check('; 2 time(s) freed from a wall' in r and 'freed from a wall: 1 after a warp, 1 at a warp from inside one, 1 time(s) LEFT '
          'inside collision (not moved)' in r, 'v3.52: the ghost teleport summary counts the frees')
    s, r = run(V351G)  # v3.52 TP352: a v3.51 log reads as before (no freed part)
    check(s.gt_room_freed == 0 and not s.gt_free and s.gt_free_no == 0 and 'freed from a wall' not in r and s.gt_skip == 4 and
          s.gt_rooms == [('2:4', 2, 1, 1, 0, 1, 0), ('2:7', 0, 0, 0, 0, 1, 0)], 'v3.52: a v3.51 log: the room lines read as before')
    s, r = run(V337I)  # v3.37 INV337
    print(r)
    check(s.inv == ('on', 'off', 'in') and s.inv_sw['extended toolbelt stays on'] == 1 and s.inv_sw['extended toolbelt off'] == 1,
          'v3.37: the inventory install line and the switches read')
    check('inventory: at the start toolbelt on, stackable off, stack guards in; switches: extended toolbelt off x1, extended toolbelt '
          'stays on x1, stackable items on x1; last counters: flips 12, overflows 3, rounds from page 2 40, cross-page reloads 4, '
          're-equips 2; stacks: merges 9, take-outs 2, stacked breaks 1, dropped 0' in r, 'v3.37: the inventory summary line')
    s, r = run(V338F)  # v3.38 EN338
    print(r)
    check(s.fol['doors'] == 2 and s.fol['follow'] == 8 and s.fol['most'] == 5 and s.fol['came'] == 2 and s.fol['wait'] == 1 and
          s.fol['stopped'] == 1 and s.fol['stops'] == 1 and s.fol['spotted'] == 2 and s.fol['story'] == 1 and s.fol['wheelchairs'] == 1,
          'v3.38: the Enemies Follow lines read (%r)' % (dict(s.fol),))
    check('enemies follow: 2 door(s), 8 follower(s) in all (at most 5 at one door), 2 came through the door, 1 waited for a later '
          'door, 1 pursuit(s) ended at doors + 1 elsewhere; 2 spotted-you mark(s); 1 story-rule refusal(s); waits for space: hit '
          'shapes (24 kept free) x1, the room\'s enemy list is full (40) x1; 1 v3.37 Wheelchair stray(s) sent home' in r,
          'v3.38: the Enemies Follow summary line')
    s, r = run(V341G)  # v3.41 EN341
    print(r)
    check(s.fol['doors'] == 1 and s.fol['came'] == 1 and s.fol['fresh'] == 2, 'v3.41: the ghost copies made without their record read (%r)'
          % (dict(s.fol),))
    check('enemies follow: 1 door(s), 2 follower(s) in all (at most 2 at one door), 1 came through the door, 0 waited for a later '
          'door, 0 pursuit(s) ended at doors + 0 elsewhere; 0 spotted-you mark(s); 0 story-rule refusal(s); 2 ghost copy(ies) made in '
          'another room without their record (the ghosts\' way)' in r, 'v3.41: the Enemies Follow summary names them')
    s, r = run(V339R)  # v3.39 PR339
    print(r)
    check(s.rend == ('dgVoodoo.conf is Direct3D 11 (16-bit) (d3d11fd_rt16, C7E9253A95A07A1466306F417978204A)', 'open', 'offered') and
          s.rend_sw == [('10:02:00', 'Direct3D 11 (16-bit)', 'Direct3D 12')] and len(s.rend_fail) == 1,
          'v3.39: the renderer start, replace and refusal lines read (%r %r %r)' % (s.rend, s.rend_sw, s.rend_fail))
    check('renderer: at the start dgVoodoo.conf is Direct3D 11 (16-bit) (d3d11fd_rt16, C7E9253A95A07A1466306F417978204A) (row open, '
          'offered); replaced: Direct3D 11 (16-bit) -> Direct3D 12 [10:02:00]; NOT replaced 1 time(s): dgVoodoo.conf could not be '
          'replaced (error 5) [10:01:00]' in r, 'v3.39: the renderer summary line')
    check('opening: Skip Logos; the boot screens skipped 0.9 s after the start; saved for the next start: Fast [10:02:00]' in r,
          'v3.39: the opening summary line')
    check(s.backend == 'Direct3D: GOG -> dgVoodoo2, Direct3D 11 (16-bit)' and len(s.perf) == 1,
          'v3.39: the Direct3D line names the variant; a summary with the readout off still parses')
    s, r = run(V343D)  # v3.43 DF343
    line = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    check(len(line) == 1, 'v3.43 DF343: one depth-of-field line')
    if line:
        L = line[0]
        check('Always Low [10:00:00], then Cut-scenes Low [10:01:30], then Cut-scenes High [10:01:31]' in L, 'v3.43 DF343: the settings in order: ' + L)
        check('drawn in 3500 of 3820 asked frame(s) over 2 room line(s)' in L, 'v3.43 DF343: frames and rooms: ' + L)
        check('1.250 ms at most (room 2:1, 0.420 on average)' in L, 'v3.43 DF343: the slowest room: ' + L)
        check('probes: flat 1, ok 1 (off in 2:2 (flat))' in L, 'v3.43 DF343: the probes: ' + L)
        check('1 pause copy(ies)' in L and 'OFF from 10:03:00: the scene could not be copied (CopyRects)' in L, 'v3.43 DF343: copies, off: ' + L)
    s, r = run(V344D)  # v3.44 DF344
    line = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    check(len(line) == 1, 'v3.44 DF344: one depth-of-field line')
    if line:
        L = line[0]
        check('drawn in 3000 of 4092 asked frame(s) over 3 room line(s)' in L, 'v3.44 DF344: frames and rooms: ' + L)
        check("not drawn: the player's position could not be projected 946, the player is behind the camera or too far 136, "
              "the room's depth probe is pending 10" in L, 'v3.44 DF344: the reasons, most first: ' + L)
        check("skipped by design: Room 302's first-person view 900, paused or an overlay 450" in L, 'v3.44 DF344: the skips: ' + L)
        check('1 pause copy(ies)' in L, 'v3.44 DF344: the Screen Effects preview copy counts: ' + L)
    s, r = run(V345D)  # v3.45 QS345
    line = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    check(len(line) == 1, 'v3.45 QS345: one depth-of-field line')
    if line:
        L = line[0]
        check('Always Medium [10:00:00], then Always Low [10:01:30], then Cut-scenes Low [10:01:31], then Cut-scenes High [10:01:32]' in L,
              'v3.45 QS345: Medium and the new Low read: ' + L)
        check("the ini's v3.44 DofStrength=Low read as Medium (rewritten)" in L, 'v3.45 QS345: the migration: ' + L)
    s, r = run(V347D)  # v3.47 DF347
    line = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    check(len(line) == 1 and 'eased in (Low 4+4, Medium 8+4, High 12+8 slices)' in line[0] and 'NOT eased' not in line[0],
          'v3.47 DF347: eased in, the ramps read: %r' % line)
    s, r = run(V347DN)
    line = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    check(len(line) == 1 and 'NOT eased in (no blend shader 1 time(s): whole levels)' in line[0],
          'v3.47 DF347: the fallback without the blend shader is named: %r' % line)
    check(s.dof_first == 'whole levels' and s.dof_noblend == 1, 'v3.47 DF347: the fallback lines parse (%r %r)' % (s.dof_first, s.dof_noblend))
    s, r = run(V344D)  # v3.45 QS345: no migration line, none in the summary
    check('DofStrength=' not in r, 'v3.45 QS345: no migration note without the line')
    s, r = run(V345R)  # v3.45 RB345
    rb = [x for x in r.splitlines() if x.startswith('  read back: ')]
    check(s.rb == ('10:00:01', 'ok', 'a lockable render target', 'a pixel check, 8x2') and len(s.rb_sw) == 2,
          'v3.45 RB345: the read back path and the switch read (%r %r)' % (s.rb, s.rb_sw))
    check(len(rb) == 1 and rb[0].startswith('  read back: through a lockable render target [10:00:01] (first: a pixel check, 8x2); '
                                            '2 switch(es) / failure(s) of a path that worked: lost a lockable render target [10:00:03], '
                                            'switch a system-memory image surface [10:00:03]'), 'v3.45 RB345: the read back summary: %r' % rb)
    check(s.dof_probe == {'ok': 1} and s.sh_px and s.sh_px[0][3] == 'ok', 'v3.45 RB345: the probe via the new path reads as before (%r)'
          % (dict(s.dof_probe),))
    s, r = run(V345N)  # v3.45 RB345: none on this backend
    rb = [x for x in r.splitlines() if x.startswith('  read back: ')]
    check(s.rb and s.rb[1] == 'none' and rb and rb[0].startswith('  read back: NONE on this backend [10:00:03]: pixel checks off, depth '
                                                                 'probe off (a lockable render target: the copy (CopyRects) failed'),
          'v3.45 RB345: none on this backend: %r' % rb)
    line = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    check(line and 'probes: no read back 1' in line[0] and 'not probed (no read back) in 2 room(s)' in line[0],
          'v3.45 RB345: the probe counted honestly: %r' % line)
    s, r = run(V343D)  # v3.44 DF344: v3.43's room lines (no reasons) still read
    line = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    check(line and 'not drawn:' not in line[0] and 'drawn in 3500 of 3820' in line[0], 'v3.44 DF344: an old log reads as before')
    s, r = run(V340S)  # v3.40 SH340
    print(r)
    check(s.sh['composites'] == 4800 and s.sh['soft'] == 4800 and s.sh['rooms'] == 2 and s.sh_worst[:2] == [2, 0.9] and
          s.sh['kept'] == 1 and s.sh['scene'] == 1 and [e for _, e in s.sh_edge] == ['Soft', 'Softer'],
          'v3.40: the Shadow Edge lines read (%r %r %r)' % (dict(s.sh), s.sh_worst, s.sh_edge))
    check('shadows: Edge Soft [10:00:00], then Softer [10:02:14]; plans: Soft 2560x1440 1 pass(es) to 1280x720, taps 0.85: 6.0 px, '
          'Softer 2560x1440 2 pass(es) to 640x360, taps 0.82: 12.0 px; 4800 composite(s) in 2 room line(s), 4800 soft, at most 2 in a '
          'frame; ours 0.900 ms at most (room 1:03, 0.060 on average); render targets at most 3.5 MB; preview: 1 pause copy(ies), page '
          'shown the scene 1 / the figure 0 time(s); OFF from 10:05:00: a render target could not be set (the soft composite)' in r,
          'v3.40: the Shadow Edge summary line')
    s, r = run(V341S)  # v3.41 SH341
    print(r)
    check(s.sh_old == 'Softer' and s.sh_copy == [1, 1, 0.31] and s.fx_pic == [1, 1, 0.42, 1, 31.5] and s.sh['kept'] == 2 and
          s.sh['keep'] == 1 and [e for _, e in s.sh_edge] == ['Soft'], 'v3.41: the new Soft lines read (%r %r %r)' % (s.sh_copy, s.fx_pic, dict(s.sh)))
    check('shadows: Edge Soft [10:00:00] (the ini\'s v3.40 Edge=Softer read as Soft); plans: Soft 2560x1440 to 320x180 (3 level(s), '
          '1 blur pass(es), 7 draws, at most 2x): 24.0 px (asked 24.0); 1200 composite(s) in 1 room line(s), 1200 soft, at most 1 in a '
          'frame; ours 0.300 ms at most (room 1:02, 0.050 on average); render targets at most 33.0 MB; preview: 2 pause copy(ies), page '
          'shown the scene 1 / the figure 0 time(s) (targets made 1 time(s), kept 1; the slowest copy 0.31 ms); pause picture: 2 on the '
          'GPU (target made 1, kept 1; at most 0.42 ms), 1 read back from the GPU (worst 31.5 ms)' in r, 'v3.41: the Shadow Edge summary line')
    s, r = run(V349S)  # v3.49 SH349
    print(r)
    check(s.sh_rib_state == 'the Darkness' and s.sh['rib'] == 640 and s.sh['rib_dark'] == 600 and s.sh['rib_rooms'] == 1 and
          s.sh['composites'] == 1200 and s.sh['rooms'] == 1 and list(s.sh_rib_vals) == ['30 -> 29 at 85 %', '1A -> 16 at 85 %'],
          'v3.49 SH349: the coverage and Tremer lines read (%r %r %r)' % (s.sh_rib_state, dict(s.sh), s.sh_rib_vals))
    check('; Tremer wall shadows: the Darkness, 640 draw(s) in 1 room line(s), 600 at the Darkness (alpha 30 -> 29 at 85 %: 85 %, '
          '1A -> 16 at 85 %: 85 %)' in r, 'v3.49 SH349: the Tremer part of the shadows summary')
    s, r = run(V349SOFF)
    print(r)
    check(s.sh_rib_state == "the game's own (TremerShadows=0)" and not s.sh['rib'] and
          "; Tremer wall shadows: the game's own (TremerShadows=0)" in r, 'v3.49 SH349: TremerShadows=0 (%r)' % s.sh_rib_state)
    check(not s.sh_rooms and not s.sh_ns and 'HARD share' not in r and 'no darkness' not in r, 'v3.52 SH352: no room lines: no share parts')
    s, r = run(V352S)  # v3.52 SH352: why composites were not soft, the rooms with a hard share
    print(r)
    check(s.sh_rib_state == 'the Darkness' and s.sh['composites'] == 5898 and s.sh['soft'] == 1588 and s.sh['hard'] == 403 and
          s.sh['nodark'] == 2769 and s.sh['unexpl'] == 1138 and s.sh_rooms['4:1F'] == [3907, 0, 0, 2769, 1138, {}] and
          s.sh_rooms['4:22'][:5] == [1591, 1588, 3, 0, 0] and dict(s.sh_rooms['4:22'][5]) == {'render targets': 1, 'failed': 2} and
          dict(s.sh_rooms['2:01'][5]) == {'Stock': 400} and [(x[1], x[2].split(' (')[0]) for x in s.sh_ns] ==
          [('4:1F', "the game's darkness is 0 here"), ('4:22', 'our render targets could not be made this time: the game\'s own hard edge, soft again at the next composite')],
          'v3.52 SH352: the tails and the reason lines read (%r %r %r)' % (dict(s.sh), s.sh_rooms, s.sh_ns))
    check('; HARD share: 403 composite(s) in 2 room(s): 2:01 400 of 400 (100 %: Stock 400), 4:22 3 of 1591 (0 %: failed 2, render '
          'targets 1)' in r and "; 2769 with no darkness (the game's own 0: no shadow shows, nothing to soften) in room(s) 4:1F" in r and
          "; 1138 not soft with no reason given (a log before v3.52) in room(s) 4:1F 1138 of 3907 (the game's own darkness is 0 there: "
          "no shadow shows)" in r and '; 2 not-soft reason line(s) (4:1F [10:01:00]: the game\'s darkness is 0 here, 4:22 [10:02:01]: our '
          'render targets could not be made this time)' in r, 'v3.52 SH352: the shadows summary names the hard rooms and why')
    s, r = run(V349S)
    check(not s.sh['hard'] and not s.sh['nodark'] and not s.sh['unexpl'] and 'HARD share' not in r and 'not soft' not in r,
          'v3.52 SH352: an all-soft log has no share parts (%r)' % dict(s.sh))
    s, r = run(V341W)  # v3.41 VW341
    print(r)
    check(s.vw.startswith("Room 302's window views keep the 4:3 view") and len(s.vw_views) == 2 and len(s.edges) == 2 and
          s.edges_free == 1, 'v3.41: the window-view and edge-cover lines read (%r %r %r)' % (s.vw, s.vw_views, s.edges))
    check("window views: Room 302's window views keep the 4:3 view with black sides in wide shapes (0054CC10 hooked); "
          "2 view(s) with black sides (321 | 320 at 2560x1440)" in r, 'v3.41: the window views summary line')
    check('borderless edges covered 2 time(s) by windows above the game (not the game\'s picture): right edge 2x1374 at 2558,15 '
          '(class Chrome_WidgetWin_1, its invisible border) [10:03:00, game not in front] | bottom edge 2560x2 at 0,1438 (class '
          'Shell_SecondaryTrayWnd, its frame) [10:05:00, game in front]; free again 1 time(s)' in r,
          'v3.41: the Borderless edges summary line')
    s, r = run(V342E)  # v3.42 EG342
    print(r)
    check(s.e2d == [('1024x768', 'aligned'), ('2560x1440', 'aligned')] and s.e2d_sides == {'2560x1440': '321 | 321'},
          'v3.42: the 2D edge lines read (%r %r)' % (s.e2d, s.e2d_sides))
    check('2D edges: canvases on pixel edges (column 0 / row 0 covered) at 1024x768, 2560x1440 (black sides 321 | 321)' in r,
          'v3.42: the 2D edges summary line')
    s, r = run(V332)
    print(r)
    check('no session header' in r, 'a v3.32 log: says there is no header')
    check('gameplay: MoveSpeed x2' in r, 'a v3.32 log: the gameplay settings line instead')
    check('install / write warnings: 1' in r, 'a v3.32 log: the "is not the stock" line is a warning')
    check('rooms seen' in r and '2:04' in r, 'a v3.32 log: rooms from the features\' own room lines')
    check('200x screen effects: Spirit Vision: a downed ghost' in r, 'a v3.32 log: the noisy repeated line is named')
    s, r = run(V341D)  # v3.41 DP341
    print(r)
    check(s.dpg == {'the room is not drawn (a view, a load or a menu)': 1, 'the Room 302 peeping flag': 1, 'a room change': 1} and
          s.dpg_shown[:3] == [4, 70.25, '1:01'] and len(s.dpg_long) == 1 and s.dpg_warp == 1,
          'v3.41: the Death Presence gate lines read (%r %r %r)' % (dict(s.dpg), s.dpg_shown, s.dpg_long))
    check('death presence hidden: 3 time(s) (' in r and 'shown again 4 time(s), the longest hold 70.2 s in 1:01 [10:01:40]; past '
          '20 s 1 time(s) (the last: the room is not drawn (a view, a load or a menu) in 1:01 [10:00:52]); 1 pending move(s) no '
          'longer waited for' in r, 'v3.41: the Death Presence gate summary line')
    check('2:01' in s.old_rooms, 'v3.41: the fog\'s room line still names its room')
    s, r = run(V343P)  # v3.43 PP343
    print(r)
    check(s.pp == ('hooked', 'Low', 'Off', 'On') and s.pp_set == [('10:00:01', 'Grading', 'Apartment')] and
          s.pp_lens['in'] == 2 and s.pp_lens['out'] == 1 and s.pp_lens_max == [100, '3:5'] and s.pp_chain == {'2560x1440': 4.7} and
          len(s.pp_fail) == 1 and s.pp_notf['colour grading under Thermal'] == 1 and s.pp_drawn['bloom Low'] == 1,
          'v3.43: the post effects lines parse (%r %r %r %r)' % (s.pp, s.pp_set, s.pp_lens_max, s.pp_fail))
    check('post effects: at the start Bloom Low, Colour Grading Off, Otherworld Lens On (device reset hook hooked); changes: Grading '
          'Apartment [10:00:01]; drawn: bloom Low; bloom targets 2560x1440 4.7 MB; lens faded in 2 / out 1 time(s), at most 100 % (3:5); '
          'not under a filter: colour grading under Thermal x1; OFF this session: bloom (its render targets could not be made 3 times '
          'in a row) [10:00:11]' in r, 'v3.43: the post effects summary line')
    s, r = run(V344C)  # v3.44 CS344
    print(r)
    check(s.pp == ('hooked', 'Very High', 'PS2', 'Off') and s.pp_set == [('10:00:01', 'Bloom', 'Very High')] and
          s.pp_drawn['bloom Very High'] == 1 and s.ps_lint, 'v3.44: Very High parses (%r %r %r)' % (s.pp, s.pp_set, dict(s.pp_drawn)))
    check(s.pp_px == [('10:00:01', 'PS2', 'ok', 1, 7.4, 7.5, 'the grade reaches the pixels'),
                      ('10:00:02', 'Bleach', 'FAILED', 22, 0.0, 14.0, 'the input came through unchanged: the grading shader did not reach the pixels'),
                      ('10:00:03', 'Otherworld', 'not run', 0, 0.0, 0.0, 'a test target could not be made')],
          'v3.44: the grade pixel checks parse (%r)' % (s.pp_px,))
    check(len(s.sh_px) == 3 and s.sh_px[0] == ('10:00:04', 'page', '4B000000 55 %', 'ok', 0.278, 0.276, 'the Darkness reaches the pixels') and
          s.sh_px[1][1:6] == ('play', '4B000000 25 %', 'FAILED', 1.0, 0.125) and s.sh_px[2][3] == 'not run',
          'v3.44: the shadow pixel checks parse (%r)' % (s.sh_px,))
    check('; grading pixel check 1 ok of 3 (Bleach FAILED: the input came through unchanged: the grading shader did not reach the pixels '
          '[10:00:02], Otherworld not run: a test target could not be made [10:00:03], PS2 ok, 1 off, 7.4 changed [10:00:01]); '
          'shaders: proven forms only' in r, 'v3.44: the grade pixel check summary')
    check('shadows: (no Edge line); pixel check 1 ok of 3 (play 4B000000 25 %: FAILED: the coverage itself came through: the composite shader did not run '
          '(no Darkness) [10:00:05], play 32000000 85 %: not run: the read back (CopyRects) failed [10:00:06], page 4B000000 55 %: '
          '0.278 of 0.276 [10:00:04])' in r, 'v3.44: the shadow pixel check summary')
    s, r = run(V343P)
    check(not s.pp_px and not s.sh_px and 'pixel check' not in r, 'v3.44: no pixel check lines, no pixel check summary')
    s, r = run(V347PX)  # v3.47 PX347
    print(r)
    check(s.px_align == [('10:00:01', 'shadows in play', -0.5, 3), ('10:00:02', 'colour grading PS2', -0.5, 6),
                         ('10:00:03', 'shadows in play', None, 0)], 'v3.47 PX347: the offset lines parse (%r)' % (s.px_align,))
    check(dict(s.px_src) == {'as asked': 2, 'lost': 1} and s.px_lost == [('10:00:03', 'pixel check (in play, Darkness 70 %)', 1)],
          'v3.47 PX347: the source lines parse (%r %r)' % (dict(s.px_src), s.px_lost))
    check(len(s.sh_px) == 2 and s.sh_px[0][3] == 'ok' and s.sh_px[1][3] == 'ok' and len(s.pp_px) == 1 and s.pp_px[0][2] == 'ok' and
          s.rb[3] == 'a pixel check, 32x4', 'v3.47 PX347: the check lines read as before (%r %r)' % (s.sh_px, s.pp_px))
    check('  pixel check pattern: the test draw samples 0.50 texel(s) left of the texel centres in 2 check(s) (shadows in play -0.50 '
          '[10:00:01], colour grading PS2 -0.50 [10:00:02]); 1 not measurable; the source: 2 as asked, 1 with a Clear that did not '
          'land, 0 not read back (pixel check (in play, Darkness 70 %): 1 cell(s) [10:00:03])' in r, 'v3.47 PX347: the pattern summary')
    check('ANOMALY' not in r.split('pixel check pattern')[1].split('\n')[0], 'v3.47 PX347: no anomaly from the new lines')
    s, r = run(V343O)
    check(s.pp_ini_off and 'post effects: off ([ScreenEffects] PostEffects=0)' in r, 'v3.43: PostEffects=0 summarised')
    s, r = run(V333_END)
    check(s.pp is None and 'post effects' not in r, 'v3.43: no post effects lines, no summary line')
    s, r = run(V343F)  # v3.43 FN343
    print(r)
    check(s.fn['set'] == ('EB Garamond', 'EBGaramond', 212, 212) and s.fn['live'] and s.fn['match'] == {0} and
          s.fn['nomatch'] == {1}, 'v3.43: the menu font lines read (%r)' % (s.fn,))
    check('menu font: EB Garamond (fonts\\EBGaramond.sh4font, 212 / 212 glyphs); HD glyph page live; fonts matching the '
          'sheet: 0; NOT matching (stock glyphs): 1' in r, 'v3.43: the menu font summary line')
    s, r = run(V343FS)
    check('menu font: STOCK ([Textures] MenuFont Fancy: fonts\\Fancy.sh4font not found (error 2))' in r,
          'v3.43: a missing sheet is shown as STOCK with its reason')
    s, r = run(V333_END)
    check('menu font' not in r, 'v3.43: no menu font line when MenuFont = Stock')
    s, r = run(V344FN)  # v3.44 FN344
    check(s.fn['row'] and s.fn['switches'] == ['EB Garamond', 'Crimson Text'] and s.fn['live'] and s.fn['set'] is None,
          'v3.44: the Menu Font row and its live switches read (%r)' % (s.fn,))
    check('menu font: Stock at the start; HD glyph page live; switched live in Options 2 time(s), last: Crimson Text' in r,
          'v3.44: the live switches summarised')
    s, r = run(V343F + '10:02:00.000 font: Menu Font -> Stock (Options, live): the stock glyph page is back (the HD page, 16 MB, '
                       'stays for a later switch)\n')
    check(s.fn['stock'] is None and 'switched live in Options 1 time(s), last: Stock' in r,
          'v3.44: a switch back to Stock is a switch, not a failure')
    s, r = run(V346O)  # v3.46 OM346
    print(r)
    check(s.om == ('in', 'Off', 0.30) and [x[1:] for x in s.om_sets] == [('Direction', 'Off', 'Options'), ('Analog', 'Direction', 'Options'),
          ('Off', 'Analog', '[Input] read again')], 'v3.46: the Stick Movement install and mode lines parse (%r %r)' % (s.om, s.om_sets))
    check(s.om_bind == [('10:00:05', 'the left stick (stock)', '0003 0001 0401 0403')] and
          s.om_checks == [('10:00:09', 1, 40, 40, 40, 40, 'GetDeviceState'), ('10:02:02', 2, 0, 0, 40, 40, 'call sites')],
          'v3.46: the bindings and self-check lines parse (%r %r)' % (s.om_bind, s.om_checks))
    check(s.om_counts == [5000, 1500, 12] and s.om_rechecks == [('10:02:00', 'GetDeviceState', 500, 600)] and s.om_anom == 1 and
          s.om_gate == ['6 / 2, overlay 2, phase 2'] and s.om_dz == [('10:02:30', 0.25)],
          'v3.46: the counts, the re-check, the gate states parse (%r %r %r)' % (s.om_counts, s.om_rechecks, s.om_gate))
    check('omni-directional movement: in at the start (Off, dead zone 0.30); 3 change(s): Direction (Options) [10:00:05], Analog (Options) '  # v3.48 OM348: the summary's name
          '[10:01:00], Off ([Input] read again) [10:03:00]; dead zone read again 0.25 [10:02:30]; bindings the left stick (stock) '
          '(0003 0001 0401 0403) [10:00:05]; self-check 2: source call sites (GetDeviceState 0/0, call sites 40/40) [10:02:02]; 1 '
          're-check(s) (last: GetDeviceState 500/600 [10:02:00]); written 5000 frame(s) (1500 from the call sites), 12 stopped '
          'inside the dead zone; pushed outside gameplay in 1 state(s): 6 / 2, overlay 2, phase 2; 1 ANOMALY line(s)' in r,
          'v3.46: the Stick Movement summary line')
    s, r = run(V346N)
    check(s.om == ('OFF', 'Direction', 0.30) and s.om_not == '005543C0 (call 00553D20)' and
          any('is not the stock code' in g[3] for g in s.warnings.values()),
          'v3.46: a refused site: parsed and kept in the warnings (%r %r)' % (s.om, s.om_not))
    check('omni-directional movement: OFF at the start (Direction, dead zone 0.30); no self-check yet (the stick not used in gameplay?); '  # v3.48 OM348
          '1 ANOMALY line(s)' in r, 'v3.46: the refused install summarised')
    s, r = run(V346B)
    print(r)
    check(s.om_bind[-1][1] == 'not a stick' and s.om_checks[-1][6] == 'none' and s.om_swap and
          any('omni: faulted' in g[3] for g in s.faults.values()), 'v3.46: other bindings, source none, swap, fault parse')
    check('bindings NOT A STICK (3580 3480 2001 2003) [10:00:01]; self-check 1: source NONE (GetDeviceState 3/40, call sites 2/40) '
          '[10:00:02]; swapped sticks without GetDeviceState: 8-way; 1 ANOMALY line(s)' in r, 'v3.46: the failure cases summarised')
    s, r = run(V347O)  # v3.47 CT347
    print(r)
    check(s.om_row == 'on Option 2, row 7 of 8' and s.om_pe == 'follows the push' and s.om_bg == [2, 304] and
          s.om_again == [1, 120] and s.om_anom == 1,
          'v3.47: the row, Playable Eileen, the focus-loss and recovery lines parse (%r %r %r %r)' % (s.om_row, s.om_pe, s.om_bg, s.om_again))
    check('; the row on Option 2, row 7 of 8; Playable Eileen follows the push; 2 focus loss(es) (GetDeviceState failed while the '
          'game was not in front, 304 frame(s): not a defect); the pad failed with the game in front 1 time(s) (120 frame(s)), then '
          'read again; 1 ANOMALY line(s)' in r, 'v3.47: the Stick Movement summary names the row, Playable Eileen and the focus losses')
    check(not any('focus lost' in g[3] for g in s.anomalies.values()), 'v3.47: a focus loss is not an anomaly')
    s, r = run(V347N)
    check(s.om_row == 'NONE (no Options pages)' and 'the row NONE (no Options pages)' in r and s.om_pe is None,
          'v3.47: no Options pages: no row, summarised (%r)' % (s.om_row,))
    s, r = run(V333_END)
    check(s.om is None and 'stick movement' not in r and 'omni-directional movement' not in r,
          'v3.46: no Stick Movement lines, no summary line')  # v3.48 OM348: + the new name
    s, r = run(V348O)  # v3.48 OM348
    print(r)
    check(s.om == ('in', 'On', 0.30) and [x[1:] for x in s.om_sets] == [('Off', 'On', 'Options'), ('On', 'Off', 'Options'),
          ('Off', 'On', '[Input] read again')] and s.om_row == 'on Option 2, row 7 of 8' and s.om_pe == 'follows the push',
          'v3.48: Off / On install, mode, row and Playable Eileen lines parse (%r %r %r %r)' % (s.om, s.om_sets, s.om_row, s.om_pe))
    check(s.om_old == [('10:00:00', 'Analog', 'written as On'), ('10:01:00', 'Direction', 'read as On')] and
          s.om_counts == [900, 0, 3] and s.om_checks[-1][6] == 'GetDeviceState',
          'v3.48: the old values (written back / read again) and the counts parse (%r %r)' % (s.om_old, s.om_counts))
    check('omni-directional movement: in at the start (On, dead zone 0.30); old value(s): Analog written as On [10:00:00], Direction read as On [10:01:00]; '
          '3 change(s): Off (Options) [10:00:06], On (Options) [10:00:07], Off ([Input] read again) [10:02:00]' in r and
          '; the row on Option 2, row 7 of 8; Playable Eileen follows the push' in r and 'stick movement:' not in r,
          'v3.48: the summary line says omni-directional movement, On / Off and the old values')
    check(any('is not Off or On' in k for k in s.kinds), 'v3.48: the unknown value is kept as its own line kind')
    s, r = run(V348F)
    print(r)
    check(s.om_old == [('10:00:00', '2', 'NOT WRITTEN (ini error)')] and s.om_row == 'NONE (no Options pages)' and
          s.om_bind[-1][1] == 'not a stick' and any('omni: faulted' in g[3] for g in s.faults.values()) and s.om_anom == 1,
          'v3.48: a failed write-back, no row, other bindings, the fault parse (%r %r)' % (s.om_old, s.om_row))
    check('old value(s): 2 NOT WRITTEN (ini error) [10:00:00]' in r and 'the row NONE (no Options pages)' in r and
          'bindings NOT A STICK' in r, 'v3.48: a failed write-back is summarised')
    s, r = run(V348W)
    check(s.om_old == [('10:00:00', 'Direction', 'NOT written (no ini)')] and
          s.om_row == "NONE (Option 2's table is not the stock one)" and 'old value(s): Direction NOT written (no ini) [10:00:00]' in r,
          'v3.48: the ini fallback (nothing written) and a non-stock Option 2 table (%r %r)' % (s.om_old, s.om_row))
    s, r = run(V349L)  # v3.49 LK349
    print(r)
    check(s.ol == 'Room302' and s.ol_sets == [('10:06:00', 'All', 'Room302')] and s.ol_bad == ['Sometimes'] and
          s.ol_bind == ('10:00:05', 'the right stick (stock)', '1403 1401 0801 0803') and s.ol_check == ('10:00:06', 40, 40, 'on'),
          'v3.49: the look start, read again, unknown value, bindings and self-check lines parse (%r %r %r %r %r)' % (
              s.ol, s.ol_sets, s.ol_bad, s.ol_bind, s.ol_check))
    check(s.ol_places == 2 and s.ol_n == [5300, 4900, 400, 300, 60, 30, 10, 0, 125, 85, 40, 720, 2000] and s.ol_room == [1, 120],
          'v3.49: the per-place count lines are summed (%r %r)' % (s.ol_n, s.ol_room))
    check("; look: OmniLook Room302, read again All [10:06:00], UNKNOWN value(s) Sometimes, self-check on (40/40) [10:00:06], 2 "
          "place line(s): pushed 5300 frame(s), 4900 analog, 400 the game's own (not here 300, key 60, no read 30, check 10, not a "
          "stick 0); reversal latch 125 frame(s) (x 85, y 40; Room 302 120); 8-way misses 720; turn kept 2000" in r,
          'v3.49: the look is summarised on the omni-directional movement line')
    check(any('is not Off, Room302 or All' in k for k in s.kinds), 'v3.49: the unknown OmniLook value is kept as its own line kind')
    s, r = run(V349LN)
    check(s.ol_bind[1] == 'not a stick' and s.ol_check[3] == 'none' and s.om_anom == 1 and
          ', bindings NOT A STICK (3180 3280 3380 3480), self-check NONE (3/40) [10:00:06]' in r,
          'v3.49: the look not on a stick, a rejected look self-check and its anomaly (%r %r)' % (s.ol_bind, s.ol_check))
    s, r = run(V346O)
    check(s.ol is None and '; look:' not in r, 'v3.49: a log without the look lines has no look part')
    s, r = run(V347SB)  # v3.47 SB347
    check(s.sb_row == ('Bank 2', 'EB Garamond', 58) and s.sb_clicks == {'left arrow': 2, 'right arrow': 1} and s.sb_stuck == 1,
          'v3.47: the Bank row draw and mouse click lines parse (%r %r)' % (s.sb_row, s.sb_clicks))
    check('bank row: "Bank 2" in Menu Font EB Garamond, 58 units wide; 3 mouse click(s): left arrow x2, right arrow x1 (1 at the '
          'first / last bank: no change)' in r, 'v3.47: the Bank row summary line')
    s, r = run(V333_END)
    check(s.sb_row is None and 'bank row' not in r, 'v3.47: no Bank row lines, no summary line')
    s, r = run(V347SB)  # v3.48 SB348: a v3.47 log's per-step lines (normal level) are still counted
    check(s.sb_steps == {'n': 2} and not s.sb_changes and
          '; 2 "save banks: bank N" line(s) at the normal level (Option 6, or a v3.47 or older list: one a step)' in r,
          'v3.48: the old normal per-step lines are read and named (%r)' % (s.sb_steps,))
    s, r = run(V348SB)  # v3.48 SB348
    check(s.sb_steps == {'v': 4, 'n': 1} and s.sb_clicks == {'right arrow': 1} and
          s.sb_changes == [('10:00:03', 3, 25, 'load', 4, 1, 1, 2, 2), ('10:00:09', 25, 24, 'load', 1, 1, 0, 0, 0)],
          'v3.48: the verbose steps and the settled changes parse (%r %r)' % (s.sb_steps, s.sb_changes))
    check('bank row: "Bank 3" in Menu Font EB Garamond, 55 units wide; 1 mouse click(s): right arrow x1; 2 bank change(s) in the '
          'lists: 3 -> 25 [10:00:03], 25 -> 24 [10:00:09] (5 step(s): keys 2, pad 1, mouse 2; by 10: 2); 1 "save banks: bank N" '
          'line(s) at the normal level (Option 6, or a v3.47 or older list: one a step); 4 verbose per-step line(s)' in r and
          'pad (' not in r.split('bank row:')[1].split('\n')[0], 'v3.48: the Bank row summary line')
    check(not any('save banks' in g[3] for g in s.warnings.values()), 'v3.48: no save banks line lands among the warnings')
    check(s.sb_bar is None and 'list bar' not in r, 'v3.49: a v3.48 log has no list bar line')
    s, r = run(V349SB)  # v3.49 SB349
    check(s.sb_bar == ('blue', "the game's blue menu bar, 56FCC0's colour; 56FDCF") and
          "bank row: (no draw line); list bar blue (the game's blue menu bar, 56FCC0's colour; 56FDCF)" in r,
          'v3.49: the blue list bar parses and is summarised (%r)' % (s.sb_bar,))
    s, r = run(V349SBY)
    check(s.sb_bar == ('yellow', "the game's own list bar; [SaveBanks] ListBar=Yellow") and
          'list bar yellow (the game' in r and 'NOT PATCHED' not in r, 'v3.49: ListBar=Yellow is no failure (%r)' % (s.sb_bar,))
    s, r = run(V349SBX)
    check(s.sb_bar == ('yellow', '56FDCF is not stock: nothing written') and
          'list bar YELLOW: NOT PATCHED (56FDCF is not stock: nothing written)' in r,
          'v3.49: a refused bar patch is summarised in capitals (%r)' % (s.sb_bar,))
    s, r = run(V347R)  # v3.47 RG347
    print(r)
    check(s.rg == ('in', 'x1', 15.0, 10, RG_WHO_PE) and s.rg_pe == 'with' and [w[1] for w in s.rg_who] ==
          [RG_WHO_PE, 'SH4 Refined (Henry)', 'nobody (Regeneration Off)', "SH4 Refined (Eileen: Playable Eileen's own regeneration is off)"],
          'v3.47: the Regeneration install, PE and who-heals lines parse (%r %r)' % (s.rg, s.rg_who))
    check(s.rg_seed == [('Regen', 'x1', "from Playable Eileen's [Defense] Regen 1 / RegenRate 15, as the v3.46 row showed it"),
                        ('RegenDelay', '10', "from Playable Eileen's [Defense] RegenDelay 10")], 'v3.47: the first start seed parses (%r)' % (s.rg_seed,))
    check(s.rg_runs == {'Henry': 3, 'Eileen': 1} and s.rg_stops == {'hurt': 1, 'near a ghost aura': 1, 'full': 1} and
          abs(s.rg_gain - 92.3) < 1e-6 and abs(s.rg_hurt - 12.3) < 1e-6 and s.rg_open == 1,
          'v3.47: the runs and their stops add up (%r %r %r %r %r)' % (s.rg_runs, s.rg_stops, s.rg_gain, s.rg_hurt, s.rg_open))
    check([x[1:] for x in s.rg_sets] == [('x2', 'x1', 'Options'), ('Off', 'x2', '[Gameplay] read again')] and
          s.rg_delay == [('10:05:10', 20)] and s.ls_route == [('10:01:20', 'direct')] and s.rg_anom == 1,
          'v3.47: the changes, the delay, the Life Steal route and the anomaly parse (%r %r %r)' % (s.rg_sets, s.rg_delay, s.ls_route))
    check("regeneration: in at the start (Regen x1 = 15 % a minute, RegenDelay 10 s; who: " + RG_WHO_PE + "); first start: "
          "Regen = x1 (from Playable Eileen's [Defense] Regen 1 / RegenRate 15, as the v3.46 row showed it), RegenDelay = 10 (from "
          "Playable Eileen's [Defense] RegenDelay 10); Playable Eileen with SH4PE_RegenActive; who heals changed 4 time(s), last: "
          "SH4 Refined (Eileen: Playable Eileen's own regeneration is off) [10:07:00]; 2 change(s): x2 (Options) [10:05:00], Off "
          "([Gameplay] read again) [10:05:20]; RegenDelay read again 20 s [10:05:10]; 4 healing run(s) (Eileen 1, Henry 3), +92.3 HP "
          "in the ended ones; stopped by: hurt 1, near a ghost aura 1, full 1 (hits 12.3 HP); 1 still running at the end of the log; "
          "Life Steal heals directly [10:01:20]; 1 ANOMALY line(s)" in r, 'v3.47: the Regeneration summary line')
    s, r = run(V347RN)  # v3.47 MERGE: w4 RG347's V347N renamed (w1 CT347 has a V347N)
    check(s.rg == ('OFF', 'x2', 30.0, 30, "nobody (Refined's regeneration is not installed)") and s.rg_pe == 'without' and
          s.rg_not == ['0052D1D0 (PlayerHealAmount) is not the stock code'] and
          any('is not the stock code' in g[3] for g in s.warnings.values()) and
          any('regeneration: faulted' in g[3] for g in s.faults.values()),
          'v3.47: a refused install, an older PE and a fault parse (%r %r)' % (s.rg, s.rg_not))
    check("regeneration: OFF at the start (Regen x2 = 30 % a minute, RegenDelay 30 s; who: nobody (Refined's regeneration is not "
          "installed)); NOT working: 0052D1D0 (PlayerHealAmount) is not the stock code; Playable Eileen without SH4PE_RegenActive "
          "(v3.46 front end)" in r, 'v3.47: the refused install summarised')
    s, r = run(V333_END)
    check(s.rg is None and 'regeneration:' not in r, 'v3.47: no Regeneration lines, no summary line')
    s, r = run(V348RZ)  # v3.48 RZ348 (round 2: PerVisit, weapons)
    print(r)
    check(s.rz == ('Random', 'Off', 'In', 123456, 137, 17, 137, 'the stock scripts', 'On', 27, 27, 19) and
          s.rz_roll == ('0', 123456, True) and s.rz_first == ('10:00:05', 2, 1, 'Random'),
          'v3.48: the Randomizer start, seed roll and first room load parse (%r %r %r)' % (s.rz, s.rz_roll, s.rz_first))
    check([p[1:4] + (p[6], p[10], p[11], p[12], p[14], p[15], p[16], p[17]) for p in s.rz_plans] ==
          [(2, 0, 'Random', 10, 0, 0, 4, 7, 'On', 1, False), (2, 1, 'Shuffle', 20, 1, 1, 7, 7, 'Off', 0, False),
           (3, 1, 'Random', 9, 0, 1, 1, 1, 'Off', 0, False)],
          'v3.48: the plans parse (%r)' % (s.rz_plans,))
    check(s.rz_rooms == {'pooled': 3, 'changed': 4, 'slots': 5, 'weapons': 2, 'revisits': 1, 'other': 1} and s.rz_lists == 1 and
          s.rz_anom == 1, 'v3.48: the room loads, the verbose list and the anomaly parse (%r %r %r)' % (s.rz_rooms, s.rz_lists, s.rz_anom))
    check([x[1:] for x in s.rz_sets] == [('Items', 'Shuffle', 'Random'), ('Seed', '777', 'New Seed'), ('PerVisit', 'Off', 'On'),
                                         ('KeepSpecials', 'On', '[Randomizer] read again')] and
          s.rz_sess[4:11] == ('Off', '40', '3', '4', '2', '3', '1'),
          'v3.48: the changes and the session line parse (%r %r)' % (s.rz_sets, s.rz_sess))
    check('randomizer: Items Random (Keep Specials Off, Room 302 In, Change Each Visit On, seed 123456) at the start; self-check 137 '
          'of 137 plain pickups and 27 of 27 weapon spots in 17 scripts (the stock scripts); 19 weapons may move; seed rolled 123456 '
          '(written); first room load 2:1 with Items Random [10:00:05]; 3 plan(s): 2/0 Random 10 of 12 changed, 4 weapon(s), 2/1 '
          'Shuffle 20 of 25 changed, 7 weapon(s), 3/1 Random 9 of 12 changed, 1 weapon(s); fewer Swords than the stock in 1 plan(s); '
          '3 room load(s) with pickups in the pool (1 of them a room visited again), 4 of 5 pickup(s) changed, 2 weapon(s), 1 room(s) '
          'with words NOT the stock item (1 verbose list(s)); 4 change(s): Items Shuffle (Random) [10:01:00], Seed 777 (New Seed) '
          '[10:01:01], PerVisit Off (On) [10:01:02], KeepSpecials On ([Randomizer] read again) [10:02:00]; session: 40 room load(s), '
          '3 pooled, 4 changed, 2 weapon(s), 3 plan(s), 1 left as found; 1 ANOMALY line(s)' in r, 'v3.48: the Randomizer summary line')
    s, r = run(V348RZB)
    check(s.rz_bad == [('Items', 'Randomize', 'Random')] and s.rz[4] == 136 and s.rz_anom == 1 and
          'unknown ini values: Items "Randomize" -> Random' in r and 'self-check 136 of 137' in r and 'NOT the stock' not in r and
          '1 plan(s) COULD NOT PLACE every item' in r and s.rz_plans[0][17],
          'v3.48: an unknown ini value, a script that is not the stock one, an unplaced plan (%r %r)' % (s.rz_bad, s.rz))
    s, r = run(V349RZL)  # v3.49 RZ349L
    check(s.rzl == ('Random', 'Spots') and s.rzl_bad == [('Ghosts', 'Rooms', 'Spots')] and s.rzl_first[1:4] == (7, 1, 'Random') and
          s.rz is None and not s.rz_sets and s.rz_anom == 0 and s.rzl_anom == 1,  # v3.49 MERGE: rz_anom 0 (the looks' ANOMALY counted apart)
          'v3.49: the looks start, the unknown value, the first room load parse, not as Items lines (%r %r %r)' % (
              s.rzl, s.rzl_bad, s.rzl_first))
    check([k for _, k, _ in s.rzl_out] == ['draw', 'takeover', 'back', 'single', 'back'] and
          s.rzl_gh == {'rooms': 2, 'moved': 3, 'victims': 4, 'revisits': 1} and
          [x[1:] for x in s.rzl_sets] == [('Ghosts', 'Off', 'Spots'), ('EileenOutfit', 'Off', '[Randomizer] read again')] and
          s.rzl_sess[1:9] == ('3', '1', '0', '1', '1', '2', '1', '') and s.rzl_sess[13:15] == ('1', '1'),
          'v3.49: the outfit events, the ghost rooms, the changes and the session parse (%r %r %r %r)' % (
              s.rzl_out, s.rzl_gh, s.rzl_sets, s.rzl_sess))
    check('randomizer looks: Eileen Outfit Random, Ghosts Spots at the start; unknown ini values: Ghosts "Rooms" -> Spots; first '
          'room load 7:1: outfit Random (the companion only: Playable Eileen dresses the player ([Variant] in '
          'SH4PlayableEileen.ini)), ghosts Spots (victim ghosts, never the story ghosts) [10:00:05]' in r and
          '; outfit: 1 draw(s), 1 with one outfit, 2 back to the set one, 1 Extra Menu choice(s); last: Default back (a world without '
          'her) [10:01:00], only the Dress unlocked (visit 3, stage 5) [10:02:00], Default back (Eileen Outfit Off) [10:03:02]' in r and
          '; ghosts: 2 room load(s) with victims (1 of them a room visited again), 3 of 4 victim(s) moved' in r and
          '; 2 change(s): Ghosts Off (Spots) [10:03:00], EileenOutfit Off ([Randomizer] read again) [10:03:01]' in r and
          '; session: outfit 3 world visit(s), 1 draw(s) (Dress 0, Nurse 1), 2 back, 1 chosen; ghosts 2 room load(s), 4 victim(s), 3 '
          'moved, 1 kept, 1 left as found; 1 ANOMALY line(s)' in r, 'v3.49: the looks summary line')
    s, r = run(V349RZLB)
    check(s.rzl_gh['ct'] == 1 and 'NOT available' in s.rzl_first[4] and
          "ghosts left alone: the cheat table's Enemy Randomizer is on" in r and 'randomizer looks: (no start line)' in r,
          'v3.49: the outfit engine off, the cheat table\'s Enemy Randomizer on (%r)' % (s.rzl_first,))
    s, r = run(V350RL)  # v3.50 RL350
    check(s.rzl == ('Random', 'Spots') and [k for _, k, _ in s.rzl_out] == ['draw'] and
          s.rzl_room == {'draws': 2, 'changed': 1, 'held': 1} and s.rzl_gh == {'rooms': 2, 'moved': 2, 'victims': 3, 'revisits': 1} and
          s.rzl_gv['far'] == 1 and s.rzl_gv['near'] == 1 and s.rzl_gv['recmoved'] == 1 and s.rzl_gv['reckept'] == 1 and
          s.rzl_gv['dmax'] == 1478 and s.rzl_sess is None and s.rzl_sess2 and s.rzl_sess2[5:7] == ('2', '1') and
          s.rzl_sess2[14:18] == ('1', '1', '1', '1') and s.rzl_anom == 0 and s.rz is None,
          'v3.50: the room draws, the final fight, the ghosts distance / record and the v3.50 session parse (%r %r %r %r)' % (
              s.rzl_room, s.rzl_gh, s.rzl_gv, s.rzl_sess2))
    check('; outfit: 1 draw(s), 0 with one outfit, 0 back to the set one, 0 Extra Menu choice(s); last: Nurse (visit 2, stage 5) '
          '[10:00:04]; outfit room draws: 2 (1 changed her outfit), 1 held in the final fight' in r and
          '; ghosts: 2 room load(s) with victims (1 of them a room visited again), 2 of 3 victim(s) moved (1 800 or more away, 1 '
          'nearer: the room allowed no farther; mean 1135, most 1478; 1 record(s) moved), 1 kept by a pinned or down record' in r and
          '; session: outfit 2 world visit(s), 1 world draw(s), 2 room draw(s) (1 changed), Dress 1 / Nurse 2, 1 back, 0 chosen; '
          'ghosts 2 room load(s), 3 victim(s), 2 moved (1 at 800 +, 1 record(s) moved), 1 kept (1 by a record), 0 left as found' in r,
          'v3.50: the looks summary line with the room draws and the ghosts distances (%r)' % r[r.find('randomizer looks'):][:900])
    s, r = run(V349RZL)  # v3.50 RL350: a v3.49 log: no distances counted, no room draws
    check(not s.rzl_gv['far'] and not s.rzl_gv['near'] and not s.rzl_room and s.rzl_sess2 is None and s.rzl_sess and
          'outfit room draws' not in r and '800 or more away' not in r, 'v3.50: a v3.49 looks log reads as before')
    s, r = run(V333_END)
    check(s.rz is None and 'randomizer:' not in r, 'v3.48: no Randomizer lines, no summary line')
    s, r = run(V349F)  # v3.49 EF349
    print(r)
    check(s.fol_mode == 'Monsters + Victims' and s.fol_modes == [('10:00:05', 'Victims Only'), ('10:01:00', 'Monsters + Victims')] and
          s.fol_home == [2, 1] and s.fol['stops'] == 1 and s.fol['doors'] == 1,
          'v3.49: the Enemies Follow value lines read (%r %r %r %r)' % (s.fol_mode, s.fol_modes, s.fol_home, dict(s.fol)))
    check('enemies follow setting: Monsters + Victims at the start; 2 change(s): Victims Only [10:00:05], Monsters + Victims '
          '[10:01:00]; with no monsters following (Off / Victims): 2 monster(s) sent home from other rooms, 1 cop(ies) made in '
          'their own rooms again' in r, 'v3.49: the Enemies Follow setting line')
    check('enemies follow: 1 door(s), 1 follower(s) in all' in r, 'v3.49: the Enemies Follow summary line still reads')
    s, r = run(V349F2)
    print(r)
    check(s.fol_mode == 'Victims Only' and s.fol_modes == [('10:00:30', 'Off')] and s.fol_home == [0, 3],
          'v3.49: Victims Only at the start, an unsaved change, Off clearing the ledger (%r %r %r)' % (s.fol_mode, s.fol_modes, s.fol_home))
    check('enemies follow setting: Victims Only at the start; 1 change(s): Off [10:00:30]; with no monsters following (Off / Victims'
          '): 0 monster(s) sent home from other rooms, 3 cop(ies) made in their own rooms again' in r,
          'v3.49: the setting line for Victims Only at the start')
    s, r = run(V350EF)  # v3.50 EF350
    print(r)
    check(s.fol_mode == 'Victims' and s.fol_modes == [('10:00:03', 'Monsters + Victims')] and s.fol_home == [1, 2],
          'v3.50: Victims (renamed) reads (%r %r %r)' % (s.fol_mode, s.fol_modes, s.fol_home))
    e = s.ef
    check(s.ef_mode == 'Normal' and s.ef_site == 'in' and s.ef_modes == [('10:00:30', 'Forgetful'), ('10:01:05', 'Relentless')],
          'v3.50: Enemy Pursuit start and changes (%r %r %r)' % (s.ef_mode, s.ef_site, s.ef_modes))
    check(e['lost'] == 3 and e['lost_doors'] == 2 and e['sight'] == 1 and e['down'] == 1 and e['behind'] == 1 and e['forgot'] == 4,
          'v3.50: lost-track counts by reason (%r)' % dict(e))
    check(e['saves'] == 2 and e['saves_unwritten'] == 1 and e['s_pur'] == 4 and e['s_here'] == 3 and e['s_q'] == 1 and
          e['s_tag'] == 2 and e['s_cw'] == 2 and e['found'] == 1 and e['none'] == 1 and e['restores'] == 1 and e['r_pur'] == 3 and
          e['r_here'] == 2 and e['r_q'] == 1 and e['r_tag'] == 2 and e['r_no'] == 1, 'v3.50: follow save / load / restore counts (%r)' % dict(e))
    check('enemy pursuit: Normal at the start; 2 change(s): Forgetful [10:00:30], Relentless [10:01:05]; 3 lost track of you at 2 '
          'door(s) (1 out of their senses, 1 down when you left, 1 did not catch up), 4 forgot before a door; followers across saves '
          'in: 2 save(s) kept followers (4 pursuer(s), 3 of them with you, 1 on their way, 2 ghost tag(s), 2 cop(ies)), 1 NOT WRITTEN '
          'to the side file; 1 load(s) found theirs, 1 had none; 1 restore(s): 3 pursuer(s) back (2 with you), 1 on their way, 2 '
          'ghost(s) let in, 0 cop(ies); left out 0 without a record, 0 of a kind not followed now; 1 found but not restored' in r,
          'v3.50: the enemy pursuit summary line')
    check('enemies follow setting: Victims at the start' in r and 'with no monsters following (Off / Victims): 1 monster(s)' in r,
          'v3.50: the setting line with Victims')
    check('inside rooms' not in r and not s.ep and s.ep_site is None, 'v3.50 log: no in-room part')  # v3.51 EP351
    s, r = run(V351EP)  # v3.51 EP351: Enemy Pursuit inside the room
    print(r)
    p = s.ep
    check(s.ep_site == 'in' and s.ef_mode == 'Forgetful', 'v3.51: in-room install line (%r %r)' % (s.ep_site, s.ef_mode))
    check(p['rooms'] == 2 and p['give'] == 5 and p['mon'] == 3 and p['ghost'] == 2 and p['again'] == 1 and p['down'] == 1 and
          p['held'] == 3 and p['stock'] == 1, 'v3.51: the rooms\' lines (%r)' % dict(p))
    check(p['v_give'] == 5 and p['v_unsensed'] == 2 and p['v_downed'] == 1 and p['v_far'] == 1 and p['v_untouched'] == 1 and
          p['f_Hummer'] == 1 and p["f_victim's ghost"] == 2 and p['f_Sniffer Dog'] == 1 and p['f_Gum Head'] == 1,
          'v3.51: the verbose give-ups by reason and family (%r)' % dict(p))
    check("; inside rooms in: 5 gave up the chase in 2 room(s) (3 monster(s), 2 ghost(s); 1 again, 1 down when you walked off), 3 "
          "set back in their rest, 1 left to the game [verbose: 5 lines: 2 unsensed, 1 downed, 1 far, 1 untouched; Gum Head 1, "
          "Hummer 1, Sniffer Dog 1, victim's ghost 2]" in r, 'v3.51: the enemy pursuit summary has the in-room give-ups')
    check(s.ef['lost'] == 2 and s.ef['sight'] == 2, 'v3.51: the door line still reads (%r)' % dict(s.ef))
    check('calm' not in r and not s.ep['undive'] and not s.ep['sensed'] and 'out of sight' not in r,
          'v3.51 log: no v3.52 calm part')  # v3.52 EP352
    s, r = run(V352EP)  # v3.52 EP352: Forgetful's ghosts
    print(r)
    p = s.ep
    check(s.ep_site == 'in' and p['rooms'] == 2 and p['give'] == 4 and p['ghost'] == 4 and p['again'] == 1 and p['held'] == 4 and
          p['stock'] == 0, "v3.52: the rooms' lines still read (%r)" % dict(p))
    check(p['undive'] == 3 and p['sensed'] == 1, 'v3.52: the calm parts (%r)' % dict(p))
    check(p['v_give'] == 3 and p['v_far'] == 1 and p['v_sight'] == 1 and p['v_untouched'] == 1, 'v3.52: out of sight (%r)' % dict(p))
    check("1 left to the game" not in r and "0 left to the game; ghosts' calm: 3 ghost dive(s) into a wall stopped, 1 ghost(s) "
          "sensed you again [verbose: 3 lines: 0 unsensed, 0 downed, 1 far, 1 untouched, 1 out of sight; victim's ghost 3]" in r,
          'v3.52: the enemy pursuit summary has the calm and the out-of-sight give-ups')
    s, r = run(V349F)
    check('enemy pursuit:' not in r and s.ef_mode is None, 'v3.49 log: no enemy pursuit line')
    s, r = run(V349F3)
    check(s.fol_mode == 'Off' and not s.fol_modes and 'enemies follow setting' not in r, 'v3.49: Off all along: no setting line')
    s, r = run(V333_END)
    check(s.fol_mode is None and 'enemies follow setting' not in r, 'v3.49: no enemies lines: no setting line')
    s, r = run(V349RZM)  # v3.49 RZ349: the Monsters
    print(r)
    check(s.rzm == ('This World', 235, 783, 17, 235, 783, 'the stock scripts', 'Whitestool, Hummer, Gum Head, Gum Head (v1), Twin '
                    'Victim, Twin Victim (v1), Tremer, Sniffer Dog, Sniffer Dog (v1)', 'in') and s.rzm_gum == 'may be rolled' and
          s.rzm_first == ('10:00:05', 2, 4, 'This World'), 'v3.49: the Monsters start, Gum Heads, first room parse (%r %r)' % (s.rzm, s.rzm_first))
    check(s.rzm_rooms == {'rooms': 2, 'revisits': 1, 'changed': 2, 'slots': 4, 'sticky': 1, 'drops': 1, 'bones': 1, 'files': 0,
                          'floor': 0, 'other': 1, 'types': 1, 'kb': 612} and s.rzm_by == {'Tremer': 1, 'Gum Head': 1, 'Sniffer Dog': 2}
          and s.rzm_lists == 1, 'v3.49: the Monsters room loads parse (%r %r)' % (s.rzm_rooms, s.rzm_by))
    check(s.rzm_anom == 1 and s.rz_anom == 0, 'v3.49: a Monsters ANOMALY counts as the Monsters\' (%d / %d)' % (s.rzm_anom, s.rz_anom))
    check([x[1:] for x in s.rzm_sets] == [('This Room', 'This World'), ('Off', 'This Room'), ('This World', '[Randomizer] read again')]
          and [c[1] for c in s.rzm_ct] == ['on', 'off again'] and s.rzm_silent == [('Twin Victim', 2)] and
          s.rzm_misc['files'] == 1 and s.rzm_misc['offdrop'] == 1 and s.rzm_misc['save'] == 1 and s.rzm_misc['saved_out'] == 1 and
          s.rzm_misc['savev'] == 1, 'v3.49: the Monsters changes, the cheat table, silent types, files, saves parse (%r %r %r)' % (
              s.rzm_sets, s.rzm_ct, s.rzm_misc))
    check(s.rzm_sess and s.rzm_sess[:6] == ('This World', '123456', 'On', '40', '2', '2') and s.rzm_sess[14:18] == ('1', '1', '1', '1'),
          'v3.49: the Monsters session line parses (%r)' % (s.rzm_sess,))
    check("randomizer monsters: This World at the start; self-check 235 of 235 source placements (783 enemy placements) in 17 scripts "
          "(the stock scripts); save filter in; Gum Heads may be rolled; the cheat table's Enemy Randomizer: on [10:01:20], off again "
          "[10:01:40]; first room 2:4 with Monsters This World [10:00:05]; 2 room load(s) randomized (1 of them a room visited again), "
          "2 of 4 placement(s) changed (made as: Sniffer Dog 2, Tremer 1, Gum Head 1), 1 kept their record's type, 1 record(s) "
          "deleted; kept as placed: 1 bones, 0 files, 0 no floor; files loaded for 1 type(s) (612 KB), 1 room(s) with words NOT the "
          "stock value (1 verbose list(s)); 1 file load(s) refused; 1 room(s) with records of earlier randomized monsters deleted; "
          "may be silent: Twin Victim in stage 2; 1 save(s) left 1 randomized record(s) out; 3 change(s): Monsters This Room (This "
          "World) [10:01:00], Monsters Off (This Room) [10:02:00], Monsters This World ([Randomizer] read again) [10:02:20]; session: "
          "40 room load(s), 2 randomized, 2 changed, 1 by record, 1 record(s) deleted (+2 off), kept 1 / 1 / 0, 1 save(s) 1 out, 1 "
          "left as found; 1 ANOMALY line(s)" in r, 'v3.49: the Monsters summary line')
    check('randomizer: Items Random' in r and '; 1 ANOMALY' not in r.split('randomizer monsters:')[0].split('randomizer: Items')[1],
          'v3.49: the Items line keeps its own ANOMALY count')
    s, r = run(V349RZMB)
    check(s.rzm_bad == ('Everywhere', 'This World') and s.rzm[1] == 230 and s.rzm[8].startswith('NOT in') and s.rzm_gum ==
          'are never rolled' and s.rzm_misc['hit'] == 1 and s.rzm_anom == 1 and 'unknown ini value: Monsters "Everywhere" -> This World'
          in r and 'self-check 230 of 235' in r and 'save filter NOT in' in r and 'the STOCK hit-shape pool' in r,
          'v3.49: an unknown Monsters value, scripts not the stock ones, no save filter, no Gum Heads, the stock hit pool (%r)' % (s.rzm,))
    check(not [v for v in s.warnings.values() if 'monsters' in v[3].lower() and 'Everywhere' not in v[3] and 'NOT in' not in v[3]],
          'v3.49: no other Monsters line lands in the warnings (%r)' % (list(s.warnings.values()),))
    s, r = run(V333_END)
    check(s.rzm is None and 'randomizer monsters:' not in r, 'v3.49: no Monsters lines, no summary line')
    s, r = run(V349RZM + '''10:09:10.000 randomizer: monsters: room 5:3: 2 record(s) of another type that Refined did not make (another writer): those placements stay as placed, the records are kept
10:09:10.001 randomizer: monsters session: 2 placement(s) left as placed for a record Refined did not make (another writer); 40 placement(s) of Refined's (0 full)
''')  # v3.49 fix2 RZ349-SAVE-001
    check(s.rzm_misc['other'] == 1 and s.rzm_misc['other_n'] == 2 and s.rzm_misc['sess_other'] == 2 and
          '; 1 room(s) with 2 record(s) another writer retyped, left alone' in r and 'FULL' not in r,
          'v3.49 fix2: records of another writer parse and are summarised (%r)' % (s.rzm_misc,))
    check(s.rzl is None and 'randomizer looks:' not in r, 'v3.49: no looks lines, no looks summary line')
    s, r = run(V350RZM)  # v3.50 RM350: a save-based game, All Worlds, the reasons
    print(r)
    check(s.rzm and s.rzm[0] == 'All Worlds' and s.rzm_first == ('10:00:05', 5, 11, 'All Worlds') and
          [x[1:] for x in s.rzm_sets] == [('All Worlds', 'This World'), ('All Worlds', '[Randomizer] read again')],
          'v3.50: All Worlds at the start, in the first room line and in the changes (%r %r)' % (s.rzm, s.rzm_sets))
    check(s.rzm_rooms['rooms'] == 3 and s.rzm_rooms['changed'] == 7 and s.rzm_rooms['why_lines'] == 2 and s.rzm_rooms['adopted'] == 8
          and s.rzm_rooms['adopt_changed'] == 7 and s.rzm_rooms['roll_stock'] == 1 and s.rzm_rooms['same'] == 1 and
          s.rzm_rooms['away'] == 1 and s.rzm_rooms['other_rec'] == 0 and s.rzm_rooms['none'] == 0 and s.rzm_by['Twin Victim'] == 3,
          'v3.50: the room lines with the reasons parse, a v3.49 room line too (%r)' % (s.rzm_rooms,))
    check(s.rzm_sess and s.rzm_sess[0] == 'All Worlds' and s.rzm_sess3 == (8, 7, 1, 1, 1, 0), 'v3.50: both session lines parse (%r %r)'
          % (s.rzm_sess, s.rzm_sess3))
    check("3 room load(s) randomized (2 of them a room visited again), 7 of 11 placement(s) changed" in r and
          "; the game's own records: 8 adopted (7 changed, 1 kept); stock type kept: 1 rolled it, 1 by a stock record, 1 recorded "
          "elsewhere, 0 for another writer's record, 0 with no target" in r and '; session records: 8 adopted (7 changed)' in r and
          'may be silent: Twin Victim in stage 5' in r, 'v3.50: the summary shows adopted / kept / changed')
    check(not [v for v in s.warnings.values() if 'monsters' in v[3].lower()], 'v3.50: no Monsters line lands in the warnings (%r)' % (
        list(s.warnings.values()),))
    s, r = run(V349RZM)
    check("the game's own records" not in r and 'session records' not in r and s.rzm_rooms['why_lines'] == 0,
          'v3.50: a v3.49 log has no reasons part')
    s, r = run(V350RZMB)
    check(s.rzm_bad == ('Everywhere', 'This World'), 'v3.50: the unknown-value line with All Worlds in its list parses (%r)' % (s.rzm_bad,))
    s, r = run(V351SN)  # v3.51 SN351: the event dogs and the second roll
    print(r)
    check(s.rzm and s.rzm[1] == 238 and s.rzm[4] == 238 and s.rzm_ev == (3, 3, 'On') and s.rzm_sess4 == (2, 'On', 3, 3),
          'v3.51: the start line (238), the event dogs line and the session line parse (%r %r %r)' % (s.rzm, s.rzm_ev, s.rzm_sess4))
    check(s.rzm_rooms['rooms'] == 3 and s.rzm_rooms['changed'] == 3 and s.rzm_rooms['again'] == 2 and s.rzm_rooms['ev_rooms'] == 2 and
          s.rzm_rooms['ev_set'] == 3 and s.rzm_rooms['ev_clear'] == 3 and s.rzm_rooms['why_lines'] == 3 and s.rzm_lists == 3,
          'v3.51: the room lines with the second roll and the event dogs parse (%r)' % (s.rzm_rooms,))
    check('; event dogs 3 of 3 stock (rolled once story flag 130 is set), roll again On' in r and
          '; 2 rolled again for their own type; event dogs in 2 room load(s): 3 rolled (story flag 130 set), 3 kept as placed '
          '(flag clear)' in r and '; session: 2 rolled again (roll again On), event dogs 3 planned / 3 kept asleep' in r,
          'v3.51: the summary shows the event dogs and the second roll')
    check(not [v for v in s.warnings.values() if 'monsters' in v[3].lower()], 'v3.51: no Monsters line lands in the warnings (%r)' % (
        list(s.warnings.values()),))
    s, r = run(V351SN.replace(SN351_EVIN % (3, 'On'), SN351_EVIN % (5, 'Off')))
    check(s.rzm_ev == (5, 3, 'Off') and '; event dogs 5 of 3 stock (NOT the stock count) (rolled once story flag 130 is set), roll '
          'again Off' in r, 'v3.51: event dogs not the stock 3: said in the summary (%r)' % (s.rzm_ev,))
    s, r = run(V350RZM)
    check(s.rzm_ev is None and s.rzm_sess4 is None and 'event dogs' not in r and 'rolled again' not in r and
          s.rzm_rooms['again'] == 0 and s.rzm_rooms['ev_rooms'] == 0, 'v3.51: a v3.50 log has no event dogs / second roll parts')
    s, r = run(V350R)  # v3.50 R302: the user's game today
    check(len(s.r3_own) == 1 and s.r3_own[0][2:4] == ('Playable Eileen', 'Playable Eileen') and s.r3_hook is None and
          len(s.r3_seed) == 4 and s.r3_anom == 0 and
          "room 302: look Playable Eileen, walk Playable Eileen (at start) [10:00:00]; Playable Eileen v1.24 loaded without "
          "SH4PE_Room302Yield (older than the hand-over); LookDownLimit 85, LookUpLimit 85, LookFeel 1, WalkSpeed 0.65; first start: "
          "4 key(s) seeded (LookDownLimit 85, LookUpLimit 85, LookFeel 1, WalkSpeed 0.65) from Playable Eileen's ini" in r and
          'look hooks in' not in r, 'v3.50: Room 302 with Playable Eileen v1.24.4: its owners line, the seed, no hook (%r)' % (s.r3_own,))
    s, r = run(V350RA)
    check(s.r3_hook == ('10:00:00', 'at start') and s.r3_visits == 2 and s.r3_n == [420, 25200, 3100, 40, 25, 15, 1, 24100] and
          s.r3_sets == [('10:06:00', 'WalkSpeed', '0.65', '0.8', 'Options')] and s.r3_bad == [('LookFeel', '0.8', '1')] and
          s.r3_rebuilt == 1 and s.r3_anom == 1 and s.r3_seed == [('LookDownLimit', '85', 'the default')],
          'v3.50: Room 302 owned by Refined: hooks, visits summed, a change, an off-row value, a rebuild, an anomaly (%r %r)' % (
              s.r3_n, s.r3_bad))
    check("room 302: look SH4 Refined, walk SH4 Refined (at start) [10:00:00]; Playable Eileen not loaded; " in r and
          '; look hooks in [10:00:00]; ini value(s) off the rows: LookFeel=0.8 -> 1; 2 visit line(s), 420 s: look 25200 tick(s) '
          '(eased 3100, clamped 40: up 25, down 15; view rebuilt 1), walk 24100 tick(s); 1 change(s): WalkSpeed 0.65 -> 0.8 '
          '(Options) [10:06:00]; 1 ANOMALY line(s)' in r and "from Playable Eileen's ini" not in r,
          'v3.50: the Room 302 summary line (Refined)')
    s, r = run(V350RW)
    check(len(s.r3_own) == 2 and s.r3_own[0][9] and not s.r3_own[1][9] and s.r3_own[1][2:4] == ('SH4 Refined', 'Playable Eileen') and
          'room 302: look SH4 Refined, walk Playable Eileen (first game tick) [10:00:01]; Playable Eileen v1.25 yields look '
          '(SH4PE_Room302Yield)' in r and 'STILL WAITING' not in r and s.r3_hook == ('10:00:01', 'first game tick'),
          'v3.50: a yielding Playable Eileen decided on the first tick (%r)' % (s.r3_own,))
    s, r = run(V333_END)
    check(not s.r3_own and 'room 302:' not in r, 'v3.50: no Room 302 lines, no summary line')
    s, r = run(V350FI)  # v3.50 FI350: Fade In
    print(r)
    check(s.fi == ('in', 'scene pass', 'Fade + Portal', '0.80') and s.fi_rooms == 2 and s.fi_portals == 1 and
          s.fi_src == {'room load': 2, 'follower': 1, 'copy': 1, 'revive': 1} and
          s.fi_skip == {'type': 1, 'dead': 1, 'own portal': 1} and s.fi_starts == {'room load': 1, 'follower': 1},
          'v3.50: the Fade In lines read (%r %r %r %r)' % (s.fi, dict(s.fi_src), dict(s.fi_skip), dict(s.fi_starts)))
    check("fade in: Fade + Portal at the start (0.80 s; portal scene pass); 5 fade(s) in 2 room visit(s) (room load 2, follower 1, "
          "copy 1, revive 1); 1 blood portal(s), first drawn [10:00:09] (3D viewport); not faded 3 (type 1, dead 1, own portal 1); "
          "verbose: 2 start line(s); own draw states: type 0A (pre-draw 0045C840); 2 change(s): Fade (menu) [10:00:30], Fade + "
          "Portal ([Gameplay] read again) [10:00:50]" in r, 'v3.50: the Fade In summary line')
    s, r = run(V350FI2)
    print(r)
    check(s.fi and s.fi[0] == 'OFF' and s.fi_bad == ('Dissolve', 'Fade + Portal') and
          'fade in: OFF (0045C880, the game\'s see-through enemy draw, is not the stock code: another mod?); 0 fade(s) in 0 room '
          'visit(s); unknown ini value: FadeIn "Dissolve" -> Fade + Portal' in r,
          'v3.50: Fade In off and an unknown ini value (%r %r)' % (s.fi, s.fi_bad))
    check([v for v in s.warnings.values() if 'fade in: OFF' in v[3]], 'v3.50: Fade In OFF (not the stock code) is in the warnings too')
    s, r = run(V350FI3)
    print(r)
    check(s.fi == ('in', 'scene pass', 'Fade + Portal', '0.80') and s.fi_fault == ('10:01:00', 'the model draw', 'C0000005') and
          s.fi_sets == [('10:01:05', 'Off', 'NOT saved')] and
          'fade in: Fade + Portal at the start (0.80 s; portal scene pass); 2 fade(s) in 1 room visit(s) (ghost 1, monster 1); 1 blood '
          'portal(s), NEVER DRAWN; 1 change(s): Off (NOT saved) [10:01:05]; FAULT in the model draw (code C0000005) [10:01:00]: '
          'off for the session' in r, 'v3.50: a portal never drawn, a fault, an unsaved change (%r %r %r)' % (s.fi, s.fi_fault, s.fi_sets))
    s, r = run(V333_END + '10:00:00.300 ' + FI_INSTALL % ('NOT available (no scene pass)', 'Fade') + '\n')
    check(s.fi == ('in', 'NOT available', 'Fade', '0.80') and 'fade in: Fade at the start (0.80 s; portal NOT available); 0 fade(s) in 0 '
          'room visit(s)' in r, 'v3.50: no scene pass: the portal is not available (%r)' % (s.fi,))
    s, r = run(V333_END)
    check(s.fi is None and 'fade in:' not in r, 'v3.50: no Fade In lines, no summary line')
    s, r = run(V351MU)  # v3.51 MU351: the sound in the Extra Menu's lists
    print(r)
    check(s.mu_set == ('10:00:00', 'paused', '1') and s.mu[('load list', 'from the pause')] == 2 and s.mu[('load list', 'paused')] == 2
          and s.mu[('load list', 'back in the pause')] == 1 and s.mu[('load list', 'a save loads')] == 1 and
          s.mu[('save menu', 'paused')] == 1 and s.mu[('storage chest', 'loud')] == 1 and s.mu_anom == 1 and
          s.mu_loud == [('10:00:40', 'storage chest', 'from the pause', 'PLAYS quietly')],
          'v3.51: the Extra Menu sound lines read (%r %r %r)' % (s.mu_set, dict(s.mu), s.mu_loud))
    check('extra menu sound: paused in the three lists as in the pause menu at the start (PauseSound=1); load list: opened 2 (2 from '
          'the pause, music paused in 2), closed 2 (1 back in the pause, 1 a save loads); save menu: opened 1 (1 from the pause, music '
          'paused in 1), closed 1 (1 back in the pause); storage chest: opened 1 (1 from the pause, music paused in 0), closed 0; the '
          'music NOT paused 1 time(s): storage chest from the pause (PLAYS quietly) [10:00:40]; 1 ANOMALY line(s)' in r,
          'v3.51: the Extra Menu sound summary line')
    check(not [v for v in s.warnings.values() if 'sound in the' in v[3]], 'v3.51: no Extra Menu sound line lands in the warnings')
    s, r = run(V351MU0)
    print(r)
    check(s.mu_set == ('10:00:00', 'v3.50', '0') and not s.mu_loud and s.mu[('load list', 'from play')] == 1 and
          s.mu[('load list', 'back to play')] == 1 and
          'extra menu sound: v3.50 (the music plays quietly in the three lists) at the start (PauseSound=0); load list: opened 2 (1 '
          'from the pause, music paused in 0, 1 from play), closed 2 (1 back in the pause, 1 back to play)' in r and 'NOT paused' not in r,
          'v3.51: PauseSound=0 and a list from play: counted, no fault (%r %r)' % (dict(s.mu), s.mu_loud))
    s, r = run(V333_END)
    check(s.mu_set is None and not s.mu and 'extra menu sound' not in r, 'v3.51: no Extra Menu sound lines, no summary line')
    s, r = run(V352QL)  # v3.52 QL352: Quick Load's paths
    print(r)
    check(s.ql_set == ('10:00:00', 'pause', '1', None) and s.ql['pause'] == 2 and s.ql['key'] == 1 and s.ql['closed'] == 1 and
          s.ql['back'] == 1 and s.ql['drop'] == 1 and s.ql['refused'] == 1 and s.ql_black == [16, 16] and s.ql_ms == [1800] and
          dict(s.ql_closed) == {'a message was up': 1} and not s.ql['loud'] and not s.ql['late'] and s.ql_anom == 0,
          'v3.52: the Quick Load lines read (%r %r %r %r)' % (s.ql_set, dict(s.ql), s.ql_black, dict(s.ql_closed)))
    check('quick load: inside the pause at the start (QuickLoadInPause=1); 4 quick load(s) (2 inside the pause, 1 by the key in '
          'play, 1 after the pause closed (v3.51)); v3.51 path because: a message was up x1; black in 16 frame(s) (worst 16); the '
          'music held until the loaded room ran: 1800 ms on average (worst 1800); 1 handed to a menu / cut-scene of the loaded room; '
          '1 refused at black' in r, 'v3.52: the Quick Load summary line')
    check(not [v for v in s.warnings.values() if 'quick load' in v[3]], 'v3.52: no Quick Load line lands in the warnings')
    s, r = run(V352QL2)
    print(r)
    check(s.ql_set == ('10:00:00', 'v3.51', '1', '0055A160 is not stock') and s.ql['late'] == 1 and s.ql['loud'] == 1 and
          s.ql['notrun'] == 1 and s.ql_anom == 2 and
          'quick load: the pause closes first (v3.51) at the start (QuickLoadInPause=1: 0055A160 is not stock); 1 quick load(s) (1 '
          'inside the pause); black in 90 frame(s) (worst 90); the music held until the loaded room ran: 30000 ms on average (worst '
          '30000); 1 NOT black in time; the music NOT paused at black 1 time(s); the loaded room NOT seen running 1 time(s); 2 '
          'ANOMALY line(s)' in r, 'v3.52: the Quick Load faults counted (%r %r)' % (s.ql_set, dict(s.ql)))
    s, r = run(V333_END + '10:00:05.000 quick load: slot 1 (sequence 25), stage 2 room 4, game time 40974\n')
    check(s.ql['unknown'] == 1 and 'quick load: (no start line); 1 quick load(s) (1 path not logged)' in r,
          'v3.52: a v3.51 quick load line (no path) is counted')
    s, r = run(V333_END)
    check(s.ql_set is None and not s.ql and 'quick load:' not in r, 'v3.52: no Quick Load lines, no summary line')
    s, r = run(V352SND)  # v3.52 SND352: Sound
    print(r)
    check(s.snd_in == ('Smooth', 'in', 'in', 'in', 'in') and s.snd_start and s.snd_start[1][:5] == ('Stereo', '1', '1', 'Smooth', '100')
          and len(s.snd_changes) == 5 and s.snd_changes[0][1:] == ('Mode', 'Stereo', 'Headphones', 'Options') and
          s.snd_changes[1][1:] == ('Stereo Width', '100', '80', 'Options') and s.snd_changes[4][1:] == ('Mode', 'Headphones', 'Stereo',
          'Options discarded') and len(s.snd_min) == 2 and dict(s.snd_fix) == {'byte': 1, 'engine': 1, 'discard': 1} and not s.snd_anom,
          'v3.52: the Sound lines read (%r %r %r %r)' % (s.snd_in, s.snd_start, s.snd_changes, dict(s.snd_fix)))
    check("sound: at the start Mode Stereo (the game's Sound 1, engine 1), pan law Smooth, Stereo Width 100 %, Room Reverb On (probe "
          "ok), Night Mode Off, Mono fold on; speakers stereo (00140004)  [10:00:01]; sites all in; 5 change(s): Mode Stereo -> "
          "Headphones (Options) [10:01:30], Stereo Width 100 -> 80 (Options) [10:01:31], Room Reverb On -> Off (Options) [10:01:32], "
          "Night Mode Off -> Low (Options) [10:01:33], Mode Headphones -> Stereo (Options discarded) [10:01:40]; 2 minute line(s): pans "
          "2D 1500 (one ear only 0.3 %; the stock law 33.3 %), streams 300 (one ear only 0.0 %; the stock law 33.3 %); volumes 2D 2000, "
          "3D 50, streams 400 (Night Mode changed 700); Mono: 0 stream block(s), 0 music fold(s); the Sound byte put back 1 time(s), the "
          "engine re-applied 2 time(s) (1 at an Options discard)" in r, 'v3.52: the Sound summary line')
    check(not [v for v in s.warnings.values() if v[3].startswith('sound: ')], 'v3.52: no Sound line lands in the warnings')
    s, r = run(V352SND1)
    print(r)
    check(s.snd_in[0] == 'Stock' and s.snd_in[1] == 'NOT IN' and s.snd_first == ('10:00:01', 'Mono', '0') and s.snd_anom == 1 and
          s.snd_notin == [('volume (004090A6 / 00409285 / 0040B5E7)', '004090A6')] and
          "sound: at the start Mode Mono (the game's Sound 0, engine 0), pan law Stock, Stereo Width 100 %, Room Reverb On (PROBE "
          "FAILED: none), Night Mode Off, Mono fold on; speakers stereo (00140004)  [10:00:01]; sites NOT IN: volume (PanLaw=Stock); "
          "first start: Mode Mono from the save's Sound 0  [10:00:01]; NOT the GOG code: volume (004090A6 / 00409285 / 0040B5E7) "
          "004090A6; 1 ANOMALY line(s)" in r, 'v3.52: the first start, PanLaw=Stock, a site not stock, the probe failed (%r %r)' % (
              s.snd_in, s.snd_first))
    s, r = run(V333_END + '10:00:00.010 sound: [Sound] Enabled=0: the game\'s own sound and Sound row (no Refined sound site)\n')
    check(s.snd_in == 'off' and "sound: off ([Sound] Enabled=0: the game's own)" in r, 'v3.52: Enabled=0')
    s, r = run(V333_END)
    check(s.snd_in is None and not s.snd_changes and '\n  sound:' not in r, 'v3.52: no Sound lines, no summary line')
    s, r = run(V351PT)  # v3.51 PT351: the tar portal and the hold
    print(r)
    check(s.fi == ('in', 'scene pass', 'Fade + Portal', '0.80') and s.fi_tar and s.fi_tar[1:] == ('Auto', '1') and s.fi_rooms == 1 and
          s.fi_portals == 3 and s.fi_by == {'game': 1, 'Refined': 2} and s.fi_held == 2 and s.fi_real == 1 and s.fi_never == 1 and
          len(s.fi_letgo) == 2 and s.fi_letgo[0][:6] == ('FFFF0007', "the game's own draw", 2.5, 'held to the end', "the ghost's own", 32) and
          s.fi_letgo[1][3:8] == ('it is gone', "Refined's", 12, 10, 25) and s.fi_starts == {'follower': 3} and
          s.fi_tar_starts == {'game, on a wall': 1, 'Refined, in the air': 1, 'Refined, on a wall': 1} and
          s.fi_tar_drawn == ('10:00:09', "the game's own draw") and s.fi_cost == [1, 0.061, 0.2],
          'v3.51: the tar portal lines read (%r %r %r %r %r)' % (s.fi_tar, dict(s.fi_by), s.fi_letgo, dict(s.fi_tar_starts), s.fi_cost))
    check("fade in: Fade + Portal at the start (0.80 s; portal scene pass); 4 fade(s) in 1 room visit(s) (room load 2, follower 2); 3 "
          "tar / blood portal(s), first drawn [10:00:09] (3D viewport); tar portal: the game's own draw, else Refined's (Auto), held; by "
          "draw: game 1, Refined 2; 2 held; first tar portal drawn [10:00:09] (the game's own draw); portal draw at most 0.061 ms a "
          "frame on average in a room (worst frame 0.200 ms); 2 let go (strands: Refined's 1, the ghost's own 1; moved 51 of 121 u; "
          "early: it is gone 1); tar starts: Refined, in the air 1, Refined, on a wall 1, game, on a wall 1; 1 placeholder(s) turned real; 1 "
          "portal(s) closed with nobody" in r, 'v3.51: the tar portal summary')
    check(s.fi_fault is None and s.fi_gfault is None, 'v3.51: no fault read from a clean log')
    s, r = run(V351PT2)
    print(r)
    check(s.fi_gfault == ('10:01:00', 'C0000005') and s.fi_fault is None and s.fi_tdm_bad == 'Splash' and
          s.fi_notstock == ["00478440 (the bone trail's start)"] and
          "NOT stock: 00478440 (the bone trail's start); unknown ini value: FadeInPortalDraw \"Splash\" -> Auto; FAULT in the game's own "
          "portal draw (code C0000005) [10:01:00]: Refined's draw after" in r and 'off for the session' not in r,
          'v3.51: a fault in the game\'s own draw is not Fade In\'s fault (%r %r %r)' % (s.fi_gfault, s.fi_fault, s.fi_notstock))
    s, r = run(V351PT3)
    print(r)
    check(s.fi_tar and s.fi_tar[1] == 'Vortex' and s.fi_by == {'vortex': 1} and s.fi_held == 0 and s.fi_cost == [0, 0.0, 0.0] and
          "tar portal NOT available (FadeInPortalDraw=Vortex): v3.50's blood vortex; by draw: vortex 1; 0 held" in r and
          'NEVER DRAWN' not in r, 'v3.51: Vortex: v3.50\'s portal, no hold (%r %r)' % (s.fi_tar, dict(s.fi_by)))
    s, r = run(V352FX)  # v3.52 FX352
    print(r)
    dof = [x for x in r.splitlines() if x.startswith('  depth of field: ')]
    pp = [x for x in r.splitlines() if x.startswith('  post effects: ')]
    check(s.dof_close == 7733 and s.dof_close_rooms == ['2:1'] and len(dof) == 1 and
          'the player close to the camera: drawn in 7733 frame(s), first in 1 room(s)' in dof[0] and 'nearer than 150' not in dof[0],
          'v3.52 FX352: the close camera reads (%r %r %r)' % (s.dof_close, s.dof_close_rooms, dof))
    check(s.dof['drawn'] == 18560 and s.dof['asked'] == 18568 and s.dof_why == {"the room's depth probe is pending": 8},
          'v3.52 FX352: the room line with the close count still reads (%r %r)' % (dict(s.dof), dict(s.dof_why)))
    check(s.pp_mig == ('Very High', 'High', True) and s.pp == ('hooked', 'High', 'Off', 'On') and s.pp_set == [('10:01:30', 'Bloom', 'Ultra')],
          'v3.52 FX352: the Bloom migration and the five levels read (%r %r %r)' % (s.pp_mig, s.pp, s.pp_set))
    check(s.pp_lens_rooms == [('2:1', 40, 60, 100), ('2:2', 15, 15, 70)] and len(pp) == 1 and
          'lens shown 55 of 75 s in 2 room line(s), at most 100 % strength' in pp[0] and
          "the ini's v3.51 Bloom=Very High read as High (rewritten)" in pp[0],
          'v3.52 FX352: the lens room lines and the summary (%r %r)' % (s.pp_lens_rooms, pp))
    s, r = run(V352FXN)
    pp = [x for x in r.splitlines() if x.startswith('  post effects: ')]
    check(s.pp_mig == ('High', 'Medium', False) and len(pp) == 1 and "Bloom=High read as Medium (NOT rewritten)" in pp[0] and
          'lens shown' not in pp[0], 'v3.52 FX352: a migration that could not be written (%r %r)' % (s.pp_mig, pp))
    s, r = run(V347D)
    check(s.dof_close == 0 and not s.dof_close_rooms and 'close to the camera' not in r and s.pp_mig is None and not s.pp_lens_rooms,
          'v3.52 FX352: an older log: no close camera, no migration, no lens rooms')
    test_compare()  # v3.40 LG340
    print('%d failure(s)' % len(FAILS))
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main())
