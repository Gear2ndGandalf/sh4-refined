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
V348RZB = V333_END + """10:00:00.040 randomizer: [Randomizer] Items "Randomize" is not Off / Shuffle / Random: Random (the default)
10:00:00.041 """ + RZ_INSTALL % ('Random', 136, ': some scripts are not the stock ones (their pickups are left alone)') + """
10:00:06.000 ANOMALY: randomizer: stage 2 visit 1: the script at 00683AA0 is not the stock one (17 plain pickup(s), 7 weapon spot(s)): its pickups are left alone
10:00:07.000 randomizer: stage 5 visit 1: Shuffle, 25 of 25 pickup(s) in the pool (5 weapon spot(s)), 20 changed (seed 1, Keep Specials Off, Room 302 In, Change Each Visit On, world visit 3); Holy Candle 4 (stock 4), Saint Medallion 1 (stock 1), Sword of Obedience 1 (stock 1), weapons 5 (stock 5); SOME ITEMS COULD NOT BE PLACED (they keep their spots)
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


def run(text, name='SH4Refined.log'):
    d = tempfile.mkdtemp(prefix='logcheck_')
    p = os.path.join(d, name)
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    s = logcheck.Session(p)
    s.read()
    return s, '\n'.join(s.report(False, None))


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
    s, r = run(V333_END)
    check(s.rz is None and 'randomizer:' not in r, 'v3.48: no Randomizer lines, no summary line')
    test_compare()  # v3.40 LG340
    print('%d failure(s)' % len(FAILS))
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main())
