# SH4 Refined (formerly SH4 Window Mod; winmm.dll) for Silent Hill 4: The Room (GOG)

Renamed 2026-11-04 (v2.1) at the user's request. v2.1.1 renames the files next to the game: SH4Refined.ini,
SH4Refined.log (+ .prev.log); `src/file_names.inc` moves SH4WindowMod.ini / .log on the first start (tests:
`test/test_names.c`, 24 checks). Unchanged: winmm.dll, the `SH4WM_*` exports (Playable Eileen imports them), the
process mutex `Local\SH4WindowMod.<pid>` and the `refined_mod` folder.

Runs the game in a real window and keeps it running when you switch away:

- **Windowed mode**: a 4:3 window (784x588 by default), centred, never always-on-top.
- **Resizing**: drag the edges; with `SnapToStandard=1` the size snaps to standard heights in the picture's shape.
- **No freeze or minimise on Alt+Tab**: the game is always told its window is active (`KeepActiveOnAltTab`), and the
  game's own "minimise when unfocused" check is switched off in memory (`NoMinimize`, also in full screen).
- **Mouse capture**: while the game is in front the cursor is hidden and kept inside the window; Alt+Tab, any other
  focus loss or dragging the window releases it (`CaptureMouse=0` leaves the cursor free). DirectInput runs
  non-exclusive, so keyboard and mouse are never taken away from Windows.
- **Options in plain words** (v3.53.0, round R1 of the Options layout plan). Every Options page, row, value and line
  says what it does, in the plan's words: page names instead of numbers (Display, Controls, Interface, Presentation,
  Gameplay, Pause Menu; Subtitles, Effects, Shadows, Room 302, Player, Enemies, Powers, Randomizer, Sound), short row
  names (labels at most 18 characters, values at most 13, so no value moves the value column), and a one-line
  description that starts with the value where the value has its own line ("Rare: about 1 kill in 16."). Delayed
  settings end with "Next start." or "Next room." (was "Next door."). The rows stay on the pages they were on; the
  later rounds regroup them. The ini keeps every key and every word it writes: where the screen shows a new word, the
  row maps it (the table "Screen words and ini words" below).
- **How the Options pages look** (v3.53.0, round R2 of the Options layout plan; hidden `[Options] Look=0` turns it off).
  - **Page strip.** A line under each page's title names the pages you can step through with the page keys (Display,
    Controls, Interface, Presentation, Gameplay, Pause Menu). The page you are on is white and underlined in red. On a
    folder page (a page you open from a row) the page it belongs to is lit in grey.
  - **Folders.** A row that opens a page or one of the game's own screens has a blue name and a blue triangle in the
    value column (Brightness, Gamma, Buttons, Room 302, the Gameplay folders Player, Enemies, Powers, Randomizer). Rows
    that have a value **and** open a page with Enter (Subtitles, Film Grain, Shadows, Sound Mode) keep their value and
    arrows, with the blue name only.
  - **Folder titles.** A folder page's title shows where it is: "Gameplay > Player", "Interface > Effects",
    "Presentation > Room 302", "Display > Sound" (the last two levels, the page names above).
  - **Grey rows.** A row that does nothing while another setting is off is grey and cannot be changed; its description
    is one line that starts with the reason, then the row's own line when both fit (else the reason alone):

    | Row | Grey while | Reason shown |
    |---|---|---|
    | Grain Strength, Grain Type, Grain Colour (Effects) | Film Grain is Off | Needs Film Grain on. |
    | Blur Strength (Effects) | Depth of Field is Off | Needs Depth of Field on. |
    | Echo Count (Gameplay > Powers) | Echo Doubles is Off | Needs Echo Doubles on. |
    | Stereo Width (Display > Sound) | Sound Mode is Mono | Needs a Sound Mode other than Mono. |
    | Edge, Darkness (Interface > Shadows) | Shadows is Off | Needs Shadows on. |
    | Pause > Extra Menu > Camera: every setting but Camera | Camera is Game | Needs the Shoulder camera. |
    | Pause > Extra Menu > Camera: Wall margin | Wall zoom is Off | Needs Wall Zoom on. |

    (In the pause Camera page the reason has its own line and the row's own line follows on the next.) The rows
    Playable Eileen owns stay grey as before, with their own reason.
  - One verbose log line per page counts its menu glyphs (`options: page N (...): about G menu glyphs ...`; the game
    queues at most 800 a frame); `tools\logcheck.py` prints an `options look:` summary with the busiest page.
  - **Limits (v3.53.0):** the strip names today's six pages; it follows the real page cycle, so it becomes the plan's
    Display / Controls / Interface / Sound / Effects / Gameplay when round R4 builds that cycle. The strip's names cannot
    be clicked with the mouse yet. On a grey row under the cursor the game's two value arrows stay white (only the label
    and value turn grey). The strip, like every menu text, has a drop shadow: it costs about twice its letters of the
    game's 800 menu glyphs a frame (about 106 with these names: 53 letters).
- **Six themed Options pages** (v3.54.0, rounds R3 + R4 of the Options layout plan). The cycle is Display, Controls, Interface, Sound, Effects, Gameplay; every other setting sits one or two folders deep (Gameplay > Player / Combat / Items / Enemies / Powers / Randomizer, Controls > Camera > Steering, Controls > Room 302, Interface > Subtitles / Menus, Effects > Film Grain / Shadows). The pause camera's settings are in Options too (Controls > Camera). v3.55.0 moved the game's own rows too. See "Options pages (v3.55.0)" below.
- **Every Options row on its own page** (v3.55.0, rounds R5 + R6): the game's own rows moved to their themed pages: Sound Mode, Music Volume and Effects Volume to Sound, Invert Look / Head Bob / Left / Right to Controls > Room 302, the Subtitles, Film Grain and Shadows switches into their folders (which only open now), Blood Colour to Gameplay; Renderer, Menu Font, Start-Up and Interface > Pause Menu are back where the plan has them; a click on a name in the page strip opens that page; the pause Extra Menu says Eileen Outfit and its Camera page uses the Options names.
- **Folders open on a click, every value its own line, a clean pause list** (v3.56.0): a mouse click anywhere on a
  folder row (its name, its blue triangle) opens it, whatever row the cursor is on (it opened only on Enter before);
  every setting's value now has its own line under the rows (Supply Drops Off no longer says "Kills sometimes leave an
  item."); the pause Extra Menu says "Eileen Default" / "Eileen Nurse" and draws each row once (no dark second copy
  under the lower rows).
- **The value arrows stay put** (v3.58.0). On every Options page the white left / right arrows of a value row stand at
  the same place for every value of every row of that page: each page has one value box, as wide as its widest value
  (measured in the game's font; the HD menu fonts share its metrics), and every value is drawn centred in it. Pages whose
  values all fit the game's own box keep the game's arrows (x 344 / 464); a page with wider values grows its box to the
  right (the right arrow at most at x 500), then to the left, never into the row names. The mouse rects move with the
  arrows, so clicking an arrow again and again never misses; the volume bars keep their bars and their arrows line up
  with the page's box. Resolution's right arrow no longer jumps 20 px for the wide sizes. The pause Extra Menu > Camera
  page does the same: its values are centred in one room and its ">" stands at one place for every row and value.
  Hidden `[Options] ValueBox=0` brings v3.57's arrows back (Options and the pause page). **Limits (v3.58.0):** a value
  whose text changes with the value (a number row) and turns out wider than the page's measure grows the box once
  (logged); the page keeps the wider box until it is shown again; without the themed layout (it failed to build) or with
  one of the four old hooks not stock, the old arrows (logged).
- **Aspect Ratio in the game's Options** (`[Window] AspectMenu`, v1.3): Options > Option 1 gets an "Aspect Ratio" row
  (4:3, 16:10, 16:9, and since v1.6 the ultrawide 21:9 and 32:9) under Resolution, and Resolution lists six sizes of
  that shape (4:3 640x480 .. 2880x2160, 16:10 768x480 .. 3840x2400, 16:9 854x480 .. 3840x2160, 21:9 1720x720, 1920x800,
  2560x1080, 3440x1440, 3840x1600, 5120x2160, 32:9 1920x540, 2560x720, 3200x900, 3840x1080, 5120x1440, 7680x2160).
  The game's own sizes keep their place, so nothing
  changes until you pick another shape. The 3D view follows the real size (wider sideways in wide shapes, the original
  look in 4:3); menus and text stretch to the picture, as with GOG's own wide sizes. The window keeps its height and
  takes the picture's shape; maximised it is the largest window of that shape. The choice is saved with the game's
  options (a byte the game does not use), so your save also works without the mod.
- **Sound: Mono / Stereo / Headphones / Surround (3D)** (v3.52.0; Options > Option 1 > Sound, `[Sound]`). The game's own
  Sound row (Monaural / Stereo / Dolby Digital) has four values, kept in `[Sound] Mode`. The save still holds the game's
  own value (Mono = Monaural, Stereo and Headphones = Stereo, Surround = Dolby Digital), so the save works without SH4
  Refined; the first start takes the mode from the save. The mode is a setting of your PC, not of a save: loading a save
  made in another mode keeps yours.
  - **Stereo:** sounds glide smoothly between left and right. The game's own pan was very steep (a sound 20 degrees off
    the middle was already in one speaker only); now it follows an equal-power curve, and a sound is in one speaker only
    at the very edge.
  - **Headphones:** Stereo, and every sound also reaches the other ear (at most 9 dB down), as speakers in a room do.
  - **Surround (3D):** the game's Dolby Digital mode: DirectSound places sounds in 3D around the camera (5.1 / 7.1
    speakers, or Windows spatial sound on headphones).
  - **Mono:** every sound in the middle; stereo music and stereo streams are folded to one channel (the game's own
    Monaural dropped the right channel of stereo streams and left the music stereo).
  - **The Sound page** (press Enter on Sound): **Stereo Width** 0 to 100 % (steps of 10; 0 % puts everything in the
    middle); **Room Reverb** Off / On (the game's own room echo, 12 area presets, Konami's Waves Reverb; the PC port
    never switched it on; On by default); **Night Mode** Off / Low / High (quiet sounds louder, the loudest quieter, for
    playing at a low volume). Every row applies at once; a changed row is yellow, and "No" when you leave Options puts
    it back. **Fixed:** changing Sound and then answering "No" when leaving Options showed the old value but kept
    playing the last value you passed over (the game's discard skipped Sound). Exe sites: 0x4090BD / 0x40B617 (pan),
    0x4090A6 / 0x409285 / 0x40B5E7 (volume), the operands at 0x409AD0 / 0x409C63 / 0x41076F / 0x4125DA (reverb),
    0x40AB4A / 0x4115E8 (Mono), byte-checked, all or nothing per group. With the Extra Menu's Quick Load from the pause
    (below) the Sound mode keeper waits until the loaded room has its music back; the music comes back through the same
    volume path, so Night Mode and the Mono fold still apply. **Limits (v3.52.0):** in Surround, Stereo Width and the
    pan law act on the 2D sounds and the streams only (3D sounds are placed by DirectSound); Room Reverb applies to
    sounds started after a switch, and if DirectSound refuses a buffer with the reverb at the start (the log's `sound:`
    line) there is no reverb that session; Mono / back to Stereo: the music already in its buffer (about 12 s) plays out
    first; Night Mode narrows the range of the volumes the game sets (the music included), a sound's own loud peaks
    stay; Voice volume is not in this version (planned: in-game voices first); with another frame-rate mod (Refined's
    frame end not hooked) the first start's migration, the reverb probe (no reverb) and the mode keeper do not run.
  - **Background** (Sound page, v3.56.0): **Mute** (the game's own: its sounds are silent while another window is in
    front) or **Play** (the game stays audible behind other windows: its sound buffers get DirectSound's global focus).
    Play applies to sounds that start after the change; a sound already playing (the room's loop, the music) follows at
    the next room / the next track. Exe sites: one byte each at 0x4095FE / 0x409B02 / 0x40A14C / 0x40A723 (the flags of
    the game's four buffer makers), byte-checked, all or nothing.
  - **No silence after Alt+Tab** (v3.56.0): while another window is in front the game's sound is muted, and a wireless
    headset whose stream carries only silence can fall asleep and take seconds to wake when you come back. Refined now
    plays one inaudible keep-awake signal of its own (a 45 Hz tone at -72 dBFS, far below hearing) under the game's
    sound for the whole session, also behind other windows, so the device never idles. Hidden `[Sound] KeepAwake` /
    `KeepAwakeLevel`.
  - **Sound device** (v3.56.0): the game's DirectSound stays on the device that was Windows' default when the game
    started (as before). Refined now notices when the default changes (a headset connected later): the log says "the
    default sound device changed to ...; restart the game to use it", and Options > Sound > Sound Mode's line reads "New
    sound device: restart the game to use it." Following the new device without a restart is not built (it would re-make
    every buffer of the game's sound engine while it plays; planned only if wanted).
  - **Background Mute is silent at once** (v3.58.0). **Mute** is silent while another window is in front, at once and for
    every sound, also the ones already playing: SH4 Refined mutes the game's sound sessions in Windows (the game's
    DirectSound sound, the films' sound and the keep-awake tone) on every output device, and unmutes them when the game is
    in front again. Only what Refined muted is unmuted: if you mute the game yourself in Windows' volume mixer, it stays
    muted. **Play** keeps the game audible behind other windows, also the sounds that started under Mute. While the game is
    behind other windows under Mute, Windows' volume mixer shows it muted; that is Refined's. If the game is closed or ends
    while it is behind other windows, Windows keeps it muted: SH4 Refined unmutes it at its next start. The file
    `SH4Refined.muted` next to SH4Refined.ini names each sound session Refined muted (Windows' own name for it, the same at
    every start); the next start unmutes only those whose mute was confirmed there (v3.58 SD358-003: "muted", not "pending"),
    and only if they are still muted, then deletes the file (safe to delete
    by hand). A muted session it does not name is yours: it stays muted and the log says "the game is muted in the Windows
    Volume Mixer". If you unmute (or unmute and mute again) the game in the Volume Mixer while it is behind other windows,
    Refined leaves it as you set it. If Windows' sound
    sessions cannot be used (logged at the start), Mute is v3.56's: sounds that start after the switch are silent behind
    other windows, and the keep-awake tone stops there. A wireless headset that sleeps on silence may take a moment to
    wake when you come back under Mute (Mute is real silence, the keep-awake tone included); Play keeps it awake. Hidden
    `[Sound] SessionMute=0` = v3.56's Mute.
  - **Music Volume / Effects Volume 0 is silent** (v3.58.0). The game itself never set a sound below -60 dB (its own 0 was
    -60 dB, and Night Mode lifted that to -40 dB): every 2D sound, the music, every stream and 3D sound set to 0 is now
    silent (Room 302's stream that faded itself back up from 0 too: exe site 0x40B2E7). Every other level is the game's own.
    A pause's silence is real silence too. Hidden `[Sound] ZeroSilent=0` = the game's -60 dB.
- **Display Mode** (v1.5; in Options > Option 4 Presentation since v3.5.0): a "Display Mode" row: Windowed, or Borderless
  (the whole screen without a frame, black bars where the monitor has another shape). Alt+Enter switches too. The
  choice is kept in `[Window] Borderless`.
- **Monitor** (v1.7, with two or more screens; Option 4 since v3.5.0): a "Monitor" row under Display Mode
  (Display 1, Display 2, ... as Windows numbers them). Windowed opens centred on that screen, Borderless covers it, and
  the game's graphics device is made on it. The choice is remembered in `SH4WindowMod.ini` across game and PC restarts,
  also when you drag the window to another screen. If that screen is not connected, the game uses the main screen.
- **4:3 menus in wide shapes** (`[Window] Menus43`, v1.4): menus, text, the HUD and movies stay 4:3 in the middle
  while the 3D world fills the wide window (blood, lights, darkness and other world effects stay full width). Peep
  holes get black sides, cinema bars stay full width, the menu mouse follows the 4:3 menus. Based on ThirteenAG's
  Silent Hill 4 widescreen fix (WidescreenFixesPack, MIT). Since v1.6 full-screen darkening and black fills cover the
  whole picture (the darkened background while you examine an item, black backdrops and fades); pictures and the rest
  of the menus stay 4:3. Since v1.8 full-screen pictures (the memo reader, the Scrap Book, the item box, the map,
  puzzle close-ups, game over) get black sides instead of the 3D room showing beside them (`MenuPillars`).
  `[Window] Menus43Log=1` writes each new full-width 2D draw into the log once (diagnostic).
- **V-Sync, anti-aliasing, anisotropic filtering** (v3.12.0 rows; removed from Option 4 in v3.39.0): set them in GOG's
  own tool, GOG Galaxy > Manage > DirectX Settings (`dxcfg.exe`), which writes `dxcfg.ini`; Refined passes those keys
  through to GOG's wrapper. Old `[Graphics] VSync` / `AntiAliasing` / `Anisotropic` values are no longer used (a value
  that is not Default is named once in the log at start). With V-Sync on in `dxcfg.ini`, a Frame Rate above the
  monitor's refresh would run the game in slow motion, so gameplay is capped at the highest rate the monitor shows (60
  at 60-75 Hz, 120 at 120-165 Hz; none at 180 Hz and above), windowed and borderless too. There is no separate frame
  limiter: the Frame Rate row is it (the game paces itself); dxcfg.ini's `fpslimit` is left as it is.
- **Frame rate (v3.56.0).** The final fight's metal contraption in the blood pool turns at the game's own speed at every
  frame rate (it used to swing 2x / 4x / 6x as fast at 60 / 120 / 180). Henry's step onto a spear's spot in that room is
  as smooth as at 30.
- **Rate guard** (v3.56.0). With the true frame rate (FrameMode 0), the game runs one step a frame: a PC that cannot
  draw 180 (or 120) frames a second runs the whole game in slow motion. When the play frames take more than 1.15 x their
  time for two 3-second stretches in a row (menus, cut-scenes, loads, doors and the game out of front do not count),
  Refined runs gameplay one step lower (180 -> 120 -> 60; never below 60) and writes it in the log. The effect budget
  thins the power effects first; the guard waits while it is still doing so, and steps at once when the budget has
  nothing left to shed. After 10 minutes without a slow stretch at the lower rate the higher rate is tried again at the
  next room change; a rate the guard had to leave twice stays off for the rest of the session. The Frame Rate row and
  `[Window] FrameRate` keep your choice; choosing a rate in the row starts the guard again. Hidden `[Window]
  RateGuard=0` turns it off.
- **Renderer** (v3.39.0, Options > Option 4, with SH4 Graphics Plus installed): Direct3D 11 / Direct3D 11 (16-bit) /
  Direct3D 12 / Stock, the dgVoodoo2 configs SH4 Graphics Plus put in `dgVoodoo_variants\` (the choices are the rows of
  its `manifest.csv`; their md5s are read from it). Leaving Options with "Yes" copies the chosen config over
  `dgVoodoo.conf` (written as `dgVoodoo.conf.new` first, its md5 checked, then moved over the old one); it applies at
  the next start. The row is greyed ("Unavailable") when `d3d9.dll` is not one the manifest allows (its
  `#d3d9.dll=<md5>` lines; without any, dgVoodoo2 2.87.6's CAA75C55...) or the manifest is missing or damaged, and shows "Custom", greyed, when `dgVoodoo.conf` is not one of Graphics Plus's (it is then never replaced);
  a Graphics Plus test config shows as "Graphics Plus: <name>" and can be replaced. Nothing is written into
  `dgVoodoo_variants\`, `d3d9.dll` is never touched and `dgVoodoo.conf` is never created; a `dgVoodoo.conf.new` left by a replace cut
  short is deleted at the next start. Notes (Graphics Plus's
  measurements): Direct3D 12 has 260-530 ms freezes entering rooms and uses about 200 MB more of the 2 GB address
  space; 16-bit adds about 1.7 ms from present to display and was tested on one AMD PC only; Stock (dgVoodoo2 passing
  through) measured the same as the system Direct3D 9; the frame spikes of Death Presence in Room 302 are not explained
  yet. The log names every change, old -> new, with md5s (`renderer:`). Display Mode's description names the running
  config (e.g. "Direct3D: GOG -> dgVoodoo2, Direct3D 11 (16-bit)").
- **Opening** (v3.39.0, Options > Option 4, `[Window] Opening`, applies at the next start): Full (the game's own: the
  KONAMI and KCET logos, the notice, the opening movie on a first start, the long title intro), **Skip Logos** (the
  default: straight to the title, about 12 s sooner; the screen gamma is still applied) or Fast (also no first-start
  movie, and the short title intro: 2.7 s instead of 8.3 s). Exe sites 0x42CD6A (the boot screens' call), 0x42CDA2 and
  0x42CDAB (Fast only), byte-checked, all or nothing; Full patches nothing.
- **Room 302** (v3.50.0, Options > Option 4 > Room 302, a folder row above Exit; `[Room302]`): Room 302's first-person
  look and walk with one set of settings for Henry and Eileen alike, taken over from Playable Eileen: **Look Down** (how
  far down the view goes: Game = 50 degrees, 60 to 85), **Look Up** (Game = 25, 35 to 85), **Look Feel** (the turns and
  tilts ease in and out: Off = the game's own, 0.5 to 2) and **Walk Speed** (Room 302's walking speed: 0.5 to 1.2, Game
  = 1; on top of Movement Speed). The pitch stops exactly at the limit (no overshoot past straight down or up). On the
  first start the values come from Playable Eileen's own [Room302] (LookDownLimit, LookUpLimit, Feel, WalkSpeed), so
  Eileen feels the same. Her body in first person, eye height, arm reach, side steps and the body camera's feel stay
  Playable Eileen's. **While Playable Eileen keeps these (every version up to v1.24.4: it has no hand-over yet), the
  four rows are grey with "Playable Eileen does this for now." and Refined changes nothing in Room 302**; a Playable
  Eileen build that hands them over (its SH4PE_Room302Yield export, planned for PE v1.25.0) lets Refined act for both
  characters. Without Playable Eileen Refined does it for Henry. All apply at once (and are saved at once); "No" at the
  Options exit question puts them back. Exe sites (only while Refined owns the look): 0x52DF74 (the call of
  Room302LookApply), the operands of 0x52DCC6 / 0x52DD14 (the limits), byte-checked, one set with rollback. **Limits:**
  nothing changes with Playable Eileen v1.24.4 (the hand-over needs its next version); Look Feel for Henry is new
  (Playable Eileen eased only her look): Off gives v3.49's look back; stage 0x0A (the crawl through the hole) keeps the
  game's look and walk; eye height, her first-person body, arm reach and the body camera stay Playable Eileen's (Eileen
  only).
- **Option 5 (Gameplay): Player and Enemies pages** (v3.25.0): Option 5 holds Item Shine, Player, Enemies and Exit;
  Enter on Player / Enemies fades to its page (like Button Config), Back / Cancel return. Player: **Movement Speed**
  (Normal, x1.25, x1.5, x2: her walk and run, the animation follows; Room 302's walk too; x2 may pass through thin
  walls), **Medallion Durability** (Normal, x2, x3, x4 against ghost contact), **Regeneration** (v3.47.0: SH4 Refined's
  own, always on the page, see the next bullet; up to v3.46.0 only while Playable Eileen was loaded, as the front end of
  its `[Defense] Regen` / `RegenRate`). Enemies: **One-Hit Kill** (Off / On: your hits, melee, guns and Playable
  Eileen's weapons, kill monsters at once; ghosts fall down for about 30 s as after any finishing blow; Walter, the
  Conjoined Bodies and the Wall boss take normal damage). Every row applies at once, turns yellow when changed and is
  undone by "No" at the Options exit. `[Gameplay]` keeps them (read again while the game runs). v3.27.0 adds a
  **Special** page (Option 5: Item Shine, Player, Enemies, Special, Exit); v3.29.0 adds rows to all three pages (below).
  Every new row defaults to Off. v3.48.0 adds the **Randomizer** folder under Special (below; its Items row defaults to
  Random); v3.49.0 adds its Monsters, Eileen Outfit and Ghosts rows (defaults This World, Random, Spots). v3.50.0 adds
  the Enemies page's **Enemy Pursuit** and **Fade In** rows (defaults Normal, Fade + Portal), the Monsters value All
  Worlds, and Option 4's **Room 302** folder (below).
- **Regeneration** (v3.47.0: SH4 Refined's own; Option 5 > Player; `[Gameplay] Regen`): Off, x1, x1.25, x2 = 15 / 18.75 /
  30 % of your max health a minute (Playable Eileen's own rates), after `RegenDelay` seconds (10 by the user's ini,
  10-120) without damage, up to full health; never while paused, in events, doors or cut-scenes, at 0 health, while you
  stand in a ghost or haunting aura, or in Room 302 while the room's own healing runs (it runs there afterwards). For
  Henry (no Playable Eileen, or its switch Off) and for Playable Eileen while her own regeneration is off. While hers is
  on (PE v1.23.2's `SH4PE_RegenActive`), Refined's row is grey, shows Off and Refined does not heal. With a Playable
  Eileen before v1.23.2 the row is still the front end for its `[Defense] Regen` / `RegenRate` (v3.46). No exe patch:
  Refined's 0x4EF982 tick, heals through the game's PlayerHealAmount 0x52D1D0. Life Steal's heals go to 0x52D1D0
  directly too, except while Playable Eileen's own regeneration is in charge (then through the game's heal path, so its
  guard keeps them). **Limits:** no healing near a ghost aura even with the Saint Medallion worn (the aura's radius
  counts, not its damage); the rate and the wait follow the game's tick rate ([0x5D5248]), so the hidden `[Window]
  RateProbe` test mode makes them wrong, as it does for the game; a brand-new SH4Refined.ini (first install) writes
  `Regen=Off`, `RegenDelay=30` (the copy from Playable Eileen's ini is made only when an existing ini lacks the keys);
  it heals the player's body only (not companion Eileen or Cynthia).
- **Death Presence** (v3.27.0, look reworked in v3.27.1-v3.29.0; Option 5 > Special, `[Gameplay] DeathPresence` Off /
  Weak / Strong): a black-violet fog round you that drains every being in it, more the longer it stays (Weak: radius
  300, 2 % of max health a second rising to 8 % after 10 s; Strong: 400, 5 % to 20 %). A monster brought to 0 dies its
  own death; ghosts go down and stay down while inside; bosses take nothing; enemies hesitate at its edge. Companion
  Eileen takes a real haunting at a quarter of that rate and keeps out of it; Cynthia recoils (no damage); in Room 302
  it slowly clears the hauntings. The look is 3D: a floor pool reaching her feet, soft smoke puffs filling a dome to hip
  height, a black "cloak of death" from toes to shoulders that frays into the pool, violet tendrils round her body, a
  thin black-violet fiery trail behind her (v3.28.0, purple since v3.29.0), and a slowly fading stain on every being it
  wraps (fades in over about 4 s). Its sprites are depth-tested at the depth the game really stores (v3.27.3), so it
  shows at any camera distance. Each filter shows it its own way: Spirit Vision a brighter violet glow, Thermal a black
  void, night vision a pitch-black cloud with green-white filaments.
- **More gameplay rows** (v3.29.0, reworked in v3.30.0; all Off by default). Special: **After Shock** (v3.30.0, was
  After-image; Off / Short / Medium / Long: while you move, 3 / 5 / 8 crackling electric copies of you trail behind over
  0.2 / 0.35 / 0.6 s; an enemy that touches one gets the Stun Gun's own hit, then a cooldown; not in Room 302), **Echo
  Doubles** (v3.30.0, was Echo; Off / Shadow Copy / Shadow Cast: dark shadow soldiers (near-black, violet rim and eyes,
  black-violet flames) repeat your swings, stomps and shots; Shadow Copy stays beside you with a drifting random delay;
  Shadow Cast sends each shadow through smoke to an enemy on your floor while you attack, to strike it from a random
  side; enemies react to their hits but cannot target them; 75 / 60 / 50 % of your damage each for 1 / 2 / 3 shadows,
  One-Hit Kill applies, Dark Pact and Life Steal do not) and **Echo Count** (1 / 2 / 3). Enemies: **Enemy Copies** (Off
  / x2 / x3 / x4: every monster and victim of a room you enter gets copies near it; never bosses, Wall Men, Tremers or
  the story ghosts; copies are not saved and do not count for the kill rank), **Enemies Follow** (Off / Monsters /
  Victims / Monsters + Victims (v3.50.0: Victims was called Victims Only; an ini with `EnemyFollow=Victims Only` still
  reads): up to 6 monsters or copies (v3.34.0; was 3) within 800 units follow you through a door, the first after 3 s;
  never through lifts, holes or into Room 302; v3.49.0: **Victims** = only the victim ghosts (and, as with Monsters +
  Victims, the story ghosts and the ghosts' copies) follow you; monsters and their copies stay in their rooms, and any
  that followed you earlier go home at the next room load. **Limits:** the Twin Victims (08) and the Patient (11) are
  monsters to the game and stay; Victims needs the ghosts' room-rule site 0x486199 (changed by another mod: nobody
  follows, logged); a monster on its way through a door when you switch stops at once and goes home at the next room
  load), **Enemy Health Bars** (Off / Locked Target / All Near; ghosts show their down timer, bosses a wider bar;
  v3.30.0: a bevelled look with a damage chip, ghost bars that show their real health, a violet revive bar while down,
  sealed by the Sword of Obedience), **Supply Drops** (Off / Normal / Generous: killed monsters sometimes leave an item;
  v3.30.0: rolled anew after every load, New Game and game start; a rare Saint Medallion, Sword of Obedience or Ampoule
  when one of the four named ghosts or the stalker Walter is fully downed), **Enemies Return** (v3.30.0 as Revive
  Enemies Off / On; v3.56.0 Off / By Room / By World: below), **Fade In** (v3.50.0, Off / Fade / Fade + Portal, default
  Fade + Portal; the row is **Reveal Method** since v3.58.0, values Off / Fade In / Portal, the ini words unchanged: monsters and victims that appear in a room no longer pop in. Each one fades in over 0.8 s, from
  invisible to whole. That covers the room load, followers coming through the door, wanderers, Enemy Copies, revived
  monsters, and ghosts or monsters the game brings in later. The fade uses the game's own see-through enemy draw, the
  one its ghosts use when they pass through walls. A monster's shadow joins when it is half in. With Fade + Portal, a
  ghost that arrives after you entered also rises from a blood-red portal of Refined's own. Never faded: bosses, Wall
  Men, Walter, event and cut-scene figures, dead bodies, and a ghost that comes out of the game's own wall portal. A
  fading enemy acts as usual. **Limits:** a fading enemy can move and attack while see-through (the stock pop-in also
  acts at once; at 0.8 s the first attack of a follower that comes through the door may land while it is still half
  see-through); see-through, not a silhouette: the game's draw has no depth write, so during the 0.8 s a monster's far
  parts can show through its near parts, as the game's own ghosts do in walls; enemies standing in the room when you
  enter fade in while the game's own screen fade-in runs and get no portal (only ghosts arriving later do); v3.50's
  portal was Refined's own additive crimson vortex (v3.51.0: the tar portal, below; the vortex stays with the hidden
  `FadeInPortalDraw=Vortex`), depth-tested against the room, so walls hide it, and it brightens, never darkens;
  Thermal's heat redraw and Spirit Vision's glow redraw the bodies at full strength over the fading draw; first sight is
  per room visit (an enemy seen while Fade In was Off does not fade when it is switched on in the same visit; a load, a
  quick load or a door starts a new visit); without the screen effects' model hooks, or when 0x45C880 is not the stock
  code (another mod), Fade In is off, the log says why and the row says "Not available"). **Fade + Portal: the tar
  portal** (v3.51.0): a ghost that arrives while you are in the room now tears free of the game's own sticky tar portal,
  the one its victims come out of walls with: a dark blot with runny black drips, and thin dark strands stretched from
  the blot to the ghost's body that snap one by one as it pulls free. The portal sits on the wall behind the ghost (up
  to 180 units behind it, as the camera sees it), else it hangs in the air just behind it. The blot and drips are drawn
  by the game's own wall-portal draws (0x4868B0 / 0x486E80) from blocks of Refined's own, right after the game's own
  effect pass; the strands are the ghost's own (victims and Cynthia have them). The game's portal grab and damage are
  never made. The ghost is held back while the strands hold: 40 % of its speed for 1.5 s, easing to its full speed by
  2.5 s, and it cannot grab or attack you until then (Enemy Pursuit leaves it alone meanwhile). When the game's portal
  textures are not loaded (or the game's draw ever fails), Refined draws a tar portal of its own: near black with a deep
  red core, black drips, 12 strands. Fade alone and Off are unchanged; bosses, story scenes and ghosts that come out of
  the game's own wall portals are left to the game. Row description: "They fade in; ghosts tear free of a tar portal."
  **Limits (v3.51.0):** seen only in the code, not yet in the game: the game's own portal draws are called at the moment
  the game calls them (after its pass 2) with blocks of Refined's own; the first live test should confirm the look, the
  depth and the drips; the portal of a ghost far from any wall hangs in the air behind it; the hold slows the ghost's
  walk across the floor, not its animation; while held, the ghost's deferred actions are refused (its grab and attacks,
  also a state change it asked for); its haunting (the headache near a ghost) is not held back; Jasper, Andrew and
  Richard have no strands of their own: they get Refined's strands with the game's blot; a ghost the game itself sends
  out of a wall (its own portal) is let go at once and its tar portal closes. Player: **Ammo Saver** (Normal / Saver /
  Endless, pistol and revolver only), **Dark Pact** (Off / Pact x1.5 / Blood x2: your hits and the damage you take, both
  ways), **Grab Breaker** (Stock / Quick / Instant against ghost and Sniffer Dog grabs), **Tireless** (no running out of
  breath), **Life Steal** (Off / 5 / 10 % of the damage you deal to monsters heals you), **Curse Meter** (Off / On /
  Companion: Eileen's curse split into its stages, since v3.30.0 a ring of thorns; with Playable Eileen loaded, On shows
  her own curse).
- **Reveal Method's portal is as big as the one who comes out of it, and its tar sticks** (v3.58.0; the user's QA of
  v3.57: "it is smaller than the victims. Make it larger, not by stretching it, but by add to the effect with more similar
  effects on and around it. Make stretchy tar-like substance stick to the victims for a few seconds and let the portal stay
  open for that long too."). When a victim or a story ghost arrives through a tar portal, the arriving body is measured
  (its skeleton as it is drawn; Andrew's bulk, a crouching victim and Playable Eileen's world alike) and the portal is laid
  out to about 1.24 x its height: the first portal at its old size, with up to nine more of the same around it (below at
  the feet, above the head, at the sides and corners, one over the middle), each a smaller copy of the first and opening
  one after another, plus two more sets of tar blots and drips spread over the whole. Nothing is stretched: every piece
  and every blot keeps its own size. On a wall a piece that would hang past the wall's edge (a door, a corner) is left out.
  The game's own wall-portal art is used when it is loaded (as in v3.51); Refined's own near-black version otherwise.
  - Thin black strands of tar run from the portal to the body (16 of Refined's own, or 8 beside a victim's and Cynthia's
    own 32), with a blob of tar on the body where each holds on, running down a little. They follow the body as it moves,
    sag more and get thinner as they stretch and age, and hold on for 3 seconds after the body is fully in. Over their last
    second and a bit they snap one by one: the wall end whips back into the portal and the body end drops off. A strand
    pulled too far (900 units) snaps at once. The portal stays open as long as the strands hold, then closes: the pieces
    around it first, so it shrinks into its centre and fades (about 5.4 s after it opened).
  - What does not change: the arriving ghost is still slowed (40 % for 1.5 s, full speed at 2.5 s) and cannot grab or
    attack for 2.5 s, and Enemy Pursuit starts its timers when that hold ends; Off and Fade In are as before; monsters keep
    their fade (the game itself has no portal for monsters: its wall portal belongs to the ghosts' wall exits and a few
    scripted ghost scenes); a ghost that the game sends out of its own wall portal is left to the game.
  - At high frame rates the extra pieces thin first: the effect budget's spark levels (60 / 35 %) and, on their own, the
    portals whenever their drawing takes more than 8 % of a frame (0.44 ms at 180 fps): the first portal, its own blots and
    every strand always stay. A portal is no power for the budget's start step (above).
  - Also fixed: Jasper, Andrew and Richard (who have no strands of their own) got none of Refined's either in v3.57 (the
    user's log: "strands: none 0"); their strands now start once their body is drawn.
  - Hidden `[Gameplay] FadeInStrandSeconds` (3.0), `FadeInPortalPieces` (9), `FadeInStrands` (16); the first two 0 =
    v3.57's size and timeline.
- **Enemies Return** (v3.30.0 as Revive Enemies; v3.56.0: Off / By Room / By World): when killed monsters come back.
  **Off** is the game's own rule: a killed monster lies dead until you leave its world through the hole, and comes back
  at your next visit; some stay dead for good (Twin Victims, about 1 Tremer in 10, and old bodies the game's record pool
  reuses), and about 4 Tremers in 10 stand again at the room's next visit. **By Room** (v3.30 - v3.55's On): a room's
  killed monsters and its killed copies stand again each time you enter the room. **By World**: killed monsters stay
  dead for the rest of the world visit (Tremers too) and all stand again when you enter the world again (through Room
  302's hole, or when the story moves you to another world). A load (Quick Load, the Load Game list, a Continue) is
  never a room change or a world entry by itself: a Quick Save and Quick Load brings nobody back early, and a save made
  in the world keeps its dead as they were when you saved (killed after the save: standing again after the load, as that
  save had them). Never returning: bosses, Walter, the Wall Men, Wheelchairs, the victims' ghosts (they never die; their
  own down countdown), story ghosts, and the rooms of story events (logged once each). Enemy Copies' copies follow their
  room: By Room brings a room's killed copies back with it; By World and Off keep them dead until you leave the world.
  Revived kills count for the ending's rank (copies never do). The world-visit rule is the one the Randomizer's Reroll
  Visits uses (`src/worldvisit.inc`).
- **After Shock lets you see the player in the over-the-shoulder camera** (v3.56.0; the user's request 9). In the
  Shoulder camera the trail of a run lies between the camera and the player, and every copy there was added over her
  body until she turned white. Now a copy that is nearer the camera than the player and covers her on screen keeps only
  30 % of its light (the nearer and more centred, the dimmer; the streaks along her path the same). Copies beside her on
  screen (when she strafes, turns or side-steps) and copies behind her keep their full glow, and the game's own cameras
  show the trail exactly as before. The size of "her" on screen is read from the body drawn that frame, so it fits
  vanilla Henry (broader) and Playable Eileen alike. The change fades in and out over 0.2 s when the camera switches.
  The stun is not changed: copies still stun what they touch, however dim they are drawn.
- **The powers step down when 120 fps or more cannot be held** (v3.56.0; the user's request 10). At a gameplay rate of
  120 frames a second or more (a true logic rate: every frame that runs late slows the game), the effect budget now
  counts a frame as late from 6 % over its time (was 25 %) and acts within about a second. First it thins the sparks,
  arcs and smoke of the powers (After Shock's lightning, sparks and streaks, Death Presence's trail fire and wisps, Echo
  Doubles' light look) to 60 %, then 35 %, before it touches any body redraw (the v3.35 levels follow, as before; their
  numbers 1 - 9 are now 3 - 11, the log names each). When frames run late while the powers cost little processor time
  (their graphics-card cost cannot be measured), it tries a step and keeps it only if the frames get at least 3 %
  shorter within 1.5 s; otherwise the step is taken back at once and not tried again for 20 s (then 40, 80, up to 120
  s). Everything comes back after a calm stretch. What a power means is never thinned: After Shock's copies and stuns,
  Death Presence's fog and its drain area, Echo's shadows' bodies (by these new levels) and Ghost Teleport's fade (it
  draws nothing extra) stay. Below 120 fps the budget works exactly as in v3.55. When nothing is left to shed, the log
  says so, and the frame-rate guard (above) then lowers the rate without waiting.
- **Powers starting together at 120 / 144 / 180 fps** (v3.58.0; the user's v3.56 QA item 13). When two or more powers
  (Echo Doubles, After Shock, Death Presence, Ghost Teleport's fade) start within a second of each other at a high frame
  rate (you turn them on in the Options, or come back to them after a long while), the effect budget thins them at once
  instead of after late frames, then brings them back a level every half second while the frames hold. They build up
  over a few seconds; the game no longer runs in slow motion while the budget catches up (the log's case: about 11 s of
  slow motion in v3.57, under 1 s in the simulation). Frames that run far over step the budget every quarter second (was
  one level a second). When the budget has nothing left to thin, the rate guard judges the last second of play (was two
  windows of 3 s) and steps 180 -> 120 (-> 60) at once. As before: never across a load, a pause, a capture, a door or the
  game out of front; back up after 10 minutes clean at a room change; `[Window] FrameRate` never changes. Reveal Method's
  portals are no power: they have their own limit (below). Hidden `[Graphics] EffectBudgetQuick=0` = v3.56's budget and
  guard.
- **Enemy Health Bars: which enemies get a bar** (v3.52.0; Option 5 > Enemies > Enemy Health Bars). A bar is drawn only
  for an enemy the game itself draws in this room: not for one that is still on its way in, hidden or a placeholder (the
  states 0, 0x0D, 0x0E and 0x0F of the enemy's work; the game's own draw test 0x45CA40), and not for one whose position
  is the origin, parked away (y 65536 or more) or not a number. This removes the bars that floated over the player in
  tiny rooms (the Building World elevators, the Subway's driver's cab 2:0F): they belonged to victims' ghosts that
  Enemies Follow had let into a room the game keeps them out of. The same rule keeps bars off a Gum Head under the
  floor, a Wall Man inside the wall and Walter before he comes out. A ghost that Enemy Pursuit keeps calm keeps its bar
  while the game draws it. **Bosses:** the bar of a boss (Walter, the Conjoined Bodies, the Wall boss) shows whenever
  the boss is up and in view, at any distance, in Locked Target too; when a body's top is above the screen and its
  middle is on screen, the bar sits at the top edge. Verbose lines for every refused bar and each boss's first bar;
  `tools\logcheck.py` sums them (`enemy health bars: ...`). No new ini key, no exe patch. **Limits (v3.52.0):** D1's
  cause (Walter's missing bar) is not proven from the 2026-11-26 log; the two likely causes (All Near's 3D distance, a
  bar above the top of the screen) are fixed and the boss line shows which it was; a victim's ghost that Enemies Follow
  lets into a tiny room stays invisible on the player for the visit (the game's arrival code finds no wall spot; only
  its bar is gone); at most 32 enemies tracked, the nearest 8 bars drawn.
- **Enemy Health Bars: no bar before the body, Walter downed** (v3.58.0). A bar shows only for an enemy the game really
  draws: never at the spawn points of enemies that are not out yet (the Subway's first dogs before their cut-scene, any
  enemy whose task draws nothing yet). With Reveal Method's fade the bar fades in with the body. **Walter downed keeps
  his bars**: knocked down (even at 0 health), Walter keeps his bar at any distance, with the violet revive bar under it
  counting down the game's own time until he gets up (the stalker in the second half: 0.5 - 60 s; the final Walter after
  the eighth spear: 3 - 5 s). While he plays dead (he waits for you to come near) the countdown is not drawn. Only his
  death or a script takes the bar away.
- **Enemy Pursuit and followers across saves** (v3.50.0; Option 5 > Enemies, `[Gameplay] EnemyPursuit`; a new row under
  Enemies Follow): how long the enemies that follow you keep track of you. **Relentless**: as v3.38 - v3.49: once a
  monster spots you in a room it follows through your next door however far away it is, and every pursuer pursues you
  for the whole world visit. **Normal** (default): a monster follows you through a door only if it sensed you (came near
  you, saw you, or was hit) in the last 10 s of play before you went through. A pursuer lying down when you leave (you
  downed it and ran) stays behind. A pursuer that does not catch up with you for 2 doors in a row stays where it is.
  Ghosts sense you through walls (every ghost in the room still pursues), but a downed ghost stays behind too.
  **Forgetful**: as Normal with 4 s and 1 door. An enemy that lost track of you stays in its room and lives by the
  game's own AI; it can spot you again. **Followers survive a save and its load**: the pursuers, the followers on their
  way through your last door, the victims' ghosts with you, and the copies that followed you come back after you load
  that save (Load Game, Quick Load, the death Continue), in the room where they were when you saved. Refined keeps this
  in its own file `save\SH4Refined_follow.dat` (the game's save files are unchanged; the last 64 saves with followers
  are kept). A save made before v3.50, or without followers, loads as before. Exe sites 0x4FC1A4 / 0x4FC6ED (the
  snapshot's EnemyManagerSave / EnemyManagerLoad calls), byte-checked, all or nothing (hidden `[Gameplay]
  FollowSaveFile=0` leaves both stock). **Limits:** a follower whose monster the Randomizer changed is left out of a
  save by the Randomizer's save filter (as in v3.49): after the load it is back at its own placement as placed and does
  not pursue; a copy that followed you comes back with the state it had when it came into the room (a standing monster
  copy: its place and health at the save); the ghost copies' kept states in their own rooms are still dropped at a load
  (they come back fresh); only the last 64 saves that had followers keep them, an older one loads as before; the side
  file is matched by the save's content: two saves made at the same moment share one entry (they hold the same state).
  **Inside the room too** (v3.51.0; v3.50's rules acted only at doors: a monster or a victim's ghost that had spotted
  you chased you for as long as you stayed in the room). With Normal or Forgetful, **monsters** (Hummer, Gum Head,
  Sniffer Dog, Twin Victim, Wall Man; their copies too) that are chasing you and have not sensed you for **10 s**
  (Forgetful **4 s**) give up and go back to their own routine: the Hummer flies off to wander, the Gum Head stands or
  crawls about, the Sniffer Dog gives up the way it does on its own and wanders (its own short cool-down before it can
  sniff you out again), the Twin Victim wanders, the Wall Man sinks back into its wall. "Sensed" = you are within its
  sense range with nothing in between (a line test against the room's walls), or close to it, or you hit it. One that
  lies down while you walk out of its senses forgets you at once. With Forgetful it also ignores you for 3 s after
  giving up, unless you hit it or come within 500 units. Afterwards it can notice you again by the game's own rules.
  **Victims' ghosts** sense you through walls, so they go by distance and time instead: one farther than **2500** units
  from you for **12 s**, or chasing you for **25 s** without touching you, gives up, floats back towards the spot it was
  made at and rests for **8 s**; a hit, or you within 400 units, ends the rest. After the rest (v3.52) it stays calm
  until it senses you again: you within 2000 units (the game's own sight range) with nothing between, or a hit, its
  touch, you within 400. **Forgetful (v3.52)** lets them go much sooner: a ghost gives up when it is farther than
  **1200** units for **3 s**, after **8 s** of chasing without touching you, or when you are **out of its sight for 3
  s** (a wall or other collision between you and it); it rests **15 s** and then stays calm until it senses you again
  within 1200 units (or a hit, its touch, you within 400). While resting or calm a ghost cannot dive into a wall to come
  out next to you: such a dive is set back to its idle float (on Normal too), and ghosts are never handed to the game's
  own rules by the quick re-notice guard. A calm ghost does not follow you through the next door. A ghost that Fade In's
  tar portal holds (below) is left alone until it is free: its timers start then. One that gave up and has not taken the
  chase up again does not follow you through your next door. **Relentless** is the game's own behaviour (nothing changes
  inside the room). Never touched: anything that holds you or is attacking (grabs, the Hummer's latch, bites), enemies
  lying down, pinned ghosts (the Sword), bosses, the story ghosts (Cynthia, Jasper, Andrew, Richard, Walter), event
  spawns, the Tremer (it never chases: it only bursts when you are near). Description lines: "Normal: unseen 10 s, they
  quit, in rooms too." / "Forgetful: unseen 4 s, they quit, in rooms too." Hidden `[Gameplay] PursuitInRoom=0` = v3.50
  (doors only). No exe patch: only stock functions are called. **Limits (v3.51.0):** not run in the game yet: the
  per-family states come from the decompilation (matched code for most; the Twin Victim's wander, the dog's hunt and the
  ghost's homing are read from unmatched code); the victims of types 0x0C v1 / 0x0D v1 (they run Cynthia's and Jasper's
  code) keep the v3.50 door rules; Gum Heads on a wall perch and the floor Wall Man keep the game's own chase; a dive
  that has already reached its wall node (its portal open) cannot be set back and still ends the rest (rare); the
  set-back writes the idle float's step after EnemySetAiState (matched code, no live run yet); a calm ghost floats
  towards its spot without its idle poses; the sight test is one straight line (glass or thin props may read as a wall);
  the sense test has no view cone (a monster gives up a little later than the game would, never earlier); if the game
  keeps noticing a monster right after it gave up (twice), Refined leaves that one to the game for the room visit
  (logged; never a ghost since v3.52).
- **The randomizer's victims follow too** (v3.58.0). With the randomizer's Enemies on Same World or All Worlds a monster's
  placement can become a victim's ghost. Such a victim now follows you through doors (Door Chase Victims or Both) and is
  judged by Pursuit inside the room exactly as the game's own victims are (the male victim 0B; the 0C / 0D victims chase
  by the game's own rules in the room, as the stock ones). v3.56 / v3.57 left them out ("of no following kind"). A copy
  of Extra Enemies drawn as a victim's ghost follows by the ghosts' rule as well (and gets a bar only once it is drawn).
  The log now says, at the next door, what became of the ghosts that followed you into a room (came in, still arriving
  through the walls, never in the room).
- **Ghost Teleport** (v3.43.0; Option 5 > Special, `[Gameplay] GhostTeleport`, default Off): press Run + Action together
  to warp like ghost Richard (his sound, his see-through fade: 0.5 s out, the warp, 0.5 s in) behind an enemy, or ahead
  the way you face. v3.49.0: **keep both held to keep warping**: the next warp starts as soon as you have faded back in;
  no cooldown (the hidden `GhostTeleportCooldown` is 0 now, was 3 s). A warp never takes you across or into the trigger
  zone of a story event the game would still play when you walk there (a cut-scene box, a walk-in trigger): such a spot
  is refused and nearer spots are tried; it never starts while a door, a room change, an interaction or a screen fade is
  under way. No exe patch. **Limits:** a zone whose own condition would not fire it is kept out of all the same (more
  refusals, never fewer); a room that keeps the screen partly faded in play would refuse every warp (none known; the log
  says "the screen is fading"); while held, Run makes her run during each fade-out and the next spot is searched from
  where she is then; No Clip can still walk around an event trigger through walls. v3.50.0: **behind the enemy you
  face**: the standing enemy nearest your facing inside +-35 degrees, from up to 5000 units (was the nearest one within
  2500, wherever it stood); never another enemy (if no spot behind it is free, no warp). With no enemy in that cone the
  warp goes **ahead, through walls**: past a wall, a turnstile, a fence, a counter or a gap in the floor, onto the
  nearest spot behind it where you can stand (the game's floor, room for your body and head, inside the room, not into
  the next room, not across or into a story event's trigger) and that a route of the room reaches (its floor is reached
  from one of the room's doors, or from a place you stood in this room). Nothing in the way: up to 800 units ahead as
  before. During the fade-out a press of Action no longer opens an examine message that calls the warp off. Row
  description (Off): "Warp behind the enemy you face, or ahead through walls." **Limits (v3.50.0):** the floor map holds
  one floor per 60 u cell: where two floors lie over each other, a spot on the one reached later is refused (never
  accepted wrongly); a spot past a wall that only a locked door or a scripted event leads to still counts as reached
  when the door's point is on that side; a barrier lower than 60 u is stepped over by the floor map (the player climbs
  40 u stairs), and a thin rail between 60 and 110 u above the floor with nothing below it may let the map pass under
  it; after a warp through a wall your companion (Eileen, Cynthia) stays on her side until she finds a way or a door
  takes you both; the first through-wall try in a big room may take a few milliseconds more while the floor map is built
  (up to 3000 cells at the press, 600 a tick through the fade-out, 3000 more at the warp); a mouse left click during the
  fade-out can still open an examine (only the pad / key presses are taken back). **Only enemies in the open**
  (v3.51.0): the enemy she faces counts only when nothing of the room stands between her and it: a wall, a prop, a
  turnstile, a counter, a fence or a window is "collision in the way". An enemy seen only through collision, one inside
  a wall, or a victim / story ghost coming through a wall (its own wall portal open) is skipped: she warps behind the
  next enemy in the cone that she has a clear line to, and with none she makes the normal warp ahead (through the wall
  onto playable floor, or up to the wall), exactly as with no enemy in front of her. Hidden
  `GhostTeleportBehindCollision=1` gives v3.50's rule back. **Limits (v3.51.0):** the test is one straight line from her
  chest to the enemy's chest (and, on one level, a low line 60 u above the floor for turnstiles and counters); on
  another level (stairs, a raised floor, more than 40 u up or down) only the chest line is tested, so a low rail between
  her and an enemy on another level does not count; characters never block the line (an enemy behind another enemy still
  counts); a ghost counts as "in its wall" while its own wall portal is open (opening, or up to about a second after it
  came out), even if part of its body is already in the room. **Every landing is proven** (v3.52.0): behind the enemy
  she faces, ahead the way she faces and through a wall, every landing spot passes the same test by the player's own
  collision and walking rules. Her body (three spheres from her knees to her head) must touch no wall or prop there, and
  the room's floor map must reach the spot. v3.43 - v3.51 tested only the floor and a line of sight for the warps behind
  an enemy and ahead, so a spot 20 u from a wall could put her body inside it (the user's v3.51 QA: stuck in a Water
  Prison wall). **Freed from a wall:** for one second after every warp her body is watched; if it stays inside collision
  for 0.15 s, she is moved to the nearest proven floor cell within 480 u. A warp that starts while she is stuck inside a
  wall and finds no other landing moves her to that cell too. No new row or ini key. **Limits (v3.52.0):** in a room
  without a solid map (tree2), or if another mod changed the collision functions, no landing can be proven and every
  warp there is refused ("the landing cannot be proven"); spots near walls are refused more often (her body needs about
  70 u of clearance); the floor map is built at the first warp of a room visit of any kind (about 10 - 25 ms once);
  being freed is checked only after a warp (up to three seconds while she stays stuck): stuck at another time, warp
  again or use No Clip; the move keeps her facing and picks a cell at least 70 u from every enemy. Ghost Teleport's body
  proof (the room's tree2 collision) and Enemy Pursuit's line test (the mover line 0x454000) are separate tests and both
  stay.
  **A clean fade** (v3.58.0): the see-through fade showed dark lines (her lashes and the edges of her hair as dark
  strips, lines along the seams of her neck, shoulders and waist). The fade used ghost Richard's own draw on her model,
  which gives every texel the fade's transparency (the cut-out parts of her lash and hair textures drew as dark strips)
  and writes no depth (every layer of her model blended: the overlaps where her body parts meet and the hair over the
  scalp showed twice). Now her body and weapon are drawn after the room's other characters, on a depth pre-pass, with
  one blend of her nearest surface and the texture's own transparency (the game's pixel shader 5): no strips, no seams,
  and an enemy behind her stays visible through her. Her real-time shadow leaves half way through the fade-out and comes
  back half way through the fade-in (the rule of Reveal Method's fade). **Under Thermal** her heat body fades with her
  (a see-through heat body at her opacity, one layer; it was solid at full heat over a half-gone body); **under Spirit
  Vision** her soul fire fades out with her and comes back at the landing (it stood in the air where she vanished);
  Night Vision and the other filters work on the finished picture and need nothing more. One exe call is hooked for it
  (0x4EF905, the call to the actor models' draw, byte-checked). Hidden `[Gameplay] GhostTeleportFadeDraw`: 2 Clean (the
  default), 1 one pass (no strips, the layers stay), 0 v3.57's look. **Limits (v3.58.0):** while she fades, particles
  and effects drawn behind her (fog sprites, sparks, blood mist) are hidden inside her outline instead of showing
  through her, as behind her solid body (enemies and the room are not: they are drawn before her); her shadow cannot
  fade by degrees (one stencil mask holds every shadow), so it goes at half; under Spirit Vision the embers her soul
  fire had already thrown fly on for a moment after she vanishes; `GhostTeleportFadeDraw` 2 needs the start (with 0 or 1
  at the start, a 2 set later draws one pass until the game is started again).
- **Randomizer** (v3.48.0, first part; Options > Option 5 Gameplay > Randomizer, a folder under Special;
  `[Randomizer]`): randomizes the **item and weapon pickups** lying in the worlds, from one seed kept in SH4Refined.ini.
  Every change applies **at the next room load** (the next door). Nothing goes into your saves: a save made with the
  Randomizer loads fine without it, and a pickup you took stays taken. **Items**: Off (the game's own pickups),
  **Shuffle** (the items of each world visit trade spots: the same number of each item and weapon as the game gives) or
  **Random** (each spot draws an item as Supply Drops does: common ammo and Nutrition Drinks, less often Medical Kits
  and Holy Candles, now and then Saint Medallions, Ampoules, Silver Bullets and Swords of Obedience; about one spot in
  25 a weapon, and a spot that held a weapon a weapon every other time); default Random. **Specials** (v3.56.0; was Keep
  Specials, turned round): **Off** = Holy Candles, Saint Medallions and Swords of Obedience stay where the game puts
  them (their spots and items leave the pool), **On** (default) = they are drawn with the rest. The specials are exactly
  these three; weapons belong to Items (Henry's weapons that lie on their own move with Items whatever Specials says);
  key items, memos and files, and every pickup that runs an event of its own never move, with any setting. **Room 302
  Items** (v3.56.0): Room 302's own pickups, independent of Items: **Off** (the game's pickups), **Shuffle** (Room 302's
  own pickups swap spots) or **Random** (default: each of its spots draws an item). **Reroll** (v3.56.0,
  `Reroll`): when Random draws again. **By Room** (default): a new draw each time you enter a room. **By World**: one
  draw for a whole visit of a world, and a new one when you enter that world again (through the hole, or when the story
  moves you there); Room 302 is a world of its own here. Shuffle always makes one new arrangement per world visit (a
  room keeps its items while you stay in that world). **A load is never a visit:** a Quick Load, the Extra Menu's Load
  Game, the title's Load and Continue after a death never count as entering a room or a world, so loading a save made in
  a room shows the same untaken items again, and a sneaky Quick Save / Quick Load cannot reroll anything; a save of
  another world, loaded this session after you were in that world, takes back that world's last visit; a world not
  visited since the game started gets "the seed's own" draw, the same for every load (the world-visit rule is the one
  Enemies Return uses, `src/worldvisit.inc`: a load is the game's own signal, its enemy manager reset by every save's
  apply). **The seed**: the number every choice comes from, shown in small grey type under the Randomizer page's
  description (only while a Randomizer row is on). It has no row of its own any more (v3.56.0: no Seed / New Seed rows).
  Set another one in `[Randomizer] Seed` to replay a run; 0 or missing rolls a new one at the next start. **A pickup
  looks like what it gives:** the model on the ground, the "Take the ...?" prompt and what lands in the inventory all
  follow the new item (the model comes from the item, the prompt and the gift from the item's own pickup script; a
  weapon spot gets the matching pickup record for its new item). **Weapons:** Henry's weapons that lie on their own (27
  spots, among them the weapon row of the Subway's first room) join the pool, and any spot may now hold a weapon. Random
  never draws a weapon you already have (the game would not show it) and never the same weapon twice in a room. The
  Pistol and the Steel Pipe of Room 302, the Torch, the Chainsaw of a New Game+ in the Forest and every other weapon
  pickup that runs an event of its own stay where they are; the Steel Pipe, the Spade, the Paper-Cutting Knife and the
  Stun Gun never move (their models load only in their own rooms); no story event needs a weapon. **Never randomized:**
  key items, memos and files, Eileen's weapons, broken golf clubs, and every pickup that runs an event of its own (22
  scripted consumables among them). The pool: the 137 plain pickups (pistol bullets 46, Nutrition Drink 40, Holy Candle
  16, Medical Kit 12, Saint Medallion 8, revolver bullets 5, Ampoule 4, Sword of Obedience 4, silver bullets 2; 18 of
  them in Room 302) and the 27 weapon spots. No exe code patched: the room script's item and pickup-record words are
  patched and restored around the room's build (the room-load hook 0x4F0AF8 that Enemy Copies uses). v3.49.0 adds
  monsters, Eileen's outfit and ghost spots (the next bullets; v3.56.0: Enemies and Spawn Site). **Limits:** changes
  apply at the next door, never in the room you stand in; with Reroll By Room, leaving a room and coming back
  rerolls its Random items (a load does not); the visit counts live in memory: after a restart a loaded save shows its
  world's "seed's own" draw until you next enter a world; weapons can come **early**: the Chainsaw (or another strong
  weapon) can lie in one of the first worlds' spots, which makes the game easier (there is no separate weapons switch
  yet: Specials does not hold weapons back, only Items Off does); Random gives no guarantee of any item (a world may get
  fewer Swords of Obedience than ghosts to pin, the log says so for each world visit, or more healing than the game
  gives; Shuffle keeps each world visit's own counts); Random does not ask which guns you have for ammo (Revolver or
  Silver Bullets can lie in a world before you own the gun); Shuffle can move a weapon you already own onto another
  spot, which then shows nothing (as the game does with a weapon you have); a model can look odd where the game placed a
  small item (a Chainsaw where a box of bullets lay in a drawer); if another mod changes the room-load site 0x4F0AF8 the
  rows say "Not available" and nothing is randomized, and a script word that is not the stock value (another mod) is
  left alone.
- **Randomizer: Enemies** (v3.49.0 as Monsters; v3.50.0 All Worlds and saves; v3.56.0 renamed Enemies, with the victims'
  ghosts; Options > Gameplay > Randomizer > Enemies, `[Randomizer] Enemies`): **Off**, **Same World** (default) or **All
  Worlds**. At every room load of the monster worlds each monster of the room's script rolls its own type (a mix per
  monster, any strength), from the same seed as the items. **Same World**: among the monster types **and the victims'
  ghosts** this world places (both of its visits) or this room has loaded; **All Worlds**: among every monster type and
  every victim's ghost of any world. The files of a type the room does not hold are loaded at the room load (at most 16
  extra files a room, 30 MB of file memory kept free; a type whose files do not fit stays as placed). **The victims'
  ghosts** (v3.56.0): a monster may become one of the game's six victim ghosts (the four male victims, type 0B, and the
  two victims 0C v1 and 0D v1). It is then the game's own victim ghost: it floats, passes walls, the Sword of Obedience
  pins it, it is never killed (no kill, no ending's kill rank), and it may follow you through the rooms of its world the
  way the game's victims do (never into another world). Two of them are kept out until the story allows them: a male
  victim's ghost never appears in the **Water Prison** before you have its first secret-number memo (the game's own code
  marks that memo as found when such a ghost appears there: story flag 2D3), and the **old man's ghost** (0D v1) never
  appears before you have met him in the **Forest** (its appearance sets the Forest's flag 23B). Which ghosts each world
  places itself: the Subway three (0B two looks, 0C v1), the Forest four (0B three looks, 0D v1), the Building three (0B
  two looks, 0D v1), the Apartment three (0B, 0C v1, 0D v1), stage 8 one (0B); the Water Prison and the Hospital none
  (with Same World their monsters roll among monsters only). **Never** a target: the story ghosts (Cynthia, Jasper,
  Andrew, Richard: their creates and routines run story events), the Patient, Walter, Wall Men, Wheelchairs, mm (its
  files exist only for its own rooms). **The victims themselves keep their own ghosts** (each victim exists once per
  world and is placed in several rooms); Spawn Site moves them. Everything else of Monsters stays as it was: games
  played from saves roll too (the game's own records adopted), the Subway's first Sniffer Dogs roll once their cut-scene
  is over, a roll of the monster's own type rolls once more (hidden `MonsterRollAgain`), the bone / enemy-list /
  hit-shape budgets, a walker (or a ghost) at a Hummer's spot on the floor under it, nothing randomized written into a
  save (a monster made a ghost is left out of the save as every randomized monster), off while the cheat table's Enemy
  Randomizer is on. **Reroll** applies: By Room (default) = the room's monsters roll again at every room entry;
  By World = a monster's record keeps its type for the world visit (as v3.55's Change Each Visit Off); a load never
  rolls them again. **This Room** is gone (v3.56.0): an ini value This Room or This World reads as Same World.
  **Limits:** a ghost outside its own world may be silent (the game's sound tables are per world; logged once per type
  and world); a room whose monsters became ghosts cannot be cleared by killing; Enemy Copies makes no copies of a
  monster made a ghost, and Enemies Follow / Enemy Pursuit leave it to the game's own ghost rules.
- **Randomizer: the dead stay dead; Extra Enemies' copies randomized** (v3.58.0; the user's v3.56 QA item 12).
  - **The dead stay dead.** A reroll redraws *what* stands where, never *whether* the dead are back: Enemies Return alone
    decides that (Off: the game's own rule; By Room: back at each room entry; By World: back when you enter the world
    again; a load is never an entry). A monster lying dead in its room keeps the type it died as until Enemies Return
    brings it back, then it is drawn anew. One killed for good by the game (its kill bit) stays as placed and is never
    made. A body whose type cannot be made in the room any more (its files, Enemies Off) stays dead, its placement empty. A
    save keeps a randomized body dead too: after a load it is not shown and does not stand (the save keeps it as the
    game's own kill bit). Items work as before: a picked-up item stays picked up, whatever the reroll draws for its spot.
  - **Extra Enemies' copies are randomized too.** With Enemies Same World or All Worlds every copy of a randomized room's
    monster draws its own type by the same rules and seed (Same World: the world's types; All Worlds: any; never a Tremer,
    which Enemy Copies does not copy; the victims' ghosts by the story rule; Gum Heads only with Playable Eileen's guard).
    Their files are loaded with the room; a type whose files do not fit leaves that copy its original's type. With Spawn
    Site On each copy stands at a site of its own. Copies of an enemy the Randomizer leaves alone (bosses, story and event
    enemies) are copies of their own type. A killed copy stays dead by the same Enemies Return rule, whatever it was drawn
    as. Copies still come at the next room load (the Options lines no longer say "Next room.").
  - **Copies never crowd out real enemies' records.** The game keeps 64 enemy records for the monsters left in rooms you
    left. A room's copies are made only while its exit can still write every real enemy's record without taking a record
    of this world (the game's own first choice, an old world's left-over record, may go): else that room gets fewer copies
    (the log says so). Hidden `[Gameplay] CopyRecordGuard` (1; 2 = free slots only, 0 = v3.57).
  - **Stages with no pickup models** (v3.58.0, MN358): a stage that loads no pickup model files (item_model.bin /
    wp_model.bin are not both in the world's file list: the story interludes, stages 10 and 11 in the v3.57 log) keeps its
    pickups as they are by design: v3.58.0 logs a plain line once per stage a session (v3.48 - v3.57 wrote an ANOMALY
    line); `tools\logcheck.py` reports it under the randomizer line as "left alone by design" and does not count either
    form as an anomaly.
- **Randomizer: Eileen Outfit** (v3.49.0; v3.50.0 every outfit, a draw per room, ghost spots you can see; Options >
  Option 5 Gameplay > Randomizer). **Eileen Outfit**: Off or **Random** (default). Random dresses **companion Eileen**
  in an outfit drawn at random among **every** outfit the game has for her: her Dress and the **Nurse** outfit (v3.50.0:
  no unlock needed any more; there is no third companion outfit). With **Reroll By Room** (the default) she gets
  a new draw at **every room load** of her world (the first room of a world visit keeps the draw made as the world
  loaded; never more than two rooms in a row in the same outfit, so you see her change); with Reroll By World one
  draw per world visit, the same for this seed. The switch happens as the room is built (the game rebuilds her model in
  every room): she is never left invisible, and in a room whose files hold only one of her models she wears that one.
  **The final fight** keeps the outfit of its world visit (no draws in the room-by-room walk to Walter). Her cutscene,
  peephole and door-viewer models follow as they follow the Extra Menu's choice (`[EileenOutfit] Cutscenes`). A choice
  in the pause menu's Extra Menu row Eileen holds until the next world visit (no room draws meanwhile). Off (or a world
  without her) puts back the outfit you set (`[EileenOutfit] Outfit`, or the Extra Menu); the Randomizer never writes
  that key. Needs the outfit engine (`[EileenOutfit] Enabled 1`, the default). **With Playable Eileen:** the player's
  own outfit belongs to Playable Eileen; this row changes the companion only. The v3.49 - v3.55 **Ghosts** row is
  **Spawn Site** since v3.56.0 (the next bullet). No exe code patched. **Limits:** only two companion outfits exist
  (Dress, Nurse): a room draw is a coin with a cap of two rooms in a row alike; in the rooms whose own file list brings
  her model (6:43 in the Apartment World; 7:10 and 2:55 have it from the world list too) she wears the model that room
  loads, and Random puts the set outfit back there (as v3.49); a save made while Random dresses her as the Nurse records
  "Nurse worn" as the Extra Menu's Nurse does; Random keeps both outfit models in memory in her worlds (about 0.7 MB).
- **Randomizer: Spawn Site** (v3.49.0 as Ghosts Spots; v3.56.0 Spawn Site for every enemy; Options > Gameplay >
  Randomizer > Spawn Site, `[Randomizer] SpawnSite`): **Off** or **On** (default). On: every monster and victim ghost a
  room places starts at a random site of its room, drawn again as the Randomizer's Reroll rule says (a load never
  draws new sites). **Monsters** stand on floor you can walk to: a map of the room's floor is flooded at the room load
  from its doors and from where you come in, by the player's own walking rules (Ghost Teleport's floor map); a site is
  never inside a wall or a prop, has room to stand, and is at least 800 units from where you come in and 400 from every
  door (monsters do not wait at the door); sites are 250 apart. A Hummer flies at its own height above its site, a
  monster Enemies made a ghost takes a ghost's site. **Ghosts** (the victims, 0B / 0C v1 / 0D v1) start anywhere inside
  the room's bounds (its collision box) in x, y and z: on any storey of the room, at their own height above the floor
  there, never inside geometry, at least 600 units from where you come in (never inside you) and from another ghost's
  place. A monster or victim the game remembers in this room (its record, from your last visit or your save) starts at
  its site too; one you knocked down or pinned stays where it lies. With **Enemies**, a monster's new type is rolled
  first and the site follows that type. **Kept as placed:** Wall Men, bosses and boss rooms, Wheelchairs, the story
  ghosts, event monsters before their event (the Subway's first dogs before their cut-scene), a Tremer that keeps its
  room box, a monster whose record is in another room (it is not made here), Room 302 and the final stages. Enemy Copies
  stand beside their moved original; a monster that follows you through a door arrives at the door as before. No exe
  code patched; nothing new in saves (a record moved with its site is saved where it stands, as any monster that walked
  there). **Limits:** a room without collision data has no floor map (its monsters keep their spots, logged once); the
  floor map takes at most 40 ms at a room load (a few ms in the rooms measured by Ghost Teleport); a big monster can
  stand in a narrow corridor of the room; a ghost site inside the room's box but outside its walls (an L-shaped room)
  comes through the wall, as the game's victims do. The old `[Randomizer] Ghosts` value is read once (Spots = On) and
  written as SpawnSite.
- **Blood Colour** (v3.25.0): Option 3's Blood Colour has 10 values: Red, Green, Purple (the game's, unchanged), Yellow,
  Orange, Pink, Blue, Cyan, White, Black. They tint the blood spray, the drops' floor splashes and the pools under dead
  enemies (the game's own three blood effects; bloody textures and haunting blood are not part of the option). The
  value is saved in the game's own settings like the stock one: a save made with a new colour shows a wrong colour /
  label in the game without SH4 Refined until Blood Colour is set again there.
- **Screen Effects** (v3.15.0, revised in v3.16.0): in Options > Option 3, Left / Right on Noise Effect still
  switch the game's noise Off / On; Enter fades to a Screen Effects page (Subtitle Style opens the same way, like
  Button Config): Noise Strength (Low, Normal = the game's, High, Very High: the game's own grain and dust), Noise Type
  (Stock, Fine, Coarse, Film, VHS, Scan Roll, Rain: v3.26.0 rain on glass, drops that refract the scene and catch the
  light, some sliding down at random angles in jerks and leaving thin water trails that break into beads), Noise Colour (Stock, Mono, Full Colour, Red, Green, Blue, Amber,
  Sepia: combines with every type) and Filter; the grain and the filter preview live on that page, over your own
  `preview.dds` picture if there is one (the first pause in gameplay saves one automatically; nothing is shipped).
  Filters: None, CRT, Vivid, Sepia, Old Film (animated: flicker, scratches, dust, burns; v3.23.0: the picture no longer
  shakes), Night Vision (green, soft
  corners, lights flare), Night Vision Mono (greyscale, lights flare), Thermal (v3.22.0: a cold world of navy, blue and
  cyan where lamps are never hot, and every living body (you, your companions, enemies, the trapped Cynthia behind the
  Subway train door) glowing red, orange, yellow and white over it, with warm fading footprints; v3.23.0: bodies are
  solid and hidden behind walls, doors and each other as in the normal picture (v3.24.0: also you with the camera close,
  and no cold / hot flicker); dead enemies cool slowly
  (v3.21.0); ghosts and victims give off no heat and are invisible), Spirit Vision (v3.17.0, was Ghost Vision: a black
  world of edge lines; ghosts glow through walls (v3.23.0 white; v3.26.0 blood-rooted: a pale core bleeding into
  crimson, a crimson aura on the body, and blood seeping, dripping and falling to the floor where the veins grow;
  `SpiritGhost=White` gives the white look back); v3.22.0: hauntings reach out with dark, wet veins that crawl onto
  walls and floors and hold on with sucker mouths, pulse as they feed, snap and re-grow, and latch onto you, coiling round
  your limbs, when you come close (the Saint Medallion makes them let go in a shower of gold sparks); v3.23.0: the veins
  grow out of the ghost's own body, are outlined by a dark crimson glow so they read on the black world, let go of anchors a
  wall now hides, up to four latch onto you, and when a ghost is knocked down they pull back into its lying body and grow
  out again over the last seconds before it rises (a ghost pinned by the Sword of Obedience keeps none); wall blood portals
  glow; Holy Candles pulse a wide gold glow slowly with a fast thin ring that travels further; the Saint Medallion glows at
  your chest while you wear it; Eileen's curse fit shows its sickly area; living monsters carry an ember-coloured soul fire
  at the heart that dies down slowly after death, Walter a large red one, you a white-gold one that shrinks as your health
  falls (v3.23.0: current / max HP), people (escort Eileen, also through the Room 302 peephole, Cynthia, the trapped
  Cynthia and other living people) a white-gold one (v3.24.0), ghosts and wheelchairs none) and Vignette. Each filter has a fixed strength. Filters show in gameplay, real-time cutscenes (row Cut-scenes:
  On / Off, v3.22.0) and under interaction text (the text stays on top, unfiltered). v3.22.0: the picture the game freezes
  for the pause menu, the item ring, the map and Options is taken with the filter on, so they show the filtered scene with
  its auras, soul fires and heat (menus stay clean); the item ring's description darkens only its own box. On the Screen
  Effects page the filter covers only the preview picture (the scene you paused in, when opened from a pause), the text
  sits on dark panels with a black outline (v3.22.0; v3.23.0: the outline no longer pushes rows and values off the page).
  v3.28.0, Spirit Vision: the crimson haunting tendrils of ghosts, the possessed companion Eileen and the wheelchairs all
  grow from one round black heart at the source's chest (a beating crimson rim; v3.29.0: an opaque black-hole fill), and
  the soul fires are living flames (flicker, rising embers, smaller as health falls, a flare and wild lashing when hit,
  guttering out like a real fire at death; companions' fires never go out). v3.29.0, Thermal: no more blue flicker on
  bodies in rooms that keep the 3D depth (Building World 2nd).
  Kept in `[ScreenEffects]`.
- **The Effects page's preview shows every filter** (v3.56.0; Options > Effects). From a pause, Thermal now shows the
  paused scene's living bodies hot (dead ones cooling red, the footprints) over its cold world, and Spirit Vision the
  ghosts glowing through the walls, the soul fires, the haunting auras, portals and veins over its edge world: in the
  frame the pause picture is taken, each look's own parts are drawn once and kept on the GPU beside the picture (nothing
  read back; the pause picture itself is unchanged). The pause menu shows them too when the filter was changed in
  Options. With no paused game (the title menu) the page shows SH4 Refined's own small scene instead of the F7 picture,
  which may hold nobody: a dim room with a window, a lamp, a door and a cabinet, a standing figure and a ghost half
  behind the cabinet, all drawn by Refined (no game art). Thermal shows the figure hot with fading footprints (the ghost
  is cold: unseen), Spirit Vision the ghost glowing through the cabinet and the figure's living soul fire; CRT, Vivid,
  Sepia, Old Film, the night visions, Vignette, Film Grain, Bloom (the window and the lamp), Colour Grading, Depth of
  Field and the Otherworld Lens all work on it. The scene is drawn only while the page is open (a few draws).
  `PreviewScene=Picture` keeps the F7 picture first (and the Filter row's " F7 in play: new preview." sentence);
  `PreviewLayers=0` turns the pause layers off. The capture frame's extra draws never count as a late frame for the
  effect budget or the rate guard. Not in the previews: Death Presence, Echo and After Shock (the Powers' own looks); a
  pause taken under Thermal has no ghosts in its Spirit Vision preview (Thermal hid them from that frame).
- **Preview Off / Stock / Live** (v3.58.0; Options > Effects > Preview). What the Effects pages (Effects, Film Grain >,
  Depth of Field >, Shadows >) show behind them:
  - **Off**: no preview at all; the game's own menu background, as on any stock page (also no split preview on the
    Shadows page). The filters still apply in play.
  - **Stock**: always the stock picture, on the title menu and in a pause (a pause shows the picture, not the paused
    scene). The filter is drawn over the picture: Thermal shows its cold heat palette over the whole picture and Spirit
    Vision its edge world (a picture holds no bodies to heat and no ghosts to glow).
  - **Live** (default): the stock picture on the title menu; in a pause the paused scene with Thermal's hot bodies and
    Spirit Vision's ghosts and soul fires (as v3.56.0). Room 302's pause is a pause too.
  - The stock picture is the file `SH4Refined_preview_stock.dds` in the game folder (the installer places the user's F7
    picture of 2026-11-26 there; it is never inside winmm.dll). F7 never overwrites it. Without the file: Live shows the
    F7 picture `preview.dds` on the title menu, else SH4 Refined's own drawn scene (v3.56.0); Stock shows the drawn scene.
    F7 in play still saves `preview.dds`; the Filter row's second line "Press F7 in play for a new preview." shows only
    where F7 changes what the page shows (Live, outside a pause, no stock picture).
  - The Shadows page keeps its rule with every Preview value: with Shadows Off its split preview shows no real-time
    shadow (unless the paused room is one the game keeps them in), and with Preview Off there is no split preview.
- **Filter Strength Low / Medium / High / Ultra** (v3.58.0; Options > Effects > Filter Strength, grey with Filter None;
  v3.15.0's row back with Ultra). High is the filters' look since v3.16.0, unchanged. Low and Medium lay the plain
  picture of the same frame back over the filter (35 % / 65 % of the filter's change); Thermal's hot bodies and Spirit
  Vision's ghosts, auras, portals and soul fires are drawn after that at full strength, so both stay readable at Low.
  Ultra takes each look's own amounts 1.35 times further (saturation, contrast, brightness, the tone, CRT's fringe,
  scanlines and grille, vignettes, grain, Old Film's flicker, Vivid's sharpening, Night Vision's bloom); Thermal and
  Spirit Vision are already the whole picture and draw as High. The Effects page's preview shows it live.
- **Effects > Depth of Field >** (v3.58.0): Depth of Field and its strength moved into a folder (rows Depth of Field,
  Strength), as Film Grain > and Shadows >, with the preview behind it. The Effects page now holds 11 rows: Film Grain >,
  Filter, Filter Strength, Cut-scene Filter, Bloom, Colour Grade, Otherworld Lens, Depth of Field >, Shadows >, Preview,
  Exit.
- **Thermal in the preview** (v3.58.0): the heat layer the pause keeps for the Effects page draws the bodies' outside
  again. v3.56.0 redrew them without writing depth, so a body the game's close-camera fade drew without depth of its own
  (the player, e.g. Playable Eileen in the shoulder camera) showed its far, inner faces. Thermal in play was right (it
  has written that depth since v3.24.0). Ghost Teleport's faded heat body keeps its own depth pre-pass there too.
- **Depth of Field eases in** (v3.47.0; the Screen Effects page's Depth of Field / DoF Strength rows). The blur no
  longer starts with an edge: from the sharp area around the player it fades in over a short distance, a quarter of a
  blur step at a time, and fades the same way into each stronger blur farther away and into the near blur in front of
  the player, at Low, Medium and High. The fades are centred where v3.46's hard steps were, so the overall amount of
  blur is about the same. The Screen Effects page's preview shows the same fade (the paused scene, any strength chosen
  on the page, and the sample picture from the title). **Limits:** an in-between step is a mix of two blur sizes (25 /
  50 / 75 %), not a true intermediate blur, so a faint double image is possible at a strong edge; without the blend
  shader (a device that cannot make it; logged once) the blur steps by whole levels again, each at the middle of its
  fade; the fade length is fixed per strength (no setting).
- **Depth of Field keeps working with a close camera** (v3.52.0). Until v3.51 the depth of field switched off whenever
  the player stood nearer than 150 units to the camera, because that was taken for a first-person view. In the worlds a
  third-person camera often comes that close (the Subway World's narrow rooms, a companion filling the frame), so the
  blur vanished in those shots. Now only the game's real first-person views skip it: Room 302's own camera (the game
  runs its first-person camera only there) and the views of another place (peepholes, the wall hole, the door scope,
  windows). With a close camera the focus stays on the player, the background blurs as usual, and nothing between the
  player and the camera is blurred; the pause preview of such a shot shows the same. **Limits:** the focus is the
  player's position, at least 200 units away; a companion or object right behind the player stays sharp at Low and
  Medium and may get a light blur at High; a cut-scene's own focus is used only when it is farther than 150 units.
- **Bloom: Off / Low / Medium / High / Ultra** (v3.52.0; the Screen Effects page). The levels are recalibrated so that
  each step is clearly stronger than the one below (about 1.5 times the glow each). Low now glows a little more than the
  old High did, Medium as much as the old Very High, High about 1.6 times the old Very High, and Ultra is a strong haze
  around every light. A value saved by v3.43 - v3.51 is read once with its old meaning and rewritten: Low stays Low,
  High becomes Medium, Very High becomes High (at least as strong as before). The Screen Effects preview shows every
  level. **Limits:** Ultra brightens large bright areas (a window, a lit doorway) to almost white (High is the strongest
  level that keeps them readable); the Bloom description stays within 68 characters (the game's text buffer).
- **Otherworld Lens: when it shows** (v3.52.0). The lens fades in while the game registers a haunting in the room: a
  ghost (Cynthia, the victims' ghosts, Jasper, Andrew DeSalvo, the Wheelchair), the companion Eileen's curse once it
  shows, or one of Room 302's hauntings. Its strength there is now 70 % (was 40 %, hard to see in SH4's dark corners;
  hidden `[ScreenEffects] LensBase`) and rises to full when the player is halfway into a ghost's aura. The preview on
  the Screen Effects page shows full strength, as before. The row's description says when it shows; a per-room log line
  says how long and how strong it showed (written when the player leaves a room, so the last room has none).
- **Shadow Edge and Shadow Darkness cover every shadow the game draws** (v3.49.0; Option 3 > Realtime Shadow, Enter: the
  Shadows page, `[Shadows]`). Silent Hill 4 has one kind of real-time shadow: stencil shadow volumes, cast by every
  model that has a shadow file, all joined into one screen mask and laid over the picture by one composite. That mask is
  what Soft blurs and what Darkness lightens, so both cover every real-time shadow: the player and the weapon in hand
  (the Sword of Obedience, held or pinned, too), Eileen and Cynthia, every enemy, ghosts, Room 302's hauntings, the
  final boss room's spears, the Water Prison's turning prison, and the figures of events and in-engine cut-scenes (the
  user's v3.48 log: 78,982 composites over 12 room lines, every one soft). New in v3.49: the **Tremers' (the leeches')
  wall shadows** are a ribbon the game draws outside the mask; **Shadow Darkness now applies to them too** (`[Shadows]
  TremerShadows=1`, hook 0x45F4CA). **Limits:** Soft cannot blur the Tremers' wall shadows (their edge is their
  texture's); the shadows of furniture, doors, props and items, in Room 302 and in the worlds, are painted into the room
  textures and stay as painted (texture work: SH4 HD Textures); items and doors have no shadow file, so they cast no
  real-time shadow; with Option 3's Realtime Shadow Off the game casts real-time shadows only in its short room
  whitelist; the type-1 enemy's "shadow on the surface" sprite is drawn with the normal alpha blend in a lit colour (not
  a darkening shadow) and is left alone. Movies are pictures. v3.52.0: **Shadow Darkness runs from light to dark**: 25 /
  40 / 55 / 70 / 85 / 100 %, Left lighter and Right darker (from 100 %, Right wraps round to 25 %, like every Options
  row); the ini keeps the percentage, so an old `[Shadows] Darkness` value reads as before, and the page's split preview
  follows the row. **Room 4:1F (stage 4 room 0x1F, Water Prison, room 31) has no real-time shadow at all**, with or
  without Refined: the game's own composite (0x55F660) sets the darkness there to 0, and cut-scene 0x43 does the same,
  so the mask is empty and the v3.51 log's "0 soft" in that room was nothing drawn, not a hard edge. Everywhere else
  every real-time shadow is soft. The log now gives a reason for every composite that is not soft (the room line ends
  with "not soft: H hard (reasons), Z with no darkness"; the first of each reason in a room is logged once), and
  `tools\logcheck.py` lists the rooms with a hard share. Room 4:1F and cut-scene 0x43 stay without real-time shadows, as
  in the game.
- **Shadows Off is the game's Off** (v3.58.0; Effects > Shadows > Shadows). **Off** is the game's own Off, at once (the
  game reads the switch every frame): no real-time shadows, except in the few rooms the game keeps them in: Room 302's
  living room (outside cut-scenes), the Water Prison (24 of its rooms) and Hospital World rooms 1 and 1F. The Tremers'
  wall shadows stay too (they are the enemy's own look; the game never hides them). Shadows baked into the room textures
  are pictures and stay. Refined follows it: the soft edge and the Darkness only touch shadows the game draws; the
  Shadows page's preview shows the game's Off (the paused scene without its shadow, no split line; from the title no
  figure's shadow); Echo Doubles' ground discs go with Off as well. The log says Shadows On / Off at the start and at each
  change. (This corrects the v3.49 note above: the game's "short room whitelist" is most of the Water Prison, Room 302's
  living room and two Hospital rooms.)
- **Vibration strength** (v3.14.0): Options > Option 2's Vibration row has Off / Low / Medium / High. Low is the
  game's own strength (GOG's controller wrapper drives the motors at about 13 % of their range), Medium about 50 %,
  High 100 %. Off and On are still the game's own byte, so saves are unchanged; the level is `[Input] Vibration`.
- **Omni-Directional Movement** (v3.46.0 as "Stick Movement"; v3.47.0: Options > Option 2 (Control), above Exit;
  v3.48.0: renamed, Off / On; `[Input] OmniMove`): the left stick walks at any angle instead of the game's 8 directions.
  **Off** (the default) is the game's own movement, byte for byte (nothing is written); **On**: any angle, and a lighter
  push walks slower (0.6 to 1 of the full walk: 0.6 is the walk clip's own floor, so the feet never slide; a full push
  is the full walk, the same speed every way: the stock diagonal was 8 % faster). On is v3.47.0's Analog; v3.46.0 /
  v3.47.0's Direction (full speed at any angle) is gone, and an ini that still says Direction, Analog or 2 reads as On
  (the first start writes it back once as On and logs it). With Playable Eileen v1.24.0 or later her walk / run follow
  the push too (Refined's export `SH4R_StickAmount`, see "For other tools"; she answers through her export
  `SH4PE_StickAmountOn`, and with an older version, or her `[Movement] StickAnalog=0`, the row's line says "Eileen keeps
  her own speed"). Hidden `[Input] OmniDeadZone` (0.30, round; the game's own +-300 of +-1000). It applies at once and
  only in gameplay: the title, Options, pauses, overlays (map, items, memos, save screens, code pads) and cut-scenes keep
  the game's input, so every menu navigates as before. Weapon raised, the steps go at a free angle through the game's
  own clip choice (no 45-degree snap). The engine was analog all along: one hook after the game builds its stick floats
  (0x5543C0, the call of 0x553D20) writes the real stick vector into `gPad.stick[0].x / y`; the raw axes come from a
  second GetDeviceState on the game's pad or from the poll's own AxisEvent calls (0x407B15 / 0x407B28), whichever a
  self-check against the game's direction bits confirms in the first second of stick use (the game's floats stay until
  then). Room 302 (first person): its readers of the stick (the walk / turn builder 0x52EEE0, 0x52E0B0) get the same
  vector; with Playable Eileen's `[Room302] PadAnalog=1` (its default) its own analog walk (0.24 dead band, from its raw
  copy) still sets forward / sideways in the move, and Refined's vector keeps the turn (when X turns). **Limits:** the
  keyboard stays 8-way (keys set the same direction bits; the stick wins while it is pushed); movement must be on the
  left stick (or the sticks swapped, then GetDeviceState only); D-pad / button movement leaves it off (logged, and the
  row says so); the weapon-raised steps need a push of about 0.83 (the game's forward > 0.9 gate), and without Playable
  Eileen no ready-stance step passes that gate at 60 fps or more (an older issue; PE scales the gate); the first ~0.7 s
  of stick use in a session stays 8-way (the self-check); with `[Window] AspectMenu=0` (no Refined Options pages) there
  is no row: `[Input] OmniMove` still applies, also edited while the game runs; the label "Omni-Directional Movement"
  is 286 canvas px wide (font 0), ending at x 326 of the value arrow's 344 (the stock Spanish "Movimiento Primera
  persona" on this page is 288); Refined's texts are English in every language, and in French (language 2) the game
  draws Option 2's rows from the 6th on one line lower, so its 8th row (Exit) comes close to the description line; the
  row's Eileen line reflects Playable Eileen's answer as Options opened (and at the stick's self-check), not a Playable
  Eileen ini edit made while Options is open. A GetDeviceState failure while the game window is not in front (a focus
  loss) is a verbose log line, not an anomaly (v3.47.0). v3.48.0: with Direction gone, a v3.46 /
  v3.47 player who chose Direction now walks slower at a light push (a full push walks as Direction did; there is no
  full-speed-at-any-angle choice any more, the user's request); an `OmniMove=Analog` / `Direction` typed into the ini
  while the game runs reads as On at once, and the file keeps the old word until the next start writes On (no write
  while the file may be open in an editor).
- **Analog look** (v3.49.0, hidden `[Input] OmniLook`, default `Room302`): while Omni-Directional Movement is On, the
  right stick turns Room 302's first-person view with its real push and angle: a light push turns slowly, a full
  push as fast as before, and a quick flick from one side to the other keeps turning. The game itself reads the right
  stick as an on / off switch (nothing below 30 % of a push, full speed above it, a push near 30 % stutters), and
  after a direct flick across the middle its stick reading stayed centred until the stick passed the middle again (the
  game's "reversal latch"; Playable Eileen v1.24.2 fixes the latch only while she is On, so vanilla Henry had it: the
  user's "worse for Henry"). `All` makes the look analog in every stage (the third-person and over-the-shoulder look
  too); `Off` keeps the game's own 8-way look. A look key held (I / J / K / L) keeps the game's look; the mouse still
  wins in the frames it moves. Also fixed: in Room 302 with First Person Movement set to turn on X, walking forward
  with the left stick no longer silences the right stick's turn. No new exe patch: the same hook after the game's
  stick floats (0x5543C0) writes `gPad.stick[1]` (0x1236C54 / 0x1236C58) from the same GetDeviceState read, after
  its own self-check against the game's look bits. **Limits:** only while Omni-Directional Movement is On with
  GetDeviceState as its confirmed source (else, at a GetDeviceState failure or while the self-check is pending, the
  game's own 8-way look stays; the first ~40 frames of look use in a session are the game's own look); the analog
  dead zone is fixed at 0.20 of a full push; `All`: the third-person builder's flick test (0x52F260: |right x| > 0.8)
  needs a fuller push than the game's on / off stick; Room 302's own pitch limits and the fixed angle a tick stay.
- **Subtitle Style** (v3.13.0): in Options > Option 3, Left / Right on Subtitles still switch them Off / On; Enter
  opens a Subtitle Style page: Size (Small, Normal, Large, Huge; long lines are fitted to the screen), Colour (White,
  Yellow, Cyan, Green, Grey), Edge (Shadow, None, Outline, Outline+Shadow) and Box (None, Translucent, Solid), with a
  preview line at the real subtitle position; Back returns. They apply at once to cutscene, in-game and movie
  subtitles; the defaults are the game's own look. Kept in `[Subtitles]`.
- **GOG display fit** (`[Window] FitWrapper`, v1.3): GOG's DirectX wrapper draws the picture at the game's own size
  (as `dxcfg.ini display=application`, `presentation=windowed`), so every size is sharp and undistorted, and the
  mouse is scaled to the window. dxcfg.ini itself is not changed.
- **Controller crash fix** (`[Input] ControllerCrashFix`): GOG's input wrapper `dinput8.dll` could crash when a
  controller (a DualSense in particular) connects; its HID reads are kept off thread stacks.
- **The level music stays paused in the Extra Menu's Storage Chest, Save Game and Load Game** (v3.51.0; pause menu >
  Extra Menu). The pause menu silences the level music and the world's sounds; opening one of these three from the Extra
  Menu used to bring the music back for a moment at full volume and then keep it playing quietly for as long as the list
  or the chest was open. Now the pause's silence goes on, as when the game's own pause menu opens its map or memo list.
  The menu sounds (cursor, select, the notebook) play as before. Back in the pause menu (Cancel, or the chest / save
  menu closed): still silent. Resume: the music comes back once, as after any pause. Picking a save in Load Game: the
  screen fades to black in silence; the music of the room you left fades back in for the moment before the loaded room
  starts (as in v3.50, and as a Quick Load); the loaded room then starts its own music as the game does. Hidden
  `[ExtraMenu] PauseSound=0` = v3.50. No exe patch. **Limits:** sounds that the game's own pause menu leaves playing
  also play on in the three menus; after you pick a save, the old room's music comes back behind the black screen for
  about half a second, until the room change takes over (as in v3.50 and as a Quick Load); until v3.51 the Extra Menu's
  Quick Load closed the pause first (as since v3.7): the music and the world resumed for a moment before the quick save
  loaded (v3.52: below).
- **Quick Load from the pause** (v3.52.0; pause menu > Extra Menu > Quick Load). It now loads from inside the pause: the
  world stays frozen and the level music stays paused while the screen fades to black (16 frames, about a quarter of a
  second at 60 fps), then the quick save loads. The loaded room starts with its own music, faded in once its first frame
  has run (through the game's own 0x56E960(-1), so the Sound page's Night Mode and Mono apply; a 30 s watchdog gives the
  sound back if no loaded room is seen running). Before v3.52 the pause closed first, so the game ran and the music
  played for a moment before the load. The Quick Load key (F9) works in play as before; while the pause is up the quick
  keys are refused, as before (use the Extra Menu's row there). If the pause is not settled yet, a message is up, or
  `[ExtraMenu] QuickLoadInPause=0` / `Seamless=0`, the row uses the old way. The log names the path of every quick load.
  No exe patch. **Limits (v3.52.0):** behind the black screen the old room takes one game tick into its door leave and
  runs the leave's 16 frames, as at every door (nothing from it is kept; Henry gets no input there); the loaded room's
  music fades in the way the game brings it back after any menu (from 20 % to full in about 0.4 s); if a cut-scene
  starts on the loaded room's very first frame, its own end brings the music back; with `PauseSound=0` the music also
  plays quietly during the fade; the Extra Menu's Load Game is unchanged (the old room's music can still rise behind its
  black screen for a moment).
- **Save Banks: the Bank row** (v3.47.0; Save Banks since v3.8.0, `[SaveBanks]`): above the save and load lists, "Bank
  N" is centred over the list and drawn in the Menu Font (Options > Option 1 > Menu Font; the stock serif with Stock),
  like the list's rows. The "Q/E L1/R1:" hint is gone (the keys still work). Point the mouse at the row (or press Up on
  the first slot) and the blue bar with the arrows appears; the left arrow goes to the previous bank, the right arrow
  to the next (clicking "Bank N" itself also goes to the next bank, as before). An arrow is drawn only where a bank
  is; clicking where a missing arrow would be buzzes, like Q at bank 1. A click on the Bank row never loads or saves
  the highlighted slot: if the list had the cursor, the click selects the Bank row. Keys and pad (v3.47.0): Q / E /
  L1 / R1 switch anywhere in the list, Left / Right on the row, the mouse wheel on the row, Down or Enter back to the
  list; v3.48.0 adds the 10-bank jumps and the held repeats (next bullet). **Limits:** the label's width is known only in game (logged at the first draw; an ANOMALY if wider than the
  128-unit bar); the arrows show only while the row is selected (the mouse on it, or Up from the first slot), as in
  the game's own A Brand-new Fear selector; the ":" and the save count's digits below the list ("Number of Saves :14")
  are the game's own special glyphs (drawn without the Latin escape by the stock list): they keep the old look with
  any Menu Font.
- **Save Banks: the Bank row's controls** (v3.48.0; the Load / Save list). Highlight "Bank N" above the list (Up from
  the first slot, or point at it with the mouse); then:

| | 1 bank back / on | 10 banks back / on |
|---|---|---|
| Keyboard | Left / Right arrow | Q / E |
| Xbox pad | D-pad Left / Right | LB / RB |
| PlayStation pad | D-pad Left / Right | L1 / R1 |
| Nintendo pad | D-pad Left / Right | L / R |
| Mouse | the left / right blue arrow (click "Bank N" = the next bank), the wheel | - |

  Holding a key, a pad button or the left mouse button on a blue arrow repeats, faster the longer you hold (after half
  a second 7.5, then 15, then 30 banks a second). A jump of 10 stops at the first / last bank (from bank 4, Q goes to
  bank 1); the buzzer sounds only when you are already there. Moving the mouse off the arrow or letting go stops the
  repeat. A click or hold on the row never loads or saves a slot. The triggers (LT / RT, L2 / R2, ZL / ZR) do nothing
  on the row. While a slot of the list is highlighted, Q / E and the shoulders still change the bank by 1, and Left /
  Right page the list as in the stock game. The pad buttons are those of GOG's controller layer (the game's own
  `dinput8.dll`), which gives every Xbox, PlayStation and Nintendo pad the same button order (the shoulders are buttons
  4 / 5; no XInput). The log writes one line per settled bank change (`save banks: bank 3 -> 27 in the load list
  (...)`) instead of one per step (v3.47.0: 558 lines in 2:40 of play). **Limits:** a pad that GOG's layer does not
  list keeps its own button numbers (buttons 4 / 5 may not be its shoulders); the left stick also steps the bank on the
  row (it is part of the game's menu directions); a bank change made less than a second before the game is closed is
  not logged (the bank itself is saved at once).
- **Save Banks: the blue list bar** (v3.49.0; the Load / Save list, `[SaveBanks] ListBar`). The highlighted save
  slot's bar is the game's blue menu bar, the one the stock game draws behind a selected game-mode / difficulty row
  (and the Bank row's bar), instead of the game's solid yellow list bar, so the Menu Font text stays readable over the
  red background. Same size, place, pulse and blending as the stock bar; only its colour changes (11 bytes of the
  game's list-bar code at 0x56FDCF, checked against the stock bytes first). `ListBar=Yellow` keeps the game's yellow.
  Also with Save Banks Off. The Bank row was blue already.
- **The mouse pointer stays on the Bank row** (v3.49.0). With "Bank N" highlighted the pointer no longer disappears
  (v3.47 / v3.48: it vanished whenever the row had the cursor, and came back when the mouse left the row). As in the
  rest of the list it still hides after a few seconds without mouse input (the game's own 180-frame rule).
  **Limits:** the bar is drawn with the game's own blending for these bars (as the game's blue mode rows on the same
  red screen), not a solid opaque blue (a brighter or opaque blue would be a later, draw-time step); ListBar is read at
  the start only (no Options row); if another tool has changed the bytes at 0x56FDCF the bar stays yellow (logged).

It is a stand-alone `winmm.dll`: the game loads it from its own folder, and it passes every Windows Multimedia call on
to the real `winmm.dll` in `C:\Windows\SysWOW64`. It needs no loader and no other mod. The game exe stays stock.

Split out of the Playable Eileen plugin (its windowed mode up to v0.81) and merged with the SH4Windowed.asi mod.

## Options pages (v3.55.0)

The Options layout plan (`staging/options_layout/PLAN.md`) is complete: rounds R1 - R4 (v3.53.0, v3.54.0) and R5 + R6
(v3.55.0) put every row on its plan page, the game's own rows too. No ini key, written value or save byte changes.

The page cycle (Q / E, the page arrows, or a click on a name in the strip under the title): **Display, Controls,
Interface, Sound, Effects, Gameplay**. A `>` row is a folder (blue, with a triangle): Enter, or a mouse click anywhere
on the row (its name, the triangle, the value column), opens its page (v3.56.0; Enter only before); Brightness, Gamma
and Buttons open the game's own screens the same way. Back (Enter or a click on it, Cancel, or a page key) goes back to
the row that opened it. Controls > Camera > Steering is the one third level. No page has more than 10 rows (Effects 11 since v3.58.0). Every value
has its own line under the rows ("Value: what it does."; v3.56.0); a value that is a number (sizes, degrees, distances,
percentages) shows the row's line, which names its scale.

A hidden `[Options] StockMoves=0` (not written to the ini) puts the game's own rows back on their v3.54.0 places
(the sound three on Display, the first-person three on Controls, the Subtitles / Film Grain / Shadows switches and
Blood Colour on Interface; Renderer on Effects, Menu Font and Start-Up in Interface > Menus).

### The settings, page by page

Values: the default in bold, the ini word in brackets where the screen word differs. "Was" = where the row was in
v3.51 - v3.53 (the plan's "Today" column, under its old name).

#### Display (top page 1)

| Row | Values | ini / save | Was |
|---|---|---|---|
| Resolution | 640x480, **1024x768**, 1280x960, 1600x1200, 2048x1536, 2880x2160 | cfg+0x20 (stock) | Option 1 > Resolution |
| Aspect Ratio | **4:3**, 16:10, 16:9, 21:9, 32:9 | cfg+0x21 (shape + 1) | Option 4 > Aspect Ratio |
| Display Mode | **Windowed**, Borderless | [Window] Borderless; cfg+0x22 | Option 4 > Display Mode |
| Monitor | **Main**, Second, Third, Fourth, Display 5 | [Window] Monitor / MonitorName / MonitorPath; cfg+0x23 | Option 4 > Monitor |
| Frame Rate | **30**, 60, 120, 180 | [Window] FrameRate | Option 4 > Frame Rate |
| Renderer | **Direct3D 11**, Direct3D 12, Stock, Custom, Unavailable | dgVoodoo.conf (copied from dgVoodoo_variants at Exit Yes) | Option 4 > Renderer |
| Brightness > | the game's screen | cfg+0x10 / +0x15 | Option 1 > Brightness |
| Gamma > | the game's screen | cfg+0x1C | Option 1 > Gamma |
| Exit | | | the game's Exit |

#### Controls (top page 2)

| Row | Values | ini / save | Was |
|---|---|---|---|
| Buttons > | the game's screen |  | Option 2 > Button Config |
| Default Speed | **Walk** (ini `0`), Run (ini `1`) | cfg+0x18 | Option 2 > Default Control |
| Vibration | Off, **Low**, Medium, High | cfg+0x0B + [Input] Vibration | Option 2 > Vibration |
| Analog Movement | **Off**, On | [Input] OmniMove | Option 2 > Omni-Directional Movement |
| Camera > | opens **Controls > Camera** | | Option 6 > Camera + pause Extra Menu > Camera |
| Room 302 > | opens **Controls > Room 302** | | Option 4 > Room 302 + Option 2 first-person rows |
| Exit | | | the game's Exit |

#### Controls > Camera

| Row | Values | ini / save | Was |
|---|---|---|---|
| Field of View | **Default**, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120 | [Window] FOV (0 = Default) | Option 4 > 3rd Person FOV |
| Camera | **Game** (ini `0`), Shoulder (ini `1`) | [Camera] Enable | Option 6 > Camera |
| Distance | **800** | [Camera] Distance (0-1000 step 25) | pause > Extra Menu > Camera > Distance |
| Side | Left 150, Left 25, **Centre**, Right 25, Right 150 | [Camera] Side (steps of 25) | pause > Camera > Side |
| Height | **330** | [Camera] Height (150-400 step 10) | pause > Camera > Height |
| Tilt Down | **17** | [Camera] TiltDown (0-40) | pause > Camera > Tilt down |
| Fixed Angles | **Off**, On | [Camera] FixedCameras | pause > Camera > Fixed cameras |
| Steering > | opens **Controls > Camera > Steering** | | pause > Camera (rows 5-10) |
| Back | | | "Back to Controls." |

#### Controls > Camera > Steering

| Row | Values | ini / save | Was |
|---|---|---|---|
| Turn Speed | **0.1** | [Camera] TurnSpeed (0.02-0.3) | pause > Camera > Turn speed |
| Tilt Speed | **0.1** | [Camera] TiltSpeed (0.02-0.3) | pause > Camera > Tilt speed |
| Invert Turn | **Off**, On | [Camera] InvertX | pause > Camera > Horizontal invert |
| Invert Tilt | **Off**, On | [Camera] InvertY | pause > Camera > Vertical invert |
| Wall Zoom | Off, **On** | [Camera] WallZoom | pause > Camera > Wall zoom |
| Wall Margin | **50** | [Camera] WallMargin (0-200 step 10) | pause > Camera > Wall margin |
| Back | | | "Back to Camera." |

#### Controls > Room 302

| Row | Values | ini / save | Was |
|---|---|---|---|
| Field of View | **Default**, 100, 105, 110, 115, 120 | [Window] FOVFirstPerson | Option 4 > 1st Person FOV |
| Look Down | Game, 60, 65, 70, 75, 80, **85** | [Room302] LookDownLimit | Option 4 > Room 302 > Look Down |
| Look Up | Game, 35, 45, 60, 70, 80, **85** | [Room302] LookUpLimit | Option 4 > Room 302 > Look Up |
| Look Smoothing | Off, 0.5, **1**, 1.5, 2 | [Room302] LookFeel | Option 4 > Room 302 > Look Feel |
| Invert Look | **Off** (ini `Normal`), On (ini `Reverse`) | cfg+0x16 | Option 2 > First Person Camera |
| Head Bob | Off, Slight, **Full** (ini `On`) | cfg+0x08 | Option 2 > Head Motion |
| Turn or Strafe | **Turn** (ini `Rotate`), Strafe (ini `Sideways`) | cfg+0x17 | Option 2 > First Person Movement |
| Walk Speed | 0.5, **0.65**, Game, 1.2 | [Room302] WalkSpeed | Option 4 > Room 302 > Walk Speed |
| Back | | | "Back to Controls." |

#### Interface (top page 3)

| Row | Values | ini / save | Was |
|---|---|---|---|
| Language | **English** | cfg+0x11 | Option 3 > Language |
| Subtitles > | opens **Interface > Subtitles** | | Option 3 > Subtitles (+ Enter: Subtitle Style) |
| Health Gauge | Always (ini `Off`), **Auto Hide** (ini `On`) | cfg+0x09 | Option 3 > Hide Gauge |
| Item Icons | Always (ini `Off`), **Auto Hide** (ini `On`) | cfg+0x0A | Option 3 > Hide Icon |
| Enemy Health | **Off**, Target (ini `Locked Target`), All Near | [Gameplay] EnemyHealthBars | Option 5 > Enemies > Enemy Health Bars |
| Curse Meter | **Off**, On | [Gameplay] CurseMeter (On = Companion) | Option 5 > Curse Meter |
| Menu Font | **Stock**, EB Garamond, Cormorant (ini `CormorantGaramond`), Crimson (ini `CrimsonText`), Custom | [Textures] MenuFont | Option 1 > Menu Font |
| Start-Up | Full, **Skip Logos**, Fast | [Window] Opening | Option 4 > Opening |
| Pause Menu > | opens **Interface > Pause Menu** | | Option 6 (Extra Menu) rows 0-6 |
| Exit | | | the game's Exit |

#### Interface > Subtitles

| Row | Values | ini / save | Was |
|---|---|---|---|
| Subtitles | Off (ini `0`), **On** (ini `1`) | cfg+0x12 | Option 3 > Subtitles (stock On / Off) |
| Size | Small, **Normal**, Large, Huge | [Subtitles] Size | Subtitle Style > Size |
| Colour | **White**, Yellow, Cyan, Green, Red, Pink, Orange, Amber, Ice Blue | [Subtitles] Colour | Subtitle Style > Colour |
| Edge | **Shadow**, None, Outline, Both (ini `Outline+Shadow`) | [Subtitles] Edge | Subtitle Style > Edge |
| Box | **None**, Translucent, Solid | [Subtitles] Box | Subtitle Style > Box |
| Appear | **None**, Typewriter, Fade In, Wave, Glitch, Blood Drip, Embers | [Subtitles] Effect | Subtitle Style > Effect |
| Vanish | **None**, Fade Out, Burn Out, Paper Peel | [Subtitles] Exit | Subtitle Style > Exit |
| Back | | | "Back to Interface." |

#### Interface > Pause Menu

| Row | Values | ini / save | Was |
|---|---|---|---|
| Storage Chest | Hidden (ini `0`), **Shown** (ini `1`) | [ExtraMenu] Chest | Option 6 > Storage Chest |
| Save Game | Hidden (ini `0`), **Shown** (ini `1`) | [ExtraMenu] Save | Option 6 > Save Game |
| Load Game | Hidden (ini `0`), **Shown** (ini `1`) | [ExtraMenu] Load | Option 6 > Load Game |
| Quick Save | Hidden (ini `0`), **Shown** (ini `1`) | [ExtraMenu] QuickSave | Option 6 > Quick Save |
| Quick Load | Hidden (ini `0`), **Shown** (ini `1`) | [ExtraMenu] QuickLoad | Option 6 > Quick Load |
| Eileen | Hidden (ini `0`), **Shown** (ini `1`) | [ExtraMenu] Eileen | Option 6 > Eileen Outfit (v3.55.0: Eileen Outfit) |
| Save Banks | Off, **On** | [SaveBanks] Enabled | Option 6 > Save Banks |
| Back | | | "Back to Interface." |

#### Sound (top page 4)

| Row | Values | ini / save | Was |
|---|---|---|---|
| Sound Mode | Mono, **Stereo**, Headphones, Surround | cfg+0x0C (stock 0/1/2) + [Sound] Mode | Option 1 > Sound (+ v3.52's mode) |
| Music Volume | **10** | cfg+0x0D | Option 1 > BGM Volume |
| Effects Volume | **10** | cfg+0x0E | Option 1 > SE Volume |
| Stereo Width | **100 %**, 90 %, ..., 0 % | [Sound] Width (0..100, steps of 10) | v3.52 Option 1 > Sound > Stereo Width |
| Room Reverb | Off, **On** | [Sound] Reverb | v3.52 Option 1 > Sound > Room Reverb |
| Night Mode | **Off**, Low, High | [Sound] NightMode | v3.52 Option 1 > Sound > Night Mode |
| Background | **Mute**, Play | [Sound] Background | (new in v3.56.0) |
| Exit | | | the game's Exit |

#### Effects (top page 5)

| Row | Values | ini / save | Was |
|---|---|---|---|
| Film Grain > | opens **Effects > Film Grain** | | Option 3 > Noise Effect (+ Enter: Screen Effects rows 0-2) |
| Filter | **None**, CRT, Vivid, Sepia, Old Film, Night Vision, Night Mono (ini `Night Vision Mono`), Thermal, Spirit Vision, Vignette | [ScreenEffects] Filter | Screen Effects > Filter |
| Filter Strength | Low, Medium, **High**, Ultra | [ScreenEffects] FilterStrength (+ FilterStrengthV) | (new in v3.58.0; v3.15's Screen Effects > Filter Strength) |
| Cut-scene Filter | **On**, Off | [ScreenEffects] Cutscenes | Screen Effects > Cut-scenes |
| Bloom | **Off**, Low, Medium, High, Ultra | [ScreenEffects] Bloom | Screen Effects > Bloom |
| Colour Grade | **Off**, PS2, Otherworld, Apartment, Bleach | [ScreenEffects] Grading | Screen Effects > Colour Grading |
| Otherworld Lens | **Off**, On | [ScreenEffects] Lens | Screen Effects > Otherworld Lens |
| Depth of Field > | opens **Effects > Depth of Field** | | v3.58.0 (was the rows Depth of Field and Blur Strength) |
| Shadows > | opens **Effects > Shadows** | | Option 3 > Realtime Shadow (+ Enter: Shadows page) |
| Preview | Off, Stock, **Live** | [ScreenEffects] Preview | (new in v3.58.0; v3.56's hidden PreviewScene) |
| Exit | | | the game's Exit |

#### Effects > Depth of Field (v3.58.0)

| Row | Values | ini / save | Was |
|---|---|---|---|
| Depth of Field | **Off**, Cut-scenes, Always | [ScreenEffects] DepthOfField | Effects > Depth of Field (Screen Effects > Depth of Field) |
| Strength | Low, **Medium**, High | [ScreenEffects] DofStrength | Effects > Blur Strength (Screen Effects > DoF Strength) |
| Back | | | "Back to Effects." |

#### Effects > Film Grain

| Row | Values | ini / save | Was |
|---|---|---|---|
| Film Grain | Off (ini `0`), **On** (ini `1`) | cfg+0x14 | Option 3 > Noise Effect (stock On / Off) |
| Strength | Low, **Normal**, High, Ultra (ini `Very High`; v3.57: Very High) | [ScreenEffects] NoiseStrength | Screen Effects > Noise Strength |
| Grain Type | **Stock**, Fine, Coarse, Film, VHS, Scan Roll, Rain | [ScreenEffects] NoiseType | Screen Effects > Noise Type |
| Grain Colour | **Stock**, Mono, Full Colour, Red, Green, Blue, Amber, Sepia | [ScreenEffects] NoiseColour | Screen Effects > Noise Colour |
| Back | | | "Back to Effects." |

#### Effects > Shadows

| Row | Values | ini / save | Was |
|---|---|---|---|
| Shadows | Off (ini `0`; the game keeps a few rooms' shadows), **On** (ini `1`) | cfg+0x19 | Option 3 > Realtime Shadow (stock On / Off) |
| Edge | **Stock**, Soft | [Shadows] Edge | Shadows > Shadow Edge |
| Darkness | 25 %, 40 %, 55 %, 70 %, **85 %**, 100 % | [Shadows] Darkness | Shadows > Shadow Darkness |
| Back | | | "Back to Effects." |

#### Gameplay (top page 6)

| Row | Values | ini / save | Was |
|---|---|---|---|
| Player > | opens **Gameplay > Player** | | Option 5 > Player (part) |
| Combat > | opens **Gameplay > Combat** | | Option 5 > Player (part) |
| Items > | opens **Gameplay > Items** | | Option 5 top rows |
| Enemies > | opens **Gameplay > Enemies** | | Option 5 > Enemies |
| Powers > | opens **Gameplay > Powers** | | Option 5 > Special |
| Randomizer > | opens **Gameplay > Randomizer** | | Option 5 > Randomizer |
| Blood Colour | **Red**, Green, Purple, Yellow, Orange, Pink, Blue, Cyan, White, Black | cfg+0x13 | Option 3 > Blood Colour |
| Exit | | | the game's Exit |

#### Gameplay > Player

| Row | Values | ini / save | Was |
|---|---|---|---|
| Move Speed | **Normal**, x1.25, x1.5, x2 | [Gameplay] MoveSpeed | Option 5 > Player > Movement Speed |
| Regeneration | **Off**, x1, x1.25, x2 | [Gameplay] Regen | Option 5 > Player > Regeneration |
| Medallion | **Normal**, x2, x3, x4 | [Gameplay] MedallionDurability | Option 5 > Player > Medallion Durability |
| Grab Escape | **Stock**, Quick, Instant | [Gameplay] GrabBreaker | Option 5 > Player > Grab Breaker |
| No Clip | **Off**, On, Fly | [Gameplay] NoClip | Option 5 > No Clip |
| Back | | | "Back to Gameplay." |

#### Gameplay > Combat

| Row | Values | ini / save | Was |
|---|---|---|---|
| One-Hit Kill | **Off**, On | [Gameplay] OneHitKill | Option 5 > Player > One-Hit Kill |
| Dark Pact | **Off**, Pact, Blood | [Gameplay] DarkPact | Option 5 > Player > Dark Pact |
| Quick Charge | **Off**, x1.5, x2, x3 (ini 1.5x / 2x / 3x) | [Gameplay] QuickCharge | Option 5 > Player > Quick Charge |
| Life Steal | **Off**, 5 % (ini `5`), 10 % (ini `10`) | [Gameplay] LifeSteal | Option 5 > Player > Life Steal |
| Ammo Saver | **Normal**, Saver, Endless | [Gameplay] AmmoSaver | Option 5 > Player > Ammo Saver |
| Back | | | "Back to Gameplay." |

#### Gameplay > Items

| Row | Values | ini / save | Was |
|---|---|---|---|
| Item Shine | Off, **On** | [Items] Shine | Option 5 > Item Shine |
| Toolbelt | **10 Items** (ini `Off`), 20 Items (ini `On`) | [Gameplay] ExtendedToolbelt | Option 5 > Extended Toolbelt |
| Item Stacks | **Off**, On | [Gameplay] StackableItems | Option 5 > Stackable Items |
| Supply Drops | **Off**, Rare, Normal, Abundant | [Gameplay] SupplyDrops | Option 5 > Enemies > Supply Drops |
| Back | | | "Back to Gameplay." |

#### Gameplay > Enemies

| Row | Values | ini / save | Was |
|---|---|---|---|
| Extra Enemies | **Off**, x2, x3, x4 | [Gameplay] EnemyCopies | Option 5 > Enemies > Enemy Copies |
| Door Chase | **Off**, Monsters, Victims, Both (ini `Monsters + Victims`) | [Gameplay] EnemyFollow | Option 5 > Enemies > Enemies Follow |
| Pursuit | Relentless, **Normal**, Forgetful | [Gameplay] EnemyPursuit | Option 5 > Enemies > Enemy Pursuit |
| Enemies Return | **Off**, By Room, By World | [Gameplay] EnemyRevive | Option 5 > Enemies > Revive Enemies (v3.55: Off / On) |
| Reveal Method | Off, Fade In (ini `Fade`), **Portal** (ini `Fade + Portal`) | [Gameplay] FadeIn | Option 5 > Enemies > Fade In (v3.57: Fade In, values Off / Fade / Portal) |
| Back | | | "Back to Gameplay." |

#### Gameplay > Powers

| Row | Values | ini / save | Was |
|---|---|---|---|
| Death Presence | **Off**, Weak, Strong | [Gameplay] DeathPresence | Option 5 > Special > Death Presence |
| After Shock | **Off**, Short, Medium, Long | [Gameplay] AfterImage | Option 5 > Special > After Shock |
| Echo Doubles | **Off**, Copy (ini `Shadow Copy`), Cast (ini `Shadow Cast`) | [Gameplay] Echo | Option 5 > Special > Echo Doubles |
| Echo Count | **1**, 2, 3 | [Gameplay] EchoCount | Option 5 > Special > Echo Count |
| Ghost Teleport | **Off**, On | [Gameplay] GhostTeleport | Option 5 > Special > Ghost Teleport |
| Back | | | "Back to Gameplay." |

#### Gameplay > Randomizer

| Row | Values | ini / save | Was |
|---|---|---|---|
| Items | Off, Shuffle, **Random** | [Randomizer] Items | Randomizer > Items |
| Specials | Off, **On** | [Randomizer] Specials (v3.55's KeepSpecials migrated: On -> Off, Off -> On) | Randomizer > Keep Specials |
| Room 302 Items | Off, Shuffle, **Random** | [Randomizer] Room302 (v3.55's In -> the Items value, Out -> Off) | Randomizer > Room 302 (Stay / Change) |
| Enemies | Off, **Same World**, All Worlds | [Randomizer] Enemies | Randomizer > Monsters (v3.56.0: renamed; This Room gone) |
| Spawn Site | Off, **On** | [Randomizer] SpawnSite | Randomizer > Ghosts (v3.56.0: renamed; Spots = On; monsters too) |
| Eileen Outfit | Off, **Random** | [Randomizer] EileenOutfit | Randomizer > Eileen Outfit |
| Reroll | **By Room**, By World | [Randomizer] Reroll (v3.55's PerVisit: On -> By Room, Off -> By World) | Randomizer > Change Each Visit |
| Back | | | "Back to Gameplay." |

The seed is drawn in small type under the page's description (`[Randomizer] Seed`; v3.56.0: no Seed / New Seed rows).

### Old place -> new place (v3.55.0)

Every row: where it was before the plan (Option N > Row, v3.51 - v3.53), where v3.54.0 had it, and where it is now.
"pause > Camera" is the pause Extra Menu's Camera page (it keeps its rows, now with these names).

| Was (v3.51 - v3.53) | v3.54.0 (where it differs) | Now (v3.55.0) |
|---|---|---|
| Option 1 > Resolution | the same | **Display > Resolution** |
| Option 4 > Aspect Ratio | the same | **Display > Aspect Ratio** |
| Option 4 > Display Mode | the same | **Display > Display Mode** |
| Option 4 > Monitor | the same | **Display > Monitor** |
| Option 4 > Frame Rate | the same | **Display > Frame Rate** |
| Option 4 > Renderer | Effects > Renderer | **Display > Renderer** |
| Option 1 > Brightness | the same | **Display > Brightness >** |
| Option 1 > Gamma | the same | **Display > Gamma >** |
| Option 2 > Button Config | the same | **Controls > Buttons >** |
| Option 2 > Default Control | the same | **Controls > Default Speed** |
| Option 2 > Vibration | the same | **Controls > Vibration** |
| Option 2 > Omni-Directional Movement | the same | **Controls > Analog Movement** |
| Option 6 > Camera + pause Extra Menu > Camera | the same | **Controls > Camera >** |
| Option 4 > Room 302 + Option 2 first-person rows | the same | **Controls > Room 302 >** |
| Option 4 > 3rd Person FOV | the same | **Controls > Camera > Field of View** |
| Option 6 > Camera | the same | **Controls > Camera > Camera** |
| pause > Extra Menu > Camera > Distance | the same | **Controls > Camera > Distance** |
| pause > Camera > Side | the same | **Controls > Camera > Side** |
| pause > Camera > Height | the same | **Controls > Camera > Height** |
| pause > Camera > Tilt down | the same | **Controls > Camera > Tilt Down** |
| pause > Camera > Fixed cameras | the same | **Controls > Camera > Fixed Angles** |
| pause > Camera (rows 5-10) | the same | **Controls > Camera > Steering >** |
| pause > Camera > Turn speed | the same | **Controls > Camera > Steering > Turn Speed** |
| pause > Camera > Tilt speed | the same | **Controls > Camera > Steering > Tilt Speed** |
| pause > Camera > Horizontal invert | the same | **Controls > Camera > Steering > Invert Turn** |
| pause > Camera > Vertical invert | the same | **Controls > Camera > Steering > Invert Tilt** |
| pause > Camera > Wall zoom | the same | **Controls > Camera > Steering > Wall Zoom** |
| pause > Camera > Wall margin | the same | **Controls > Camera > Steering > Wall Margin** |
| Option 4 > 1st Person FOV | the same | **Controls > Room 302 > Field of View** |
| Option 4 > Room 302 > Look Down | the same | **Controls > Room 302 > Look Down** |
| Option 4 > Room 302 > Look Up | the same | **Controls > Room 302 > Look Up** |
| Option 4 > Room 302 > Look Feel | the same | **Controls > Room 302 > Look Smoothing** |
| Option 2 > First Person Camera | Controls > Invert Look | **Controls > Room 302 > Invert Look** |
| Option 2 > Head Motion | Controls > Head Bob | **Controls > Room 302 > Head Bob** |
| Option 2 > First Person Movement | Controls > Left / Right | **Controls > Room 302 > Turn or Strafe** |
| Option 4 > Room 302 > Walk Speed | the same | **Controls > Room 302 > Walk Speed** |
| Option 3 > Language | the same | **Interface > Language** |
| Option 3 > Subtitles (+ Enter: Subtitle Style) | Interface > Subtitles (On / Off, Enter: the page) | **Interface > Subtitles >** |
| Option 3 > Hide Gauge | the same | **Interface > Health Gauge** |
| Option 3 > Hide Icon | the same | **Interface > Item Icons** |
| Option 5 > Enemies > Enemy Health Bars | the same | **Interface > Enemy Health** |
| Option 5 > Curse Meter | the same | **Interface > Curse Meter** |
| Option 1 > Menu Font | Interface > Menus > Menu Font | **Interface > Menu Font** |
| Option 4 > Opening | Interface > Menus > Start-Up | **Interface > Start-Up** |
| Option 6 (Extra Menu) rows 0-6 | Interface > Menus | **Interface > Pause Menu >** |
| Option 3 > Subtitles (stock On / Off) | Interface > Subtitles (its On / Off) | **Interface > Subtitles > Subtitles** |
| Subtitle Style > Size | the same | **Interface > Subtitles > Size** |
| Subtitle Style > Colour | the same | **Interface > Subtitles > Colour** |
| Subtitle Style > Edge | the same | **Interface > Subtitles > Edge** |
| Subtitle Style > Box | the same | **Interface > Subtitles > Box** |
| Subtitle Style > Effect | the same | **Interface > Subtitles > Appear** |
| Subtitle Style > Exit | the same | **Interface > Subtitles > Vanish** |
| Option 6 > Storage Chest | the same | **Interface > Pause Menu > Storage Chest** |
| Option 6 > Save Game | the same | **Interface > Pause Menu > Save Game** |
| Option 6 > Load Game | the same | **Interface > Pause Menu > Load Game** |
| Option 6 > Quick Save | the same | **Interface > Pause Menu > Quick Save** |
| Option 6 > Quick Load | the same | **Interface > Pause Menu > Quick Load** |
| Option 6 > Eileen Outfit | the same | **Interface > Pause Menu > Eileen** (v3.55.0 "Eileen Outfit") |
| Option 6 > Save Banks | the same | **Interface > Pause Menu > Save Banks** |
| Option 1 > Sound (+ v3.52's mode) | Display > Sound Mode | **Sound > Sound Mode** |
| Option 1 > BGM Volume | Display > BGM Volume | **Sound > Music Volume** |
| Option 1 > SE Volume | Display > SE Volume | **Sound > Effects Volume** |
| v3.52 Option 1 > Sound > Stereo Width | the same | **Sound > Stereo Width** |
| v3.52 Option 1 > Sound > Room Reverb | the same | **Sound > Room Reverb** |
| v3.52 Option 1 > Sound > Night Mode | the same | **Sound > Night Mode** |
| Option 3 > Noise Effect (+ Enter: Screen Effects rows 0-2) | the same | **Effects > Film Grain >** |
| Screen Effects > Filter | the same | **Effects > Filter** |
| Screen Effects > Cut-scenes | the same | **Effects > Cut-scene Filter** |
| Screen Effects > Bloom | the same | **Effects > Bloom** |
| Screen Effects > Colour Grading | the same | **Effects > Colour Grade** |
| Screen Effects > Otherworld Lens | the same | **Effects > Otherworld Lens** |
| Screen Effects > Depth of Field | the same | **Effects > Depth of Field** |
| Screen Effects > DoF Strength | the same | **Effects > Blur Strength** |
| Option 3 > Realtime Shadow (+ Enter: Shadows page) | the same | **Effects > Shadows >** |
| Option 3 > Noise Effect (stock On / Off) | Interface > Film Grain | **Effects > Film Grain > Film Grain** |
| Screen Effects > Noise Strength | the same | **Effects > Film Grain > Strength** |
| Screen Effects > Noise Type | the same | **Effects > Film Grain > Grain Type** |
| Screen Effects > Noise Colour | the same | **Effects > Film Grain > Grain Colour** |
| Option 3 > Realtime Shadow (stock On / Off) | Interface > Shadows | **Effects > Shadows > Shadows** |
| Shadows > Shadow Edge | the same | **Effects > Shadows > Edge** |
| Shadows > Shadow Darkness | the same | **Effects > Shadows > Darkness** |
| Option 5 > Player (part) | the same | **Gameplay > Player >** |
| Option 5 > Player (part) | the same | **Gameplay > Combat >** |
| Option 5 top rows | the same | **Gameplay > Items >** |
| Option 5 > Enemies | the same | **Gameplay > Enemies >** |
| Option 5 > Special | the same | **Gameplay > Powers >** |
| Option 5 > Randomizer | the same | **Gameplay > Randomizer >** |
| Option 3 > Blood Colour | Interface > Blood Colour | **Gameplay > Blood Colour** |
| Option 5 > Player > Movement Speed | the same | **Gameplay > Player > Move Speed** |
| Option 5 > Player > Regeneration | the same | **Gameplay > Player > Regeneration** |
| Option 5 > Player > Medallion Durability | the same | **Gameplay > Player > Medallion** |
| Option 5 > Player > Grab Breaker | the same | **Gameplay > Player > Grab Escape** |
| Option 5 > No Clip | the same | **Gameplay > Player > No Clip** |
| Option 5 > Player > One-Hit Kill | the same | **Gameplay > Combat > One-Hit Kill** |
| Option 5 > Player > Dark Pact | the same | **Gameplay > Combat > Dark Pact** |
| Option 5 > Player > Quick Charge | the same | **Gameplay > Combat > Quick Charge** |
| Option 5 > Player > Life Steal | the same | **Gameplay > Combat > Life Steal** |
| Option 5 > Player > Ammo Saver | the same | **Gameplay > Combat > Ammo Saver** |
| Option 5 > Item Shine | the same | **Gameplay > Items > Item Shine** |
| Option 5 > Extended Toolbelt | the same | **Gameplay > Items > Toolbelt** |
| Option 5 > Stackable Items | the same | **Gameplay > Items > Item Stacks** |
| Option 5 > Enemies > Supply Drops | the same | **Gameplay > Items > Supply Drops** |
| Option 5 > Enemies > Enemy Copies | the same | **Gameplay > Enemies > Extra Enemies** |
| Option 5 > Enemies > Enemies Follow | the same | **Gameplay > Enemies > Door Chase** |
| Option 5 > Enemies > Enemy Pursuit | the same | **Gameplay > Enemies > Pursuit** |
| Option 5 > Enemies > Revive Enemies | the same | **Gameplay > Enemies > Enemies Return** |
| Option 5 > Enemies > Fade In | the same | **Gameplay > Enemies > Fade In** |
| Option 5 > Special > Death Presence | the same | **Gameplay > Powers > Death Presence** |
| Option 5 > Special > After Shock | the same | **Gameplay > Powers > After Shock** |
| Option 5 > Special > Echo Doubles | the same | **Gameplay > Powers > Echo Doubles** |
| Option 5 > Special > Echo Count | the same | **Gameplay > Powers > Echo Count** |
| Option 5 > Special > Ghost Teleport | the same | **Gameplay > Powers > Ghost Teleport** |
| Randomizer > Items | the same | **Gameplay > Randomizer > Items** |
| Randomizer > Keep Specials | the same | **Gameplay > Randomizer > Specials** (v3.56.0: turned round) |
| Randomizer > Room 302 | the same | **Gameplay > Randomizer > Room 302 Items** |
| Randomizer > Monsters | the same | **Gameplay > Randomizer > Enemies** (v3.56.0) |
| Randomizer > Ghosts | the same | **Gameplay > Randomizer > Spawn Site** (v3.56.0) |
| Randomizer > Eileen Outfit | the same | **Gameplay > Randomizer > Eileen Outfit** |
| Randomizer > Change Each Visit | the same | **Gameplay > Randomizer > Reroll** |
| (new in v3.56.0) | - | **Sound > Background** |

v3.56.0: the Seed and New Seed rows are gone (the seed in small type under the Randomizer page's description); the pause
Extra Menu's own row reads **Eileen** with its outfit after it ("Eileen Default", "Eileen Dress", "Eileen Nurse"; v3.55.0
"Eileen Outfit Default"). The Randomizer's row **Eileen Outfit** keeps its name.

### Old name -> new name (v3.56.0)

| Where | v3.55.0 | v3.56.0 | ini |
|---|---|---|---|
| Interface > Pause Menu | Eileen Outfit | **Eileen** | `[ExtraMenu] Eileen` (unchanged) |
| pause > Extra Menu | Eileen Outfit Default / Dress / Nurse | **Eileen** Default / Dress / Nurse | - |
| Display > Renderer's lines | "Direct3D 11 through dgVoodoo2 (default). Next start." ... | "Direct3D 11: dgVoodoo2, the default. Next start.", "Stock: ...", "Custom: ...", "Unavailable: ..." | - |
| Gameplay > Blood Colour's lines | "Makes blood colour yellow." | "Yellow: yellow blood." (Red / Green / Purple have lines too) | - |
| Gameplay > Enemies | Enemies Return: Off / On | Enemies Return: Off / **By Room** / **By World** (On = By Room) | `[Gameplay] EnemyRevive` (On read once as By Room, written back) |
| Gameplay > Randomizer | Keep Specials (Off / On) | **Specials** (Off / On, turned round: Off = they stay) | `KeepSpecials` -> `Specials` (once) |
| Gameplay > Randomizer | Room 302 Items (Stay / Change) | Room 302 Items (**Off** / **Shuffle** / **Random**) | `Room302` In -> the Items value, Out -> Off (once) |
| Gameplay > Randomizer | Monsters: Off / This Room / This World / All Worlds | **Enemies**: Off / Same World / All Worlds | `Monsters` -> `Enemies` (This Room, This World -> Same World) |
| Gameplay > Randomizer | Ghosts: Off / Spots | **Spawn Site**: Off / On | `Ghosts` -> `SpawnSite` (Spots -> On) |
| Gameplay > Randomizer | Reroll Visits (Off / On) | Reroll Visits (**By Room** / **By World**) | `PerVisit` -> `Reroll` (On -> By Room, Off -> By World) |
| Gameplay > Randomizer | Seed (its number), New Seed | (gone: the seed in small type under the description) | `Seed` (unchanged) |
| Gameplay > Randomizer | "Next room." at the end of each line | (gone) | - |
| Sound | - | **Background** (Mute / Play) | `[Sound] Background` (new) |

Every value has its own line now (MN356: 142 values in 48 rows, the Randomizer page and Enemies Return by their owners); a
number keeps the row's line. The Options layout plan's copy of these changes: `staging/v356_feedback/merge356_tools/LAYOUT_CHANGES.md`.

### Old name -> new name (v3.57.0)

Options pass 1 (the Options Layout session's review of every row, `staging/options_layout/ASSESSMENT.md`): no row moves,
no ini key or written value changes. Three names change; 48 rows change only their descriptions (the conventions N1 - N3,
D1 - D9 of the assessment: one short line, "as in the game", "you", no seconds or symbols where words do, one name for
one thing).

| Where | v3.56.0 | v3.57.0 | ini |
|---|---|---|---|
| Controls > Room 302 | Left / Right (Turn / Strafe) | **Turn or Strafe** (Turn / Strafe); line "What left and right do." | cfg+0x17 Rotate / Sideways (unchanged) |
| Gameplay > Combat > Quick Charge | Off / 1.5x / 2x / 3x | Off / **x1.5** / **x2** / **x3** (as Move Speed, Medallion, Regeneration) | `[Gameplay] QuickCharge` Off / 1.5x / 2x / 3x (unchanged) |
| Gameplay > Randomizer | Reroll Visits (By Room / By World) | **Reroll** (By Room / By World); line "When to draw again. Loading a save never does." | `[Randomizer] Reroll` (unchanged) |
| Gameplay > Player (folder) | "Speed, healing, escapes, damage and ammo." (the old Option 5 folder record) | "Speed, healing and escapes." (Combat has its own folder) | - |
| Display > Frame Rate > 120 / 180 | "shown only with HighFrameRates=1." | "120: experimental; needs a fast PC." / "180: ..." | - |
| state lines | "Not available ... (see SH4Refined.log)." | "Not available ... . See SH4Refined.log." (16 such lines, the Filter's F-key sentence " Press F7 in play for a new preview.") | - |

### Old name -> new name (v3.58.0)

The user's v3.57 QA (`staging/v358_feedback/USER_FEEDBACK_v357.md`). No ini key changes its words; two rows are new, two
rows move into a new folder.

| Where | v3.57.0 | v3.58.0 | ini |
|---|---|---|---|
| Gameplay > Enemies | Fade In: Off / Fade / Portal | **Reveal Method**: Off / **Fade In** / Portal | `[Gameplay] FadeIn` Off / Fade / Fade + Portal (unchanged; a typed `Fade In` reads as Fade) |
| Effects > Film Grain | Strength: ... / Very High | Strength: ... / **Ultra** | `[ScreenEffects] NoiseStrength` Very High (unchanged; a typed `Ultra` reads as Very High) |
| Gameplay > Enemies | Extra Enemies: "... Next room." in every line | no "Next room." (the copies still come at the next room load) | - |
| Gameplay > Items | Item Stacks On: "On: pickups, gifts and the chest stack." | "On: supplies, ammo and items stack up to 99." | - |
| Effects > Shadows | Shadows Off: "Off: no real-time shadows." | "Off: none (the game keeps them in a few rooms)." | cfg+0x19 (unchanged) |
| Effects | Depth of Field | Effects > **Depth of Field >** > Depth of Field (the row moved into the folder; same values, same line) | `[ScreenEffects] DepthOfField` (unchanged) |
| Effects | Blur Strength | Effects > Depth of Field > **Strength** (line "How strong the blur is."; values and their lines unchanged) | `[ScreenEffects] DofStrength` (unchanged) |
| Effects | (none) | **Depth of Field >** (a folder row: "Blur by distance, and how strong.") | - |
| Effects | (none) | **Filter Strength** (Low / Medium / High / Ultra) | `[ScreenEffects] FilterStrength` (new, + the marker `FilterStrengthV`) |
| Effects | (none) | **Preview** (Off / Stock / Live) | `[ScreenEffects] Preview` (new) |
| hidden | `[ScreenEffects] PreviewScene` | `[ScreenEffects] Preview` (migrated once) | Scene / Picture -> Live |

With the hidden `[Options] StockMoves=0` (v3.54's interim layout) Renderer now sits on the Sound page instead of Effects
(Effects is full at 11). Every Options value arrow now stands in one value box per page (MN358, above). The Options
layout plan's copy of these changes: `staging/v358_feedback/merge358_tools/LAYOUT_CHANGES.md`.

### Known limits (v3.58.0)

- Not yet installed or tested in game (every v3.58 part is built and tested offline only).
- **Background Mute after an exit behind other windows (SD358, SD358-001):** Windows remembers only "muted", not who muted
  it. SH4Refined.muted names the sessions Refined muted, and the next start unmutes those if they are still muted: a mute
  made on the same session by something else after the game ended (Windows' Volume Mixer does not list a game that is not
  running; another mixer program or an edit of Windows' settings could) is undone at that start too. A session whose
  identity cannot be read is never muted by Refined (so it can never be left muted by it). Between the start of the game and
  its first frame (about a second) a mute left by the last exit is still on. A sound session on a device that is not there at
  the next start stays named (and muted) until a start finds it, 30 days at most; the log names it.
- **SH4Refined.muted cannot be written (SD358-002):** it is written whole to a temporary file and then put in place; if that
  fails (a full disk, a read-only game folder), the old one stays and Refined does not mute a session it could not name: that
  session's sound goes on behind other windows (logged, at most 3 lines; sounds that start later follow DirectSound's own
  focus rule). If the rewrite fails right after you changed a muted session in the Volume Mixer, the next start may still
  unmute that one. (v3.58 SD358-003: a failed rewrite is tried again at each later pass, every 2 s behind other windows and
  at the next focus change; the same holds when a rewrite after an unmute fails and you mute that session in the Volume Mixer
  before a rewrite works, then the game is killed.)
- **A mute that was not confirmed (SD358-003):** each entry of SH4Refined.muted is written as "pending" before Refined mutes
  the session and rewritten as "muted" once the mute worked; only a "muted" entry unmutes at the next start, so a failed mute
  can never undo a mute you make later. If the mute worked but no rewrite reached the file before the game was killed, the
  next start leaves that session muted and the log says so ("pending ... if the game stays silent, unmute it in the Windows
  Volume Mixer"): unmute the game there.
- **Value box (MN358):** a value whose text changes with the value (a number row) and turns out wider than the page's measure
  grows the box once (logged); the page keeps the wider box until it is shown again. Without the themed layout (it failed to
  build) or with one of the four old hooks not stock, the old arrows (logged).
- **Preview / Filter Strength (PV358):** Thermal and Spirit Vision over the stock picture (title menu, or Stock in a pause) show
  only their palette / edge world: a picture has no bodies or ghosts to redraw (their hot bodies / ghosts show over a pause's
  own scene with Live). Filter Strength Low / Medium copies the frame once a frame while a filter is on (one more full-screen
  target of the screen's size: 14.1 MB at 2560x1440, released when the strength goes back to High / Ultra or the Filter to
  None, and at every device reset); if the copy fails 3 times the filter shows at High for the session (logged). Ultra draws
  Thermal and Spirit Vision as High. In the frame a pause picture is taken, the preview's heat layer writes the bodies'
  depth (as live Thermal does): sprites behind a close-camera-faded player may be hidden by her in that one frozen picture.
  With Preview Off the Shadows page shows no split preview; its Darkness row then says "How dark shadows are.".
- **Randomizer (RZ358):** Enemies Return Off: a randomized body that went through a save and a load stays dead even after you
  leave that world through its hole and come back (the game itself would bring it back there). After a load a randomized
  body is not shown (it stays dead); its bodies of the stock type are shown as before. A Tremer still stands again at its
  room's next visit 4 times in 10 when killed with Enemies Return Off (the game's own coin flip at its death; By World keeps
  every Tremer's body). A victim ghost pinned by the Sword keeps its pin through rerolls; a pinned randomized ghost is left
  out of a save as before. With Spawn Site On copies stand away from their originals (sites keep 800 from where you
  arrive). A drawn copy type needs its files: at most 16 extra files a room (shared with the placements and Enemies
  Follow); copies of types that do not fit keep their original's type. A randomized body without a kill index (none in the
  stock game) whose type cannot be made in its room would stand again; logged.
- **Enemies (EN358):** a ghost that is completely see-through (inside a wall) has no bar until it is drawn again. A followed
  victim the randomizer drew is not in a save (as every retyped monster since v3.49): after a load in another room it is
  gone from there. Walter's revive bar is read from the GOG v7 code (two byte checks); if they differ (another mod), only
  his health bar shows while he is down (logged once). In-room Pursuit (Normal / Forgetful) acts on the male victim 0B only,
  as before; 0C / 0D victims chase by the game's rules.
- **Reveal Method's portal (FI358):** built and tested offline only (the layouts on `staging/v358_feedback/w5_portal/portal_sheet.svg`,
  the rules in the native test): the look in the game, the stacking of the overlapping pieces' layers and the strands'
  thickness are first judged by the user. The game skips a whole grid piece when any of its points is off the screen (its
  own rule), so near the screen's edge a big portal loses some of its outer pieces. The cost on the graphics card is not
  measured (the log's "portal draw" is the processor's time); the own limit guards that time only. Thermal hides ghosts,
  so a ghost that arrives while Thermal is on has no portal (as before); a portal already open stays and reads as a cold
  dark stain. Under Spirit Vision the tar shows as dark outlines. With 8 portals open at once a ninth arrival fades in
  without a portal (as before; a portal whose strands still hold is never taken over).
- **Ghost Teleport (GT358):** particles and effects drawn after the actor models and behind her (fog sprites, sparks, blood
  mist) are hidden inside her outline while she fades; her real-time shadow goes and comes at half opacity; under Spirit
  Vision the embers already thrown by her soul fire fly on for a moment. `GhostTeleportFadeDraw=1` gives the one pass look
  (no pre-pass) for comparison.
- **Shadows / effect budget (GX358):** stock Off keeps the real-time shadows of Room 302's living room, most of the Water
  Prison and Hospital rooms 1 / 1F, and the Tremers' wall shadows everywhere (as the unmodded game does). Powers turned on
  together build up over 0.5 - 6 s (thinned first, then back); on a PC that holds the rate with every power the build-up is
  all that shows. A power whose own drawing costs under 0.03 ms a frame is not seen starting (then v3.56's rules apply).
  The guard can now step after about a second of clear slow motion once the budget is out of options: a heavy stretch can
  cost 180 for 10 minutes (the retry and the two-steps lock are v3.56's).
- **Sound (SD358):** the session mute was not run in the game in this round; the log says which way Mute works. 3D sounds
  started at 0 begin at the game's -96.5 dB (inaudible) until their next volume update (Play3D's own first SetVolume
  0x40A021 is not hooked). Unmuting the game in the mixer while it is behind other windows under Mute: Refined mutes it again
  within 2 s.
### Known limits (v3.56.0)

- Not yet installed or tested in game (every v3.56 part is built and tested offline only).
- **Folder click (MN356):** a click on a folder row's name (or on a row other than the cursor's) needs the menu mouse hooks of
  `[Window] Menus43=1` (the default); with `Menus43=0` a click on the cursor row's triangle or value column still opens it. A
  click while a page slides or fades, or while a page-strip click waits, is not taken (the log says so; the page strip uses
  the same rule): click again once the page is still.
- **Value lines (MN356):** a value line comes from the plan's words when the row's own code has none for that value; a value
  whose word is not the plan's (a number, a sixth monitor) keeps the row's line and the log names it once. The Film Grain and
  Shadows switches now show Refined's own "Off" / "On" (the same words) so their lines are found.
- **Sound (SD356):** the sound still stays on the device the game started with: connect the headset (or set it as Windows'
  default) before starting the game, or restart the game after the change; Refined says when a restart is needed. Background
  Play acts on sounds that start after the change; the FMVs (DirectShow) keep their own focus rule. The keep-awake signal
  cannot help when the silence has another cause (a Bluetooth headset switching to its hands-free profile); a headset that
  treats a -72 dBFS 45 Hz tone as silence may still fall asleep: raise `[Sound] KeepAwakeLevel` (up to 64) or use Background
  Play. With `[Sound] Enabled=0` none of this runs.
- **Frame rate (FR356):** the guard looks only at gameplay frames above 60 fps in FrameMode 0 (FrameMode 1 has its own
  `InterpAutoLower`); a step down lasts at least 10 minutes even if the slow scene was short, then one try at a room change;
  the Options row still shows the rate you chose while the guard holds a lower one (the log says which rate runs); Walter's
  arm-gun flash in the final fight is still 4 ticks long; the final room's ending fade (and every fade sized `0x80 / fps`) is
  1.5x quick at 180 only.
- **Powers (PW356):** the 30 % was chosen on an offline sheet (`tools/ots_fade_preview.py`): `[Gameplay] AfterImageOtsFade`
  changes it without a new build; a copy right at the camera is dimmed too; a tested step's verdict can be fooled when the
  scene changes within its 1.5 s; a PC that can never hold the rate sees a short thinning of the sparks at most every 20 - 120 s
  while the budget tests (the rate guard is the cure there); Ghost Teleport has nothing to thin; the Effects page's preview
  right after a thinned stretch can show Death Presence's trail fire thinner until play resumes.
- **Effects preview (PV356):** the pause layers need the pause picture on the GPU: the first pause that saves `preview.dds`
  and a pause whose frame F7 is saving keep no layers (logged); Death Presence, Echo and After Shock are not in the previews; a
  body another effect hid in the pause frame has no heat there; a pause taken while Thermal is live has no ghosts in its
  Spirit Vision preview; the title scene's ghost has no hauntings / portals of its own.
- **Randomizer items (RI356):** the visit counts are memory only: after a restart, a loaded save shows "the seed's own" draw
  of its world until you next enter a world (the same draw for every load, never a fresh one); a save of world A loaded while
  you are in world B takes back A's last visit of this session (if you entered A again after saving, that newer visit);
  without the subtitle hooks (another mod on 0x559B64 / 0x559D34) the seed line is drawn at full size (logged), without the
  description hook (`[Window] DescFit=0`) it is not drawn; Eileen Outfit keeps its own per-world counter: a load can still
  change the companion's outfit (cosmetic).
- **Enemies / Spawn Site (RE356):** a monster made a ghost may follow you through its world by the game's own ghost rule (not
  one of Refined's followers, copies or pursuers); a ghost outside its own world may be silent; the floor map takes up to
  40 ms at a room load.
- **Enemies Return (ER356):** By World's released event dogs of Subway room 2:2 (after their event): a load before you reach
  2:2 forgets their "stand again awake" mark and they come in their event mode; By World judges a Randomizer placement by its
  stock type at the world entry; killed copies (Enemy Copies) are never saved (a title load's world reset forgets them); the
  world-visit rule reads the game's own enemy manager (0xFD0BA4): a mod that wrote it would make a load look like a room change.

### Known limits (v3.55.0)

- Not yet installed or tested in game.
- Music Volume / Effects Volume: their bars are drawn by their record through one patch of the game's row draw
  (0x525672, 6 bytes, byte-checked). If another mod changed those bytes, the two rows keep the names BGM Volume /
  SE Volume (still on Sound, still bars) and the log says why.
- The Subtitles, Film Grain and Shadows switches (row 0 of their folders) also flip on Enter, as the game's own
  On / Off rows do.
- Clicking a name in the page strip needs the menu mouse hooks of `[Window] Menus43=1` (the default); with
  `Menus43=0` the strip is not clickable (Q / E and the arrows still work). A page in the same slot as the one shown
  (Display / Sound, Controls / Effects, Interface / Gameplay) or a click on a folder page goes through the fade, as a
  folder does; the others slide.
- Wall Zoom lists On before Off (the camera's own value order, as in the pause menu).
- Without the Options sub-screen fade (see the log) there are no folders, so the layout falls back to v3.54.0's
  places (as `StockMoves=0`).


## Options names (v3.53.0)

### Old name -> new name (v3.53.0)

| Where (today) | Old | New |
|---|---|---|
| Option 1 | page "Option 1 (Video&Sound)" | **Display** |
| Option 1 | Sound (Monaural / Stereo / Headphones / Surround (3D)) | **Sound Mode** (Mono / Stereo / Headphones / **Surround**) |
| Option 1 > Sound page | "Back to Option 1." | "Back to Display." |
| Option 2 | page "Option 2 (Control)" | **Controls** |
| Option 2 | Button Config | **Buttons** |
| Option 2 | Default Control | **Default Speed** (Walk / Run) |
| Option 2 | Head Motion (Off / Slight / On) | **Head Bob** (Off / Slight / **Full**) |
| Option 2 | First Person Movement (Rotate / Sideways) | **Left / Right** (**Turn** / **Strafe**) |
| Option 2 | First Person Camera (Normal / Reverse) | **Invert Look** (**Off** / **On**) |
| Option 2 | Omni-Directional Movement | **Analog Movement** |
| Option 3 | page "Option 3 (Others)" | **Interface** |
| Option 3 | Noise Effect | **Film Grain** |
| Option 3 | Hide Gauge (Off / On) | **Health Gauge** (**Always** / **Auto Hide**) |
| Option 3 | Hide Icon (Off / On) | **Item Icons** (**Always** / **Auto Hide**) |
| Option 3 | Realtime Shadow | **Shadows** |
| Subtitle Style | page "Subtitle Style" | **Subtitles** |
| Subtitle Style | Effect / Exit | **Appear** / **Vanish** |
| Subtitle Style | Edge: Outline+Shadow | Edge: **Both** |
| Screen Effects | page "Screen Effects" | **Effects** |
| Screen Effects | Noise Strength / Noise Type / Noise Colour | **Grain Strength** / **Grain Type** / **Grain Colour** |
| Screen Effects | Cut-scenes | **Cut-scene Filter** |
| Screen Effects | Filter: Night Vision Mono | Filter: **Night Mono** |
| Screen Effects | Colour Grading | **Colour Grade** |
| Screen Effects | DoF Strength | **Blur Strength** |
| Shadows | Shadow Edge / Shadow Darkness | **Edge** / **Darkness** |
| Option 4 | page "Option 4 (Presentation)" | **Presentation** |
| Option 4 | Monitor: Main Display / Secondary / Tertiary / Quaternary Display | **Main** / **Second** / **Third** / **Fourth** |
| Option 4 | Opening | **Start-Up** |
| Option 4 > Room 302 | page "Presentation: Room 302" | **Room 302** |
| Option 4 > Room 302 | Look Feel | **Look Smoothing** |
| Option 4 > Room 302 | Back: "Back to Option 5 (Gameplay)." (a text bug) | "Back to Presentation." |
| Option 5 | page "Option 5 (Gameplay)" | **Gameplay** |
| Option 5 | Extended Toolbelt (Off / On) | **Toolbelt** (**10 Items** / **20 Items**) |
| Option 5 | Stackable Items | **Item Stacks** |
| Option 5 | Special (folder) | **Powers** |
| Player | Movement Speed | **Move Speed** |
| Player | Medallion Durability | **Medallion** |
| Player | Grab Breaker | **Grab Escape** |
| Enemies | Enemy Copies | **Extra Enemies** |
| Enemies | Enemies Follow (Monsters + Victims) | **Door Chase** (**Both**) |
| Enemies | Enemy Pursuit | **Pursuit** |
| Enemies | Enemy Health Bars (Locked Target) | **Enemy Health** (**Target**) |
| Enemies | Revive Enemies | **Enemies Return** |
| Enemies | Fade In: Fade + Portal | Fade In: **Portal** |
| Special | Echo Doubles: Shadow Copy / Shadow Cast | Echo Doubles: **Copy** / **Cast** |
| Randomizer | Room 302 (Out / In) | **Room 302 Items** (**Stay** / **Change**) |
| Randomizer | Change Each Visit | **Reroll Visits** |
| Randomizer | "Next door." | "Next room." |
| Option 6 | page "Option 6 (Extra Menu)" | **Pause Menu** |
| Option 6 | Storage Chest ... Eileen Outfit: Off / On | **Hidden** / **Shown** |
| Option 6 | Camera: OTS / Game / Playable Eileen | Camera: **Shoulder** / Game / **Eileen** |
| Option 6 | (the unused Save Bank row's texts) | gone (Q / E in the save list choose the bank) |

### Screen words and ini words (the ini is unchanged)

| Row: screen word | ini value written |
|---|---|
| Quick Charge: x1.5 / x2 / x3 | `[Gameplay] QuickCharge` 1.5x / 2x / 3x |
| Camera: Game / Shoulder | `[Camera] Enable` 0 / 1 |
| Toolbelt: 10 Items / 20 Items | `[Gameplay] ExtendedToolbelt` Off / On |
| Door Chase: Both | `[Gameplay] EnemyFollow` Monsters + Victims |
| Enemy Health: Target | `[Gameplay] EnemyHealthBars` Locked Target |
| Echo Doubles: Copy / Cast | `[Gameplay] Echo` Shadow Copy / Shadow Cast |
| Reveal Method: Fade In / Portal | `[Gameplay] FadeIn` Fade / Fade + Portal |
| Film Grain > Strength: Ultra | `[ScreenEffects] NoiseStrength` Very High |
| Room 302 Items: Stay / Change | `[Randomizer] Room302` Out / In |
| Edge (Subtitles): Both | `[Subtitles] Edge` Outline+Shadow |
| Filter: Night Mono | `[ScreenEffects] Filter` Night Vision Mono |
| Menu Font: Cormorant / Crimson | `[Textures] MenuFont` CormorantGaramond / CrimsonText |
| Pause Menu rows: Hidden / Shown | `[ExtraMenu] *` 0 / 1 |
| Sound Mode: Surround | `[Sound] Mode` Surround |
| Health Gauge / Item Icons: Always / Auto Hide; Head Bob: Full; Turn or Strafe: Turn / Strafe; Invert Look: Off / On | the game's own Options bytes (cfg+0x09 / 0x0A / 0x08 / 0x17 / 0x16), unchanged |

The settings table (`## Settings`) keeps its keys; only the "(Options > ...)" paths in its descriptions change, by the
old -> new table above (for example `[Gameplay] MoveSpeed`: "Options > Gameplay > Player > Move Speed").

## Install

Close the game, then copy `winmm.dll` into the game folder (the folder with `SILENT HILL 4.exe`, for example
`G:\GOGGalaxy\Games\Silent Hill 4`).

- On the first start it writes `SH4WindowMod.ini` next to it. If `SH4PlayableEileen.ini` has a `[Window]` section,
  your values from there are copied (else those of `scripts\SH4Windowed.ini`, else the defaults).
- With **Playable Eileen**: use plugin **v0.82 or later** (v0.82 no longer has its own windowed mode and moves its old
  `[Window]` section out of `SH4PlayableEileen.ini` once `SH4WindowMod.ini` holds those values; a copy of the file before
  that is kept as `SH4PlayableEileen.ini.before_winsplit`). Next to plugin v0.81 or older this mod stays inactive (the
  log says so), because that plugin windows the game itself.
- With **SH4Windowed.asi** (Ultimate ASI Loader): remove `scripts\SH4Windowed.asi` first; this mod does the same and stays
  inactive while that one is loaded.
- `dxcfg.ini` (GOG's display settings) is not changed. With `FitWrapper=1` the mod answers its `display=` and
  `presentation=` lines itself (application, windowed). Note: `display=` only knows `desktop`, `application` or an
  exact mode like `1600x1200@60`; a value such as `1280x960` without `@60` means the desktop.

## Uninstall

Close the game and delete `winmm.dll` from the game folder (and `SH4WindowMod.ini` / `SH4WindowMod.log` if you like).
The game then keeps its stock window behaviour, the way GOG's DirectX wrapper sets it up (`dxcfg.ini`, `presentation=`):
it can minimise or pause when it loses focus again. Nothing else was changed on disk.

## Settings (`SH4WindowMod.ini`, read at game start)

The in-game Options menu wins: every setting it shows is written to this file the moment you change it (Display Mode and
Monitor when you leave Options with Yes), and "No" writes the old value back. The file is read when the game starts;
only `[Items]` is read again while the game runs, and only an edit you make to the file yourself changes anything then.

| Key | Default | Meaning |
|---|---|---|
| `[Window] Enable` | 1 | Windowed mode. 0 = the game keeps its own window (as GOG's dxcfg.ini sets it up). |
| `Width`, `Height` | 0, 0 | Client size in pixels; 0 = automatic (three quarters of the screen height). v1.9: new installs default to 0 (784x588 before; tiny on high-DPI screens). |
| `Resizable` | 1 | Window edges can be dragged. |
| `KeepActiveOnAltTab` | 1 | The game keeps running and never minimises when you switch window. |
| `CaptureMouse` | 1 | Cursor hidden and kept inside the window while the game is in front. |
| `SnapToStandard` | 1 | Drag-resizing snaps to standard heights in the picture's shape. |
| `NoMinimize` | 1 | The game's own minimise-on-focus-loss check is switched off (exe fix `no_minimize`, 0x415040). |
| `Borderless` | 0 | Options: Display > Display Mode. Whole screen without a frame instead of a window (v1.5; Options > Display Mode and Alt+Enter change it). |
| `Monitor` | 0 | Options: Display > Monitor. Screen for the game (v1.7; Options > Monitor changes it). 0 = the main screen; `2` = `\\.\DISPLAY2`. Not used with `Enable=0`. |
| `MonitorName`, `MonitorPath` | (empty) | Options: Display > Monitor. Written by the mod: the screen's own name and device path, so it is found again if Windows renumbers the screens. To pick a screen by hand, set `Monitor` to its number. |
| `Menus43` | 1 | Menus, text, HUD and movies 4:3 in the middle in wide shapes (v1.4); full-screen fills full width (v1.6). |
| `MenuPillars` | 1 | Black sides beside full-screen pictures (memos, Scrap Book, item box, map) in wide shapes (v1.8). Needs `Menus43=1`. |
| `FOV` | 0 | Options: Controls > Camera > Field of View. v3.3: the THIRD-PERSON field of view (was Option 4 > 3rd Person FOV; all worlds, the world rig's setter call 0x50629D, Playable Eileen's over-the-shoulder Fov included), in degrees, 60-120 in steps of 5, 0 = the game's own; saved and applied at once: the horizontal angle of the usual third-person camera (63 deg vertical) at the picture's shape; every camera scales by the same factor. v1.9's `FieldOfView` (percent) is converted once (at 16:9). Cutscenes keep theirs. |
| `FOVFirstPerson` | (FOV) | Options: Controls > Room 302 > Field of View. v3.3: the FIRST-PERSON field of view (was Option 4 > 1st Person FOV; Room 302, the setter calls 0x500829 / 0x500B9E); its degrees are the side-to-side angle of Room 302's own 55 deg (vertical) camera. Added once as a copy of `FOV`, so an update changes nothing until a new angle is picked. v3.3.5: Default or 100-120 in steps of 5; an older value is moved once (below 95 -> 0, the game's own; 95 -> 100; above 120 -> 120). While Room 302's window or peephole views run (or the view turns to them) the game's own angle is used, eased in and out over 0.3 s. |
| `[Items] Shine` | 1 | Options: Gameplay > Items > Item Shine. v3.3 (v3.3.5: renamed from `Sparkle`): a shine on items and memos lying in the worlds (was Option 5 > Item Shine). Only items the player can see and take (the game's own enabled bit); not in Room 302. It keeps going through pickups and stays, frozen, in the paused picture. An old `Sparkle*` key in the file is converted to its `Shine*` name (also while the game runs). |
| `[Gameplay] MoveSpeed` | Normal | Options: Gameplay > Player > Move Speed. v3.25.0 (was Option 5 > Player > Movement Speed): Normal, x1.25, x1.5 or x2; her walk / run travel and animation, and Room 302's walk. x2 may pass through thin walls. |
| `[Gameplay] MedallionDurability` | Normal | Options: Gameplay > Player > Medallion. v3.25.0 (was Option 5 > Player): Normal, x2, x3 or x4: the Saint Medallion wears that many times slower against ghosts (worlds and Room 302). |
| `[Gameplay] OneHitKill` | Off | Options: Gameplay > Combat > One-Hit Kill. v3.25.0 (was Option 5 > Enemies): On = your hits kill monsters at once and knock ghosts down; Walter, the Conjoined Bodies and the Wall boss as normal. |
| `[Gameplay] DeathPresence` | Off | Options: Gameplay > Powers > Death Presence. v3.27.0 (was Option 5 > Special): Off, Weak (radius 300, 2-8 % of max health a second) or Strong (400, 5-20 %): the black-violet fog that drains every being round you. |
| `[Gameplay] AfterImage` | Off | Options: Gameplay > Powers > After Shock. v3.29.0 (was Option 5 > Special, After Shock since v3.30.0): Off, Short, Medium or Long (3 / 5 / 8 copies; v3.29's On reads as Medium). Hidden, not written: `AfterImageSound` 1, `AfterImageDryRun` 1 = look only (no hits), `AfterImageSkip` enemy types (hex) never stunned; `AfterImageTrail` is retired (v3.30.0). |
| `[Gameplay] AfterImageOtsFade` | (absent) 30 | v3.56.0, hidden: in the over-the-shoulder camera, the light (in percent, 0 - 100) an After Shock copy keeps when it sits right over the player between her and the camera; copies beside or behind her are not dimmed. 100 = v3.55 (no fade). |
| `[Gameplay] Echo` | Off | Options: Gameplay > Powers > Echo Doubles. v3.29.0 (was Option 5 > Special, Echo Doubles since v3.30.0): Off, Shadow Copy or Shadow Cast (v3.29's Shadow, Twin and Mirror read as Shadow Copy). Hidden: `EchoDamage` percent, `EchoSide`, `EchoColour`, `EchoDepth`, `EchoRim` (see the default ini's comment). |
| `[Gameplay] EchoCount` | 1 | Options: Gameplay > Powers > Echo Count. v3.30.0 (was Option 5 > Special > Echo Count): 1, 2 or 3 shadows. |
| `[Gameplay] EnemyCopies` | Off | Options: Gameplay > Enemies > Extra Enemies. v3.29.0 (was Option 5 > Enemies): Off, x2, x3 or x4 monsters and victims per room. The bone pool grows to 4096 records at the next New Game or load; crowded rooms get fewer copies. Hidden: `BonePool` Auto (or a record count). |
| `[Gameplay] CopyRecordGuard` | (absent) 1 | v3.58.0, hidden: Extra Enemies' copies and the 64 enemy records. 1 = copies only while the room's exit keeps every real record of this world (an old world's left-over record may go, the game's own first choice); 2 = only while it needs no real record at all (free slots only); 0 = as v3.57 (no test). Any other value: 1. |
| `[Gameplay] EnemyFollow` | Off | Options: Gameplay > Enemies > Door Chase. v3.29.0 (was Option 5 > Enemies): Off, Monsters, Victims (v3.49.0 as Victims Only; v3.50.0: renamed, `Victims Only` still reads, logged once) or Monsters + Victims follow you through doors. Victims: the victim ghosts (the story ghosts too while `FollowStoryGhosts` is 1, their copies while `FollowCopies` is 1) follow, no monster or monster copy does; the old values read as before. Hidden: `FollowMax` 1-8 (6; v3.34.0, was 1-3 (3)), `FollowCopies` 1 (v3.34.0: copies follow too; 0 = v3.33), `FollowDelay` seconds 0-30 (3), `FollowRange` units 200-3000 (800). |
| `[Gameplay] EnemyPursuit` | Normal | Options: Gameplay > Enemies > Pursuit. v3.50.0 (was Option 5 > Enemies > Enemy Pursuit): Relentless (v3.49: once spotted, followed through every door), Normal (a monster follows through a door only if it sensed you in the last 10 s; a downed pursuer stays; 2 doors without catching up and it stays) or Forgetful (4 s, 1 door). Ghosts sense you through walls; a downed ghost stays behind. |
| `[Gameplay] FollowSaveFile` | (absent) 1 | v3.50.0, hidden: 1 = the followers survive a save and its load (side file `save\SH4Refined_follow.dat`, the last 64 saves with followers; hooks 0x4FC1A4 / 0x4FC6ED); 0 = both hooks stay stock (followers are not kept across saves). |
| `[Gameplay] PursuitInRoom` | (absent) 1 | v3.51.0, hidden: 1 = Enemy Pursuit Normal / Forgetful also acts inside the room (monsters give up after 10 / 4 s unsensed; victims' ghosts when far 2500 / 1200 u for 12 / 3 s or after a 25 / 8 s chase without touching you, Forgetful also 3 s out of their sight; rest 8 / 15 s, then calm until they sense you again within 2000 / 1200 u, v3.52); 0 = v3.50: the door rules only. |
| `[Gameplay] EnemyHealthBars` | Off | Options: Interface > Enemy Health. v3.29.0 (was Option 5 > Enemies): Off, Locked Target (your target and monsters hurt in the last 4 s) or All Near (also every monster within 1500 units). |
| `[Gameplay] SupplyDrops` | Off | Options: Gameplay > Items > Supply Drops. v3.29.0 (was Option 5 > Enemies): Off, Normal (about 1 kill in 8) or Generous (about 1 in 4): items left by killed monsters; rare items from the four named ghosts and the stalker Walter. The drop counter is kept in the save. Hidden `SupplyDropsSeed=Fixed` (v3.30.0): v3.29's fixed rolls per kill instead of a new seed each load. |
| `[Gameplay] EnemyRevive` | Off | Options: Gameplay > Enemies > Enemies Return. `Off` / `By Room` / `By World` (v3.56.0; was Off / On: v3.30 - v3.55's `On` reads as `By Room` and is written back once, logged). By Room = a room's killed monsters and copies stand again each time you enter it; By World = they stay dead for the world visit and stand again when you enter the world again. A load is never a room change or a world entry. Hidden: `ReviveEventDogs` 1 (v3.34.0: a story room's released event dogs, e.g. Subway 2:2 after their event, revive awake; 0 = v3.33); `ReturnTremers` 1 (v3.56.0: By World keeps a killed Tremer's body, exe site 0x48ACE5; 0 = the site stays stock and Tremers keep the game's coin flip). |
| `[Gameplay] FadeIn` | Fade + Portal | Options: Gameplay > Enemies > Reveal Method (v3.58.0; was Fade In). v3.50.0 (was Option 5 > Enemies > Fade In): Off, Fade or Fade + Portal (the screen shows Off / Fade In / Portal; also read: Fade In, Portal, On / 1 = Fade + Portal, 0 = Off; an unknown text is read as Fade + Portal and logged). Fade: a monster or victim seen for the first time in a room visit fades in over `FadeInSeconds`, drawn by the game's see-through enemy draw 0x45C880 (no exe patch). Fade + Portal: a ghost arriving after the room's first 1.5 s also gets a portal behind it (v3.50.0 a blood-red vortex; v3.51.0 it tears free of a tar portal: the game's own wall portal look, drips and strands; held 2.5 s, no grab meanwhile). Read again while the game runs. Hidden: `FadeInSeconds` 0.8 (0.2 to 3). |
| `[Gameplay] FadeInPortalDraw` | (absent) Auto | v3.51.0, hidden: Auto = the game's own wall-portal draw when its textures are loaded, else Refined's own tar portal; Refined = always Refined's own; Vortex = v3.50's blood-red vortex and no hold (also read: Game / 1, Own / 2, 0 / Blood). An unknown text reads as Auto (logged). |
| `[Gameplay] FadeInHold` | (absent) 1 | v3.51.0, hidden: 1 = a ghost coming out of a tar portal is held (40 % speed for 1.5 s, full by 2.5 s, no grab / attack until then); 0 = it acts at once (the portal and strands stay). |
| `[Gameplay] FadeInStrandSeconds` | (absent) 3.0 | v3.58.0, hidden: how long (0 - 6 s) Reveal Method's tar strands hold on to an arriving ghost after it is fully in; the portal stays open as long. 0 = v3.57's timeline (the strands snap between 1.5 and 2.5 s, the portal closes from 2.5 s). Not a number: 3.0, logged. |
| `[Gameplay] FadeInPortalPieces` | (absent) 9 | v3.58.0, hidden: how many smaller portals (0 - 9) are laid around the first to make the whole about 1.24 x the arriving body's height. 0 = v3.57's single portal. |
| `[Gameplay] FadeInStrands` | (absent) 16 | v3.58.0, hidden: Refined's sticky tar strands (0 - 24) on an arriving ghost; half as many beside a victim's or Cynthia's own 32. 0 = none of Refined's (Jasper, Andrew and Richard then have no strands). |
| `[Gameplay] AmmoSaver` | Normal | Options: Gameplay > Combat > Ammo Saver. v3.29.0 (was Option 5 > Player): Normal, Saver (every other pistol / revolver shot free) or Endless. Not Playable Eileen's SMG. |
| `[Gameplay] DarkPact` | Off | Options: Gameplay > Combat > Dark Pact. v3.29.0 (was Option 5 > Player): Off, Pact (your hits and the damage you take x1.5) or Blood (x2). Playable Eileen's SMG hits are not scaled. |
| `[Gameplay] GrabBreaker` | Stock | Options: Gameplay > Player > Grab Escape. v3.29.0 (was Option 5 > Player): Stock, Quick (half the hold) or Instant (free after 0.1 s; the grab's first damage stays) against ghost and Sniffer Dog grabs. |
| `[Gameplay] QuickCharge` | Off | Options: Gameplay > Combat > Quick Charge. v3.43.0 (was Option 5 > Player, replaces Tireless): 1.5x / 2x / 3x faster charged attacks (Henry and Playable Eileen). The old `Tireless` key is removed once. |
| `[Gameplay] GhostTeleport` | Off | Options: Gameplay > Powers > Ghost Teleport. v3.43.0 (was Option 5 > Special); v3.44.0: Run + Action together (within 80 ms, either order) -> Richard's 0.5 s fade out, the warp, the fade in; a refused try leaves Action to the game (hidden GhostTeleportRange / Cooldown / Sound); v3.49.0: hold both to keep warping (the next warp as soon as she has faded in); hidden `GhostTeleportCooldown` 0 (was 3; 0 or 0.5-30 s, an old explicit value still applies); never across or into a walk-in event zone the game would still fire, never during a door, a room change, an interaction or a screen fade; v3.50.0: behind the standing enemy she faces (inside `GhostTeleportCone`, from `GhostTeleportRange`), never another one; with none in the cone ahead through walls onto floor a route of the room reaches. |
| `[Gameplay] GhostTeleportRange` | (absent) 5000 | hidden; v3.50.0: 500 .. 12000 u (was 2500, max 6000): how far the faced enemy may be; an old explicit value still applies. |
| `[Gameplay] GhostTeleportCone` | (absent) 35 | v3.50.0, hidden: 10 .. 90 degrees, the cone around her facing; two enemies within 5 degrees: the nearer. |
| `[Gameplay] GhostTeleportThroughWalls` | (absent) 1 | v3.50.0, hidden: 0 = v3.49's warp ahead only (never past a wall). |
| `[Gameplay] GhostTeleportBehindCollision` | (absent) 0 | v3.51.0, hidden: 0 = never warp behind an enemy with collision between you and it (a wall, a prop, a turnstile, a ghost in its wall: the next enemy in the cone in the open, else the warp ahead); 1 = v3.50's rule (also behind an enemy seen through a wall). Read again with the ini. |
| `[Gameplay] GhostTeleportForward` | (absent) 800 | hidden; 0, 300 .. 2000 u: the warp ahead's reach when nothing is in the way (0 = off); v3.50.0: also the reach of the through-wall search. |
| `[Gameplay] GhostTeleportFadeDraw` | (absent) 2 | v3.58.0, hidden: the see-through fade's draw. 2 = Clean: her body and weapon drawn after the actor models on a depth pre-pass with one blend through the game's pixel shader 5 (no dark strips, no seam lines; the call 0x4EF905 hooked at the start); 1 = one pass in place with pixel shader 5 (no strips; her layers still blend); 0 = v3.57 (pixel shader 4: the strips and seams; no filter part fades with her, her shadow always drawn). Anything else reads as 2 (logged once). Read again with the ini (2 takes effect only if it was 2 at the start). |
| `[ScreenEffects] DepthOfField` | Off | Options: Effects > Depth of Field. v3.43.0: Off / Cut-scenes / Always (+ `DofStrength`); v3.44.0: its rows are on the Screen Effects page (was Option 3 > Noise Effect, Enter); Hide Icon is stock again. v3.45.0: `DofStrength` Low (new, gentler) / Medium (default; = v3.44's Low) / High (0 / 1 / 2); a v3.43 / v3.44 value is migrated once (Low / 0 -> Medium, High / 1 -> High; hidden marker `DofStrengthV=345`). v3.47.0: the blur eases in at every strength (quarter-level steps, no edge where it starts). |
| `[ScreenEffects] Bloom` | Off | Options: Effects > Bloom. v3.43.0: Off / Low / High; v3.44.0: + Very High. v3.52.0: Off / Low / Medium / High / Ultra (also 0 - 4), recalibrated; a v3.43 - v3.51 value is migrated once (Low -> Low, High -> Medium, Very High -> High; hidden marker `BloomV=352`, written by Refined). |
| `[ScreenEffects] LensBase` | (absent) 70 | v3.52.0, hidden: the Otherworld Lens' strength in percent while a haunting is registered in the room (10 - 100; v3.51 used 40). Full strength halfway into an aura either way. |
| `[Textures] MenuFont` | Stock | Options: Interface > Menu Font. v3.43.0: Stock / EBGaramond / CormorantGaramond / CrimsonText (the sheets in `fonts\`, OFL); v3.44.0: also the Menu Font row on Option 1 (live switch). |
| `[Gameplay] LifeSteal` | Off | Options: Gameplay > Combat > Life Steal. v3.29.0 (was Option 5 > Player): Off, 5 or 10 (% of your damage to monsters, at most 10 % of max health a hit; not ghosts, Walter or the Wall boss). |
| `[Gameplay] Regen` | Off | Options: Gameplay > Player > Regeneration. v3.47.0 (was Option 5 > Player > Regeneration): Off, x1, x1.25 or x2 (15 / 18.75 / 30 % of max health a minute after RegenDelay s unhurt, up to full; On reads as x1, anything else as Off). Henry, and Playable Eileen while her own regeneration is off; grey Off while hers is on. The first start of v3.47 copies Playable Eileen's `[Defense] Regen` / `RegenRate` (a standard rate; else Off). Applies at once, also when edited while the game runs. |
| `[Gameplay] RegenDelay` | 30 | v3.47.0, no Options row: seconds without healing after any damage, 10 to 120. The first start of v3.47 copies Playable Eileen's `[Defense] RegenDelay`. |
| `[Gameplay] CurseMeter` | Off | Options: Interface > Curse Meter. v3.29.0 (was Option 5 > Player): Off, On (Playable Eileen's own curse when it is loaded, else companion Eileen's) or Companion (always companion Eileen's). |
| `[Randomizer] Items` | Random | Options: Gameplay > Randomizer > Items. v3.48.0 (was Option 5 > Randomizer > Items): Off, Shuffle or Random (or 0, 1, 2): the item and weapon pickups' mode. Read again while the game runs (an edit applies at the next door); an unknown value is read as the default and logged. |
| `[Randomizer] Specials` | On | Options: Gameplay > Randomizer > Specials. v3.56.0 (was `KeepSpecials`, turned round: v3.55's KeepSpecials On -> Specials Off, Off -> On, read once, written as Specials, the old key removed, logged): Off = Holy Candles, Saint Medallions and Swords of Obedience stay where the game puts them; On = they are drawn too. Key items never move. |
| `[Randomizer] Room302` | Random | Options: Gameplay > Randomizer > Room 302 Items. v3.56.0: Off / Shuffle / Random (Off = the game's pickups, Shuffle = Room 302's own pickups swap spots, Random = each of its spots draws an item); v3.55's In / Out are written once in the new word (In = the Items value then, Out = Off; logged). |
| `[Randomizer] Enemies` | Same World | Options: Gameplay > Randomizer > Enemies. v3.56.0 (was `[Randomizer] Monsters`, read once, written as Enemies and the old key removed, one log line; its This Room and This World read as Same World): Off, Same World, All Worlds (or 0, 2, 3; World, All): each monster of a room's script rolls its own type among the monster types and victims' ghosts of this world (or this room's loaded ones), or of every world (their files loaded at the room load). The game's own records of stock monsters (a save's) roll too. Read again while the game runs; an unknown value is read as the default and logged; Seed and the Reroll row are shared with the items. Hidden `MonsterSaveFilter` 1, `MonsterRollAgain` 1 (unchanged). |
| `[Randomizer] EileenOutfit` | Random | Options: Gameplay > Randomizer > Eileen Outfit. v3.49.0, v3.50.0 (was Option 5 > Randomizer > Eileen Outfit): Off / Random (or 0 / 1); Random: companion Eileen wears Dress or Nurse (every outfit, no unlock), a new draw at every room load of her world with Reroll By Room (one per world visit with By World; v3.56.0); the final fight keeps its outfit; an Extra Menu choice holds until the next world visit; Off puts `[EileenOutfit] Outfit` back. With Playable Eileen only the companion changes. |
| `[Randomizer] SpawnSite` | On | Options: Gameplay > Randomizer > Spawn Site. v3.56.0 (was `[Randomizer] Ghosts` Off / Spots, read once, written as SpawnSite, Spots = On, the old key removed): Off / On (or 0 / 1, Spots): every monster and victim ghost of a room starts at a random site of the room (monsters on floor you can walk to, 800 from where you come in, 400 from a door; ghosts anywhere in the room's box, 600 from you and from another ghost); their records in the room move with them, a downed or pinned one stays. |
| `[Randomizer] Reroll` | By Room | Options: Gameplay > Randomizer > Reroll. v3.56.0 (was `PerVisit`: On -> By Room, Off -> By World, read once, written as Reroll, the old key removed, logged): By Room = Random draws new items (and Enemies new types, Spawn Site new sites) each time you enter a room; By World = one draw for each visit of a world (a new one when you enter that world again). Shuffle always swaps a world's items once a world visit. A Quick Load or a loaded save is never a visit: it never rerolls. |
| `[Randomizer] Seed` | 0 | v3.48.0: the seed every choice comes from (1 .. 4294967295): the same seed gives the same items; 0 or missing: a new one is rolled at the next start and written. v3.56.0: shown in small type under the Randomizer page's description (no Seed / New Seed rows any more); set another one here. |
| `[Items] ShineMemos` / `ShineRange` / `ShineThroughWalls` | 1 / 3000 / 0 | v3.3: memos shine too; how far (game units, 500-20000); 1 = also through walls. |
| `[Items] ShineBodies` / `ShineDepthTest` / `ShineEvery` / `ShineFor` | 1 / 0 / 7 / 2 | v3.3.3: characters in front hide the shine (v3.3.5: companions too); v3.3.2's depth-buffer test (experimental, it hid everything); seconds between an item's flashes; how long a flash lasts. |
| `[Textures] Pool` | 0 | v3.4.0: where `[Textures] Pack` replacements are kept. 0 = MANAGED, as before (Direct3D 9 keeps a system-memory copy of each one inside the game's 2 GB address space). 1 = DEFAULT: video memory only, uploaded through a temporary staging texture; after a device reset (resolution / display mode / aspect change, a lost device) each one is filled again from its .dds. Anything that fails falls back to MANAGED. Read at game start. |
| `FrameRate` | 30 | Options: Display > Frame Rate. The gameplay rate, 30 or 60 (v3.31.0: 120 / 180 only with the hidden `[Window] HighFrameRates=1`; a saved 120 / 180 then runs as 60) (was Option 4 > Frame Rate, applied when gameplay resumes). v2.1 FrameMode 1: the DISPLAY rate (in-between pictures, capped at the monitor refresh); the logic stays 30. FrameMode 0: the v2.0 logic-rate engine (front end, menus and v2.1 cutscenes at 30; 168 per-tick constants, animations, fades, springs and the entity gate follow the rate). |
| `FrameMode` | 0 | v3.0 default 0 = true frame rate (the logic runs at FrameRate; tables + hooks of research `framerate/v5/`). 1 = v2.1 frame interpolation (`interp.c`, research `framerate/v3/interpolation.md`): the game runs at its own 30, each frame's D3D8 calls are recorded and replayed with blended vertex-shader constants. |
| `InterpAutoLower` | 1 | v2.1, FrameMode 1: lower the display rate when the game's own ticks stretch (retried after 1-8 min). 0 = never. |
| `InterpCullMargin` / `InterpPacing` / `InterpMatrixRule` / `InterpDiag` | 25 / 1 / 1 / 0 | v2.2, not written to the default ini: view-culling margin in percent (0 = off); 0 = the v2.1 spacing rule; 0 = the v2.1 per-row jump rule; 1 = pacing file + frame dumps, 2 = frame dumps on Ctrl+F12 (`interp_diag.inc`). |
| `SmoothCutscenes` | 0 | v2.1, FrameMode 1: also interpolate real-time cutscenes (phase [0xFD5C20]==1). 0 = cutscenes at 30 (the user's wish). |
| `EntityGate` / `CompanionGate` / `EntitySmooth` | 0 / 0 / 1 | FrameMode 0 only: monster / companion task updates run on 30 Hz ticks (hook 0x553877), drawn at in-between positions (research `framerate/v3/entity_speed.md`). v3.0: the two keys are independent; monsters default to the true rate (v5 audit: their movement is dt-scaled); v3.1: companions default to the true rate too (21 hooks, section "v3.1 COMPANIONS"). The haunting cache 0x5390CE only with EntityGate=1. |
| `RateProbe` | 0 | v3.0, hidden, FrameMode 0: 1 = the loop stays at 30 ticks/s while the engine is told FrameRate (everything rate-aware runs in slow motion; per-tick leftovers at normal speed). A diagnostic. |
| `RateGuard` | (absent) 1 | v3.56.0, hidden, read at start, FrameMode 0 only: 1 = above 60 fps, when the play frames' mean over 3 s is over 1.15 x the frame time in 2 windows in a row, gameplay steps down 180 -> 120 -> 60 (the effect budget sheds first; the guard waits for it only while it can still shed). After 10 minutes of clean play at the lower rate the next room change tries the higher rate again; a rate it stepped down from twice stays off for the rest of the session. 0 = off (v3.55.0: a rate the PC cannot hold runs the game in slow motion). FrameRate itself never changes. |
| `Menus43Log` | 0 | Diagnostic (v1.6): the log names each new full-width 2D draw once, with what the mod did with it. |
| `AspectMenu` | 1 | Aspect Ratio row in Options, six sizes per shape, 3D view from the real size (v1.3). 0 = the stock Options pages: none of Refined's rows or pages; every setting from this ini. |
| `[Options] Look` | (absent) 1 | v3.53.0, hidden, read only: 0 = v3.52's Options look (no page strip, folder triangles, "Parent > Page" titles or the new grey rows); the words of v3.53 stay. |
| `[Options] StockMoves` | (absent) 1 | v3.55.0, hidden, read only: 0 = the game's own rows on their v3.54.0 places (the sound three on Display, the first-person three on Controls, the Subtitles / Film Grain / Shadows switches and Blood Colour on Interface, Renderer on Effects, Menu Font and Start-Up in Interface > Menus) and no patch at 0x525672. |
| `[Options] FolderClick` | (absent) 1 | v3.56.0, hidden, read only: 1 = a mouse click anywhere on a folder row (name, triangle, value column; Brightness / Gamma / Buttons; Back; StockMoves=0's Subtitles combo on its name) opens it; 0 = v3.55.0 (folders open on Enter only). |
| `[Options] ValueBox` | (absent) 1 | v3.58.0, hidden, read at start: 1 = each Options page's values centred in one box, both value arrows at its edges on every row and for every value, the mouse rects with them (the volume bars' arrows too: exe sites 0x5256B6 / 0x5256E2) and the pause Extra Menu > Camera page's ">" at one place; 0 = v3.57.0's arrows (each row its own place, a long value moves the column, Resolution's +20). |
| `FitWrapper` | 1 | GOG's wrapper draws at the game's size in a window; mouse scaled to the window (v1.3). |
| `[EileenOutfit] Enabled` / `Outfit` / `Instant` / `Cutscenes` | 1 / -1 / 1 / 1 | v3.9.0: the companion Eileen's outfit (-1 Default, 0 Dress, 1 Nurse; the pause Extra Menu row writes it), put on at once while she walks with you, and whether cutscene / peephole files follow it. |
| `[SaveBanks] Enabled` / `Bank` | 1 / 1 | Options: Interface > Pause Menu > Save Banks. v3.8.0: Save Banks on / off and the bank the lists open on (1 = sh4.sav, 2-99 = sh4_bankN.sav); Q / E / L1 / R1 and (v3.47.0) the mouse on the Bank row's arrows write `Bank`; v3.48.0, on the Bank row: Left / Right and the D-pad 1 bank, Q / E and the pad's shoulders 10 banks (not the triggers), held keys, buttons and mouse arrows repeat (no new key). |
| `[SaveBanks] ListBar` | Blue | v3.49.0: the save and load lists' highlight bar: Blue (the game's blue menu bar) or Yellow (the game's own list bar); also 1 / 0, any case; anything else = Blue (logged). Read at the start (the `[SaveBanks]` comment: Enabled and Bank apply at once, ListBar at the next start). |
| `[ExtraMenu] Enabled` / `Chest` / `Save` / `Load` / `QuickSave` / `QuickLoad` / `Eileen` | 1 each | Options: Interface > Pause Menu. v3.7.0: the pause Extra Menu (Enabled needs a restart) and its rows (was Option 6). |
| `[ExtraMenu] PauseSound` | (absent) 1 | v3.51.0, hidden: 1 = the level music and the world's sounds stay paused in the Extra Menu's Storage Chest / Save Game / Load Game, as in the pause menu; 0 = v3.50 (the music plays at 20 % in them). Read at the start; only with the default `Seamless=1`. |
| `[ExtraMenu] QuickLoadInPause` | (absent) 1 | v3.52.0, hidden: 1 = the Extra Menu's Quick Load runs inside the pause (the world frozen, the music paused until the loaded room starts); 0 = v3.51 (the pause closes first). Needs `[ExtraMenu] Seamless=1`. |
| `[ExtraMenu] StockShadow` | (absent) 0 | v3.56.0, hidden, read only: 0 = the pause list (Extra Menu, its Camera page, the top level) draws each row once with its letters' own shadow; 1 = v3.55.0 (the game's second shadow copy of every row at +2, +2, which shows as a dark double under bright parts of the picture). |
| `[QuickSave] Enabled` / `SaveKey` / `LoadKey` / `Folder` / `Feedback` | 1 / F5 / F9 / save\quick / 1 | v3.6.0: Quick Save / Quick Load keys (F1-F12 or a Windows key code; not with Ctrl / Alt / Shift), the folder of qs1-3.bin, the on-screen line. |
| `[Saves] WorldTitles` | 1 | v3.5.2: a save made in a world is titled after that world (and its 2nd visit) instead of the last return to Room 302. 0 = the game's own titles. |
| `[Input] PadNames` | Auto | Options: Controls > Buttons. v3.10.0: pad button names in Options > Button Config: Auto (from the connected pad), Xbox, PlayStation, Nintendo or Generic. |
| `[Graphics] VSync` / `AntiAliasing` / `Anisotropic` | (not used) | v3.12.0 - v3.38: the Option 4 rows. v3.39.0: gone; GOG Galaxy > Manage > DirectX Settings (`dxcfg.exe`) sets them in `dxcfg.ini`. A value that is not Default is named once in the log at start. |
| `[Graphics] PerfOverlay` | (absent) 0 | v3.37.0 the Option 4 > Performance row; v3.39.0 a hidden key: 1 = the corner readout (fps, frame ms, 1 % low of the last 5 s, Refined's ms a frame, address space). The log gets a `perf:` summary every 5 minutes whether it shows or not. |
| `[Graphics] EffectBudgetHighRate` | (absent) 1 | v3.56.0, hidden: the effect budget's high frame-rate rules at 120 fps and more (late from 1.06 x the frame, the spark levels first, tested steps when the powers' cost is not measurable). 0 = v3.55's rules at every rate. |
| `[Graphics] EffectBudgetQuick` | (absent) 1 | v3.58.0, hidden: 1 = two or more powers starting within a second at a high frame rate take the effect budget's start step (the deepest level at once, one level back each 0.5 s while frames hold), far-late frames step 0.25 s apart, and the rate guard judges the last 1 s while the budget has nothing left to shed; 0 = v3.56's budget and rate guard (no start step, no quick step, the guard's 3 s windows only). |
| `[Shadows] Edge` | Stock | Options: Effects > Shadows > Edge. v3.40.0 Stock / Soft / Softer; v3.41.0 Stock / Soft (Soft and Softer read as Soft). Option 3 > Realtime Shadow, Enter: the Shadows page with a split-view preview. Hidden `SoftPx` (default 12, the edge in px at 720 lines, scaled with the height). v3.49.0: the text says what it covers: every real-time shadow (the player, companions, enemies, ghosts, Room 302's hauntings, event and cut-scene figures); shadows painted into the room textures are not changed. |
| `[Window] WindowView43` | (absent) 1 | v3.41.0, hidden: Room 302's window views keep the 4:3 view with black sides at wide aspects; 0 = full width (v3.40). |
| `[Window] PixelAlign2D` | (absent) 1 | v3.42.0, hidden: the 2D canvas half a pixel up / left so the first column / row are covered (0 = v3.41). |
| `[Shadows] Darkness` | 85 | Options: Effects > Shadows > Darkness. v3.42.0: 100 / 85 / 70 / 55 (% of the shadow's darkness after Playable Eileen's ShadowDarkness), the Shadows page's Darkness row; v3.49.0: every real-time shadow and the Tremers' wall shadows; v3.52.0: the row runs 25 / 40 / 55 / 70 / 85 / 100 (Left lighter, Right darker); the ini value is unchanged. |
| `[Shadows] TremerShadows` | 1 | v3.49.0: 1 = Shadow Darkness also applies to the Tremers' wall shadows (the alpha of their subtractive ribbon x the Darkness; hook 0x45F4CA). 0 = the game's own. Needs a game restart. A missing key is added with 1. |
| `[Sound] Mode` | (absent: from the save) Stereo | Options: Sound > Sound Mode. v3.52.0 (was Option 1 > Sound): Mono / Stereo / Headphones / Surround (also Monaural, Dolby, Dolby Digital). Missing = taken once from the save's Option 1 Sound (0 Mono, 1 Stereo, 2 Surround); the save keeps the game's own 0 / 1 / 2. |
| `[Sound] Width` | 100 | Options: Sound > Stereo Width. v3.52.0 (the Sound page's Stereo Width): 0 .. 100 %, any value here; the row steps by 10. |
| `[Sound] Reverb` | On | Options: Sound > Room Reverb. v3.52.0 (the Sound page's Room Reverb): On / Off (also 1, 0): the game's own Waves Reverb and area presets. |
| `[Sound] NightMode` | Off | Options: Sound > Night Mode. v3.52.0 (the Sound page's Night Mode): Off / Low / High (also 0, 1, 2): range compression of the volumes the game sets (Low 0.7, High 0.5 around -20 dB). |
| `[Sound] Background` | Mute | Options: Sound > Background. v3.56.0: Mute / Play (also 0, 1). Mute = the game's own: its sounds are silent while another window is in front. Play = the game's sound buffers get DirectSound's global focus (the four one-byte sites 0x4095FE / 0x409B02 / 0x40A14C / 0x40A723): the game stays audible behind other windows; sounds that start after a change follow it. Anything else reads as Mute (logged). A missing key is added with Mute. v3.58.0: Mute mutes the game's sound sessions in Windows (every sound at once, the ones already playing too, the keep-awake tone included; hidden `SessionMute`), and the buffer sites carry the global focus in both modes while that works. |
| `[Sound] KeepAwake` | (absent) 1 | v3.56.0, hidden: 1 = an inaudible keep-awake signal of Refined's own (a looping 45 Hz tone, global focus) plays under the game's sound for the whole session, so the sound device and a wireless headset never get a silent stream (no lost seconds after Alt+Tab). 0 = none. |
| `[Sound] KeepAwakeLevel` | (absent) 8 | v3.56.0, hidden: the keep-awake tone's amplitude, 0 .. 64 of 32767 (8 = -72 dBFS, 64 = -54 dBFS; 0 = digital silence: keeps the stream open only). Raise it if a headset still falls asleep; anything else reads as 8 (logged). |
| `[Sound] DeviceWatch` | (absent) 1 | v3.56.0, hidden: 1 = log the device the game plays on and every change of Windows' default sound device (and show "New sound device: restart the game to use it." on Sound Mode's line while the default is another device). 0 = off. |
| `[Sound] ZeroSilent` | (absent) 1 | v3.58.0, hidden: 1 = Volume 0 is silent (the volume hooks give DirectSound -10000 for the game's own 0, plus the exe site 0x40B2E7 for Room 302's fading stream 0x2774); 0 = the game's own (-60 dB; -40 dB with Night Mode High). |
| `[Sound] SessionMute` | (absent) 1 | v3.58.0, hidden: 1 = Background Mute by Windows' sound sessions of the game (every output device; only Refined's own mutes undone; the marker file `SH4Refined.muted` lets the next start unmute a mute left by an exit behind other windows); 0 = v3.56's Mute (the buffers' focus flag, sounds that start from then on). |
| `[Sound] Enabled` | (absent) 1 | v3.52.0, hidden: 0 = the game's own sound and Sound row (no Refined sound site). |
| `[Sound] PanLaw` | (absent) Smooth | v3.52.0, hidden: Stock = the game's own steep pan (Width and the Headphones floor do nothing). Read at the start. |
| `[Sound] HeadphonesFloor` | (absent) 9 | v3.52.0, hidden: 6 .. 12 dB, how far down the other ear may go in Headphones. Read at the start. |
| `[Sound] MonoFold` | (absent) 1 | v3.52.0, hidden: 0 = the game's own Monaural (stereo streams keep their left channel, the music stays stereo). Read at the start. |
| `[Window] EdgeCoverLog` | (absent) 1 | v3.41.0, hidden: in Borderless, log (once a second at most, class names only) any window above the game that covers a thin strip of its edges. |
| `[Window] Opening` | Skip Logos | Options: Interface > Start-Up. v3.39.0 (was Option 4 > Opening, the next start): Full (the game's own logos, notice, first-start movie and title intro), Skip Logos (straight to the title) or Fast (also no first-start movie, the short title intro). Also read: Stock / 0, Skip / 1, Straight to Menu / 2. |
| `[Room302] LookDownLimit` | 85 (first start: Playable Eileen's) | Options: Controls > Room 302 > Look Down. v3.50.0 (was Option 4 > Room 302 > Look Down): 0 (the game's 50), 60, 65, 70, 75, 80, 85 degrees; Henry and Eileen alike. While Playable Eileen keeps Room 302 (up to v1.24.4) the rows are grey and these wait. |
| `[Room302] LookUpLimit` | 85 (first start: Playable Eileen's) | Options: Controls > Room 302 > Look Up. v3.50.0 (Look Up): 0 (the game's 25), 35, 45, 60, 70, 80, 85 degrees. |
| `[Room302] LookFeel` | 1 (first start: Playable Eileen's Feel) | Options: Controls > Room 302 > Look Smoothing. v3.50.0 (Look Feel): 0 (Off, the game's own look), 0.5, 1, 1.5, 2: the turns and tilts ease in and out. |
| `[Room302] WalkSpeed` | 0.65 (first start: Playable Eileen's) | Options: Controls > Room 302 > Walk Speed. v3.50.0 (Walk Speed): 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.9, 1 (the game's), 1.1, 1.2, times the game's (and Movement Speed). A value off the list is read as the nearest one (logged once). |
| `[Subtitles] Size` | Normal | Options: Interface > Subtitles > Size. v3.13.0: subtitle size: Small, Normal (the game's), Large or Huge (long lines fitted). was Option 3 > Subtitles (Enter). |
| `[Subtitles] Colour` | White | Options: Interface > Subtitles > Colour. v3.19.0: White (the game's), Yellow, Cyan, Green, Red, Pink, Orange, Amber or Ice Blue (an old Grey reads as Red). |
| `[Subtitles] Effect` / `Exit` | None / None | Options: Interface > Subtitles > Appear / Vanish. Effect None, Typewriter (with a cursor), Fade In (the whole line; fades out too when Exit is None), Wave, Glitch (blocks with colour fringes), Blood Drip, Embers (v3.21.0: Tremble reads as None, Flicker as Blood Drip); Exit None, Fade Out, Burn Out, Paper Peel (was Option 3 > Subtitles). |
| `[Subtitles] Edge` | Shadow | Options: Interface > Subtitles > Edge. v3.13.0: Shadow (the game's), None, Outline or Outline+Shadow. |
| `[Subtitles] Box` | None | Options: Interface > Subtitles > Box. v3.13.0: None (the game's), Translucent or Solid dark box behind the subtitle. |
| `[ScreenEffects] NoiseStrength` | Normal | Options: Effects > Film Grain > Strength. v3.15.0: the game's grain / dust: Low (x0.5), Normal (the game's), High (x1.5) or Very High (x2; the screen shows Ultra, v3.58.0; Ultra typed by hand reads as Very High). was Option 3 > Noise Effect (Enter). |
| `[ScreenEffects] NoiseType` | Stock | Options: Effects > Film Grain > Grain Type. v3.18.0: Stock (the game's), Fine, Coarse, Film (grain and dust), VHS (line noise), Scan Roll (a band rolling over the screen now and then) or Rain (drops on the lens). v3.16.0's Colour reads as Fine + Noise Colour Full Colour. |
| `[ScreenEffects] NoiseColour` | Stock | Options: Effects > Film Grain > Grain Colour. v3.18.0: Stock (each type's own), Mono, Full Colour, Red, Green, Blue, Amber or Sepia. |
| `[ScreenEffects] PreviewImage` | preview.dds | v3.18.0: your own picture behind the Screen Effects page (DDS DXT1/3/5 or 32-bit), relative to the game folder. v3.58.0: a `PreviewImage` that names the stock picture `SH4Refined_preview_stock.dds` is never written by F7 or the first-pause capture (logged once). |
| `[ScreenEffects] PreviewCapture` | 1 | v3.18.0: 1 = the first gameplay pause saves that clean picture as PreviewImage when the file does not exist; 0 = off. |
| `[ScreenEffects] Filter` | None | Options: Effects > Filter. None, CRT, Vivid, Sepia, Old Film, Night Vision, Night Vision Mono, Thermal, Spirit Vision (v3.17.0; `Ghost Vision` still reads as it) or Vignette. v3.18.0: Noir removed (reads as Night Vision Mono); v3.15.0's High Contrast / Otherworld read as None. |
| `[ScreenEffects] FilterStrength` | High | Options: Effects > Filter Strength. v3.58.0: Low, Medium, High, Ultra (also 0 .. 3): Low / Medium lay the plain picture back over the filter (35 / 65 % of its change; the hot bodies and ghosts stay full), High = the filters' look of v3.16 - v3.57, Ultra = each look's own amounts x 1.35 (Thermal and Spirit Vision as High). Grey with Filter None. Read only with the marker `FilterStrengthV=358` beside it: v3.15.0 wrote this key too (Low / Medium / High) and v3.16.0 stopped reading it, so an old value without the marker is read once as High and rewritten with the marker (logged: `FilterStrength=Medium is v3.15's ...: now High`). |
| `[ScreenEffects] FilterStrengthV` | 358 | v3.58.0, hidden, written: the marker above (do not edit). |
| `[ScreenEffects] ThermalGhost` | (empty) | v3.16.0: more enemy types (hex, e.g. `08` Twin Victim) that Thermal hides and Ghost Vision shows. Ghosts 0B-0F are included already. Read at start. |
| `[ScreenEffects] ThermalNotGhost` | (empty) | v3.16.0: ghost types (0B-0F) treated as warm instead. Read at start. |
| `[ScreenEffects] ThermalCoolSeconds` | 30 | v3.21.0: seconds a dead enemy takes to cool in Thermal (0 = at once, up to 600). Read at start. |
| `[ScreenEffects] SoulFadeSeconds` | 6 | v3.21.0: seconds a dead enemy's soul fire takes to die down in Spirit Vision (0 = at once, up to 600). Read at start. v3.28.0: default 10 -> 6 for the new going-out sequence (an existing ini keeps its value; the v3.29.0 install set the user's to 6). |
| `[ScreenEffects] ThermalCold` | (empty) | v3.16.0: enemy types Thermal draws without the heat boost (e.g. `03` Hummer). Read at start. |
| `[ScreenEffects] Cutscenes` | On | Options: Effects > Cut-scene Filter. v3.22.0 (Screen Effects page row Cut-scenes): On = the filter also runs in real-time cut-scenes; Off = only in play (our Noise Type / Colour layer too; movies are never filtered). |
| `[ScreenEffects] ThermalHeat` | 1 | v3.22.0: 1 = Thermal draws the world cold (navy - blue - cyan) and redraws living bodies in heat colours; 0 = the v3.21 brightness-based look (ambient boost). Read at start. |
| `[ScreenEffects] SpiritGhost` | Blood | v3.26.0: Spirit Vision ghosts. Blood = a pale core bleeding into crimson with a crimson aura and blood at the vein roots; White = the v3.23.0 white glow (no aura or blood). Read at start. |
| `[ScreenEffects] SoulNone` | 06 | v3.22.0: enemy types (hex) without a soul fire in Spirit Vision (06 = Wheelchair); the ghosts 0B-0F never have one. Read at start. |
| `[ScreenEffects] PreviewKey` | F7 | v3.22.0: a key (F1-F12 or a virtual-key number) that saves the current clean gameplay frame as PreviewImage, the picture behind the Screen Effects page, replacing it (the stock save sound confirms it). None = off. v3.39.0: F7 is the default (was None) for an ini without the key; a `PreviewKey=None` already in the ini stays None (write F7 there to use the key). F7: no key of Refined, Playable Eileen or the cheat table; F8 sits next to Quick Load's F9. The Filter row's description names the key (v3.56.0: only with `PreviewScene=Picture`; v3.58.0: only where F7 changes what the page shows: Preview Live, outside a pause, no stock picture). Since v3.58.0 the page shows the F7 picture with Preview Live on the title menu when there is no stock picture `SH4Refined_preview_stock.dds`; F7 never writes the stock picture. |
| `[ScreenEffects] Preview` | Live | Options: Effects > Preview. v3.58.0: Off, Stock, Live (also 0 / 1 / 2): what the Effects pages (Effects, Film Grain, Depth of Field, Shadows) show behind them. Off = nothing (the game's own menu background; no split preview on the Shadows page); Stock = the stock picture `SH4Refined_preview_stock.dds` (a file in the game folder, never in the dll, never written by F7), also in a pause; Live = the stock picture on the title menu, the paused scene with its heat and spirit layers in a pause. Without the stock file: Live shows PreviewImage on the title menu, else SH4 Refined's own scene; Stock the scene (one log line). |
| `[ScreenEffects] PreviewScene` | - | Removed in v3.58.0: an ini with `PreviewScene` (Scene or Picture) and no `Preview` gets `Preview=Live` once and the old key is removed (logged: `PreviewScene=... is now Preview=Live`). v3.56.0 - v3.57.0: what the Effects page shows with no paused game (the title menu): Scene = SH4 Refined's own room with a figure and a ghost, made for every filter (Thermal's heat and footprints, Spirit Vision's ghost glow and soul fire); Picture = PreviewImage first (as before v3.56.0), then the scene. While it is Scene the Filter row's description no longer names the PreviewKey (F7 still saves PreviewImage: Picture and the Shadows page use it). |
| `[ScreenEffects] PreviewLayers` | 1 | v3.56.0: 1 = every pause keeps Thermal's hot bodies and Spirit Vision's ghosts, soul fires and haunting auras for the Effects page opened from it (two render targets of the screen's size, 28 MB at 2560x1440, kept between pauses like the Shadows page's); 0 = off (the page shows their palette / edge world only, as before v3.56.0). |
| `[ScreenEffects] ThermalDepthBias` | (absent) 0.3 | v3.24.0, hidden key: how much nearer (per mille, 0..1; 0 = off) Thermal's heat colours are drawn, so they never lose a near-tie with the body's own surface (cold / hot flicker). Higher values may let a body show through a surface right in front of it. v3.29.0: also applied to c12, the clip-z row of the skinned shader family (bodies; c9 is the rigid family's), and values up to 10 are accepted as a logged test value. |
| `[ScreenEffects] ThermalRedrawDepth` | (absent) 1 | v3.29.0, hidden test key: 0 = Thermal's heat redraw ignores the depth buffer (bodies show through walls); settles whether anything but the stock draw wins the depth test over a body. |
| `[ScreenEffects] DeathPresenceDepth` | (absent) 1 | v3.27.3, hidden: 1 = Death Presence's depth is the one the game stores (z = P22 + P32 / w); 0 = v3.27.2's x 1/128 (for comparison). |
| `[ScreenEffects] DeathPresenceProbe` | (absent) 0 | v3.27.2, hidden diagnostic: 1 = nine floor squares at depth scales x 0.25 .. x 4; 2 = a depth-contour map (12 full-screen quads at w = 100 x 1.35^k); 3 = the contours with x 1/128. |
| `[Input] Vibration` | Low | Options: Controls > Vibration. v3.14.0: rumble strength when Option 2 > Vibration is on: Low (the game's own), Medium or High. Scales GOG's controller wrapper's motor command (DualSense / DualShock 4 / Xbox over HID; capped at 100 when an Xbox pad is driven over HID). |
| `[Input] PadGlyphs` | 1 | v3.11.0: 1 = Button Config shows pad buttons as glyphs (Kenney Input Prompts, CC0) for Xbox, PlayStation and Nintendo pads; 0 = the v3.10.0 names. Generic pads, numbered buttons and PlayStation's Create / Share / Options keep names. |
| `[Input] ControllerCrashFix` | 1 | Controller-connect crash fix for GOG's dinput8.dll. |
| `[Input] OmniMove` | Off | Options: Controls > Analog Movement. v3.46.0, v3.47.0 on Option 2 (Control), v3.48.0 Option 2 (Control) > Omni-Directional Movement: Off (the game's 8 directions) or On (any angle, the speed follows the push, 0.6-1; Playable Eileen v1.24.0 and later follows it too); also 0 / 1. v3.46 / v3.47's Direction, Analog and 2 read as On (written back once as On); anything else reads as Off. Applies at once, also when edited while the game runs. Gameplay only; the keyboard stays 8-way; needs movement on the left stick (or the sticks swapped). |
| `[Input] OmniDeadZone` | 0.30 | v3.46.0, no Options row: Omni-Directional Movement's round dead zone, 0.05-0.90 of a full push (0.30 = the game's own +-300); inside it the game's own input stays (above 0.30 a push the game would still read is stopped). Read again while the game runs. |
| `[Input] OmniLook` | Room302 | v3.49.0, no Options row: the right stick's look while Omni-Directional Movement is On: `Room302` (Room 302's first-person view turns with the stick's push, any angle), `All` (every stage), `Off` (the game's 8-way look); also 0 / 1 / 2. Needs the look on the right stick (or the sticks swapped). Applies at once (also edited while the game runs). |

`SH4WindowMod.log` (previous run: `SH4WindowMod.prev.log`) says what was applied.

## For other tools

The export `SH4WM_Patches` lists every range of the game image this mod changed: `{ u32 magic 'SHWM' (0x4D574853),
u32 version 1, u32 count, { u32 va, u32 len } item[512] }` (512 since v2.0, 64 before; the exe fixes, hooks, the frame-rate table's sites and the exe's patched import slots), the same
layout as Playable Eileen's `SH4PE_Patches`. The cheat table's `restoreGameCode()` leaves those bytes alone.

v3.47.0: `float __cdecl SH4R_StickAmount(void)` (ordinal 199, by name) = Omni-Directional Movement's push this frame
(v3.47.0's Stick Movement), for Playable Eileen: 0 = Off or it cannot act (behave as before), 1 = On in a frame whose
stick vector is not Refined's (the self-check pending, the dead zone, menus, pauses, cut-scenes, the keyboard), 0.6..1 =
the push written this frame. v3.48.0 keeps the meaning (On = v3.47.0's Analog; Direction, which always gave 1, is gone).
Set once per InputPollPad on the game thread (before the game's logic of that frame); any thread may read it. Playable
Eileen can answer with its export `int __cdecl SH4PE_StickAmountOn(void)` (1 = she scales her walk / run by it).
Contract: `staging/v347_feedback/w1_control/CONTRACT_analog_pe.md`.

v3.50.0: `SH4R_Room302` (ordinal 200, DATA, by name): the Room 302 hand-over block Playable Eileen reads (56 bytes,
R3Export in `src/r302_sim.h`: which mod owns Room 302's look and walk, the values); Playable Eileen answers with its
export `SH4PE_Room302Yield` (from PE v1.25.0). Contract: `staging/v350_feedback/w6_room302/R302_CONTRACT.md`.

## Build

`build.cmd` (Visual Studio 2022, x86, static CRT) -> `out\winmm.dll`. `src\winmm.def` and `src\winmm_exports.inc` are
generated from the real `SysWOW64\winmm.dll` by `tools\gen_exports.py` (193 exports at their own ordinals, one of them
NONAME; no data or forwarded exports); `tools\check_exports.py` compares the built dll with the real one.
`test\run_tests.py` builds two test programs and runs the offline checks (forwarding, window behaviour, settings
import, coexistence with the plugin in both load orders); results in `test\results.txt`.

## Ultrawide notes (v1.6)

- The 3D view keeps the original vertical view and gets wider sideways (Hor+). The game's fixed and event cameras were
  framed for 4:3, so at 21:9 and especially 32:9 you can see past the edges of some sets (unbuilt geometry, characters
  waiting off-screen). Menus, text and the HUD stay 4:3 in the middle.
- A window cannot be larger than the monitor: on a 16:9 monitor a 21:9 or 32:9 window is fitted to the monitor's width
  (smaller, same shape); Borderless shows the picture letterboxed.
- A save made at 21:9 / 32:9 also loads in v1.3 .. v1.5 and in the stock game (they read the shape as 4:3 / 16:9).

## History

- v1.0 (2026-10-26): split out of Playable Eileen; windowed mode, resizing, Alt+Tab, mouse capture, controller fix.
- v1.2: 4:3 sizes for the wide Options entries. v1.3: Aspect Ratio row, Hor+ 3D, window takes the picture's shape,
  FitWrapper. v1.4: 4:3 menus in wide shapes (Menus43). v1.4.1: export `SH4WM_Scale2D`, a save load keeps size and
  shape. v1.5: Display Mode row (Windowed / Borderless), Alt+Enter. Old sources in `history\`.
- v1.6 (2026-11-01): full-screen darkening and black fills full width in wide shapes (item examine screen first),
  `Menus43Log`, the 21:9 and 32:9 shapes; the Resolution arrow moves by the label's length.
- v1.7 (2026-11-01): Monitor row and `Monitor` / `MonitorName` / `MonitorPath`; the Display Mode row's right arrow
  no longer moves between Windowed and Borderless.
- v1.8 (2026-11-02): black sides beside full-screen pictures in wide shapes (memo reader, Scrap Book and the like),
  `MenuPillars`.
- v1.9 (2026-11-03, laptop rig): Field of View row (`FieldOfView`, hook on the FOV setter 0x4F70A0) and an
  experimental Frame Rate row (`FrameRate`: engine rate [0x5D5248] at 0x403169, main-loop step store 0x414F0E,
  animation rate 0x414E09 / 0x418C9C). Audit fixes (`research\audit_2026-11-03\window_mod_audit.md`): a failed device
  reset is retried quietly and falls back to the last size that worked after 10 tries (INT-1); the GOG wrapper probe runs
  under SEH (INT-5); `GetCurrentThreadStackLimits` is looked up at run time, so Windows 7 can load the dll (CMP-5);
  new installs default to an automatic window size (CMP-4).
- v2.0 (2026-11-03, laptop rig, overnight): Field of View in degrees (`FOV`, applies at once, also in Room 302, whose
  camera sets its angle only when it starts). Frame rate engine: a governor at the frame end (0x415202) runs the front
  end and in-game menus (mode [0x1236D4C] != 6, overlay sub-mode, game phase [0xFD5C20] == 2) at 30 and gameplay at the
  chosen rate, live; the animation advance (0x4184DC / 0x4192B5 / 0x41848E) is scaled by 30 / rate with the tick
  counter spreading the remainder; `fps_tail_table.h` (generated by `tools/gen_tail.py` from
  `research/framerate/v2/fps_tail.json`) rewrites 163 per-tick constants + one magic division at every switch; hooks
  B1 (fade speed floor 0x565CB0) and B2 (spring helper 0x4216D0); key repeat 0x4F85A0. Alt+Enter inside Options also
  updates the Options backup. `SH4WM_Patches` holds 512 items. Reviews: `research/framerate/v2/review_wm20.md`.
- v2.1 (2026-11-04, laptop rig): renamed SH4 Refined. Frame interpolation (FrameMode=1, default; `interp.c` +
  clean-room `interp_pair.inc`, `cleanroom/SPEC.md`; method from sh4vita, see THIRD-PARTY-NOTICES). FrameMode 0
  extras: cutscenes (phase 1) at 30, phase-setter hooks 0x4EFB70/0x4EFB1E/0x4EFB69 (chest fade), 5 new tail rows
  (eye/hand pulse 0x4F4D4D, its fade, movie fade 0x412C3B), the entity tick gate (0x553877, haunting 0x5390CE).
  Save / title-Load screens get black sides (sprite return 0x4CCD2A, flush return 0x570584). Research and reviews:
  `research/framerate/v3/` (web_research, entity_speed, menus_cutscenes_poi, save_menu_pillarbox, interpolation,
  review_v21). Tests: `test/` (interp, 48 checks) and `cleanroom/` (pairing, 128 checks).
- v2.1.1 (2026-11-04, laptop rig): files named SH4Refined.ini / SH4Refined.log / SH4Refined.prev.log; the old
  SH4WindowMod.* ones are moved on the first start (`file_names.inc`; the old log becomes the previous log when it is
  the newer one). Live: the laptop's ini moved with every value (FrameRate 60, FOV 120). Nothing else changed.
- v2.2 (2026-11-04 night, laptop rig; research `research/framerate/v4/` culling.md + live_2026-11-04.md):
  even pacing (the rest of the tick split by k - 1, not k: v2.1 bunched the two pictures after every hitch while its
  phase decayed; measured 16.67 / 16.66 ms, every picture in its own refresh), matrix jump rule (`ClassifyMatrix`:
  3- and 4-register uploads judged as a whole; v2.1 kept items and bones at tick T during camera turns = sliding),
  view-culling margin (0x42814B model outcode, 0x422B40 map cull matrix; 25 % / 12 %), diagnostics
  (`interp_diag.inc`). Hidden keys: InterpPacing=0, InterpMatrixRule=0, InterpCullMargin=<percent>, InterpDiag=1|2.
  Tests: test_interp 48, test_matrix 12, test_names 24, clean-room test_pair 128.
- v3.0 (2026-11-05, laptop rig; research `research/framerate/v5/`: web_true60, true60_monsters, true60_misc,
  true60_companions, live_2026-11-05): **true frame rate** is the default (FrameMode=0). The per-tick table
  (`fps_tail_table.h`, from `tools/gen_true60.py` + `tools/gen_tail.py`) grows to 352 rows (+184: monsters 24, HUD /
  lens 3, companions 120, events 37; i16 operands, 16-byte originals); `true60_fixed.h`: 22 player clip-frame ->
  seconds calls redirected from 0x55AE60 (1/fps) to a constant 1/30 (hit, combo, item-use windows opened N/30 late)
  and 4 `fps == 30 ? 28 : ...` branches forced to the 30 path; hooks: ghost hover damping 0x483DAC (damp^(30/N)),
  health gauge pulse 0x50E3D0, Holy Candle flame 0x53FCDA, look-icon echoes 0x4F4EF8 (30 Hz steps via g_gateTick).
  EntityGate (monsters, default 0) and CompanionGate (companions, 1) are independent; hkHaunt only with EntityGate.
  Hidden `RateProbe`. Live: logic 60.0 ticks/s, 59 position updates a second, pause menu 30 <-> 60, 352/352 rows and
  every hook installed. Walk 5.6 % / run 2.9 % slow at 60 come from Playable Eileen's forced walk amount (+ one
  1.5*dt ramp step), a PE-side fix. Tests: test_interp 48, test_matrix 12, test_names 24.
- v3.1 (2026-11-05, laptop rig): **companions at the true rate** (CompanionGate default 0). Section "v3.1 COMPANIONS"
  (`CompanionInstall`, after True60Install): 21 hooks from `research/framerate/v5/true60_companions.md` section 2:
  H1 walk integrator loads x g_cs (0x441B24 / 0x441B56 / 0x441631), H2 heading step wrapper on 6 calls of 0x421B30
  (0x4ECE57 with g_ck2 = 2(1 - 0.5^s), its step also halves per tick), H4 haunting damage to Eileen x g_cs before
  0x43EB70 (0x43FBF8; PE's entry hook still runs), H5 GateDec: 11 integer timers subtract g_compTick instead of 1
  (0x446900, 0x446D48, 0x446E7B, 0x447020, 0x44957A, 0x449624, 0x44CC9A, 0x44CD27, 0x44D085, 0x44D133, 0x44D147).
  The audit's 22nd site (0x44AF6A) is the alternative to the table rows 0x4470FD / 0x447201 and is not used.
  g_cs / g_ck2 are set by TailWrite / TailApply (`CompanionRate`): 30/N with the N table, 1 at 30, at rates not in
  the table and inside a gated update; g_compTick = g_gateTick with the N table, else 1 (GateAdvance). Every site is
  checked against the GOG v7 bytes (no branch target inside any of them). Live: 21/21 installed, hook code and
  return pointers verified in the running game; at 60 in gameplay g_cs 0.5, g_ck2 0.5858. Companion movement not
  measured yet (no laptop save with a companion). Playable Eileen v1.03.3 at 60: walk 175.5, run 663.0 u/s (= 30).
- v3.2 (2026-11-05, laptop rig): two fixes from the user's 180 play-test, section "v3.2 WEAPON ROPES + CUTSCENE
  STREAM" (`RopeStreamInstall`, after CompanionInstall). (1) Eileen's weapon rope step 0x443EB0 (purse = type 1 / item 0x80, 0x4443B0, 3 nodes, damp 0.95, sag 5; chain = type 4 / 0x84,
  0x4448B0, 8 nodes, damp 0.9, sag 4; both set up by 0x443DF0): Verlet `x' = x + d (x - x_prev) + g (0,1,0)` per
  30 Hz tick with no table row, so at N the rope fell (N/30)^2 as hard (36x at 180) and lost its swing N/30 as fast
  (user report: "gravity constantly pulling them to the ground"). Hooks 0x443EF9 (damp [ctx+2A4h] -> d^g_cs, cached
  powf) and 0x443F3D (sag [node+34h] x g_cs2 = g_cs^2). The riding crop
  (type 3, 0x444A00) already had table rows. (2) The cutscene stream clock: 0x560A6E adds [0x12E7C50] per tick to the demo stream time [demo+E0h]; its only writer
  0x5600B0 sets it to 30 / fps once, from 0x4F1D00 (scene load, 40+ callers), one tick BEFORE 0x4CCBF0 enters
  phase 1, where the governor runs 30. Loaded at N, played at 30: characters at 30/N speed while the camera kept its
  schedule and swung to the next character on time (user report at 180). Hook 0x560A6E: 30 / [0x5D5248] each tick,
  rounded to a float like the stock variable (the stock value at any steady rate; also right for the phase-0
  in-world scenes and across a rate change). The rest of 0x4F1D00's call tree reads no rate (scanned 4 deep). g_cs2 is set with g_cs in CompanionRate.
  Sites checked against GOG v7 bytes, no branch target inside, no Playable Eileen patch on them.
- v3.22.0 (2026-11-16, main rig; installed md5 a0b809fc; seen in game 2026-11-16: Thermal heat, pause / ring / Item Shine filters, gameplay subtitles, candles / medallion / soul fires confirmed; regressions and requests in `staging/feedback_v323/`): **The user's 17-item v3.21 test list.** Thermal v2: the old "heat" was a brighter model ambient mapped by brightness (lamps hot, dark enemies cold); now the world palette is cold-only and every living body is redrawn after it through the ghost-glow draw-call override in a heat mode (Z test relaxed to -EQUAL, no Z write, a ps.1.1 heat shader, per-body heat for the death cool-down), footprints drawn warm after the palette, the trapped Cynthia heated also in phase 1 with a first-entry gate log (`ThermalHeat=0` = the old look). Overlays: the scene filter now also runs in the capture frame, so the game's frozen picture (pause, quit prompt, item ring, map, Options fade) holds the filter with all its 3D layers; overlays over it add nothing (menus clean, no Options flash); a filter changed in Options during a pause re-filters the clean pause picture at frame start (0x55AE31 / 0x55AE44). The ring description's dim (0x50EE30) is limited to the description box (DimDraw). Item Shine is drawn before the filter. New Screen Effects row Cut-scenes (On / Off). Options: submenu rows bright blue / deep blue under the cursor instead of arrows (label call 0x525553); on the Screen Effects page dark panels and a black text outline (shadow-string sites 0x523A3A / 0x52418D / 0x5259C3, glyph-budget checked). Subtitle effects run on gameplay dialogue (the event poll re-starts the same line every tick; L4 continues it; exits wait two empty frames). Spirit Vision: haunting veins (src/svveins.inc: tendrils anchored by collision ray casts, sucker blots, pulses, snap / re-grow, latching coils on the player's bones, medallion let-go); candles R 400 with a slow pulse plus a fast thin ring; the medallion at the chest bone whenever worn; warm soul fires, none for SoulNone types (06), the player white-gold, Walter large red. The Screen Effects page shows the paused scene when opened from a pause; `PreviewKey` saves a clean frame as the picture. Review: plan approved by Codex (round 5); code inspections 2 of 2 used, the second left three item-9 findings open (4 latch strands, re-anchor when a wall blocks a strand, art grade 6 / 6). Plan / reviews / renders: `staging/feedback_v322/`.
- v3.23.0 (2026-11-16, main rig; md5 6d5c2eae; installed 2026-11-16 with the game closed, not yet seen in game): **The user's 18-item v3.22.0 test list.** Screen Effects page (items 1 / 4 / 5): the game's text queue holds 800 glyphs a frame (5570B0 drops the rest silently) and v3.22's outline queued every string 10 times, so the Filter value, Cut-scenes, Back and the description fell off; the outline is now drawn in the glyph pass (the shadow quad hook D1 0x559B64) as extra quads, never queued, under one shared vertex reserve (`SubSpare`: every extra copy, menu or subtitle, leaves 12 vertices for each glyph still to come). Blue submenu rows keep their blue during the cursor's arrival flash (5239A0's flash path redrawn in blue, item 6). Thermal (items 7 / 8): the heat redraw ran in the 2D scene slot with Z off; now Z test on (reversed Z, GREATEREQUAL), no Z write, living bodies unblended (solid); the Z states found are logged once. Old Film without the frame weave (item 10). Spirit Vision: ghosts glow white (item 3); the player's soul fire scales with current / max HP (item 16); veins root on the ghost's own bones (0x52AEB5 model, `[model+0xC0]` matrices; no bones this frame = no tendrils, item 15); a downed ghost's veins retract and regrow over the last 2.5 s of a running get-up countdown (class 9), none while pinned (item 17, the user's choice); the vein art gets a crimson additive halo under a dark body, brighter rim / line, more pulses (item 2), up to 4 latched strands (V322-R1), anchors re-checked for line of sight and re-anchored behind a new wall (V322-R2). Review: plan approved by Codex in round 5 of 5 (6 findings, all accepted); code inspections 2 of 2: the first found one halo-reserve issue (fixed), the second APPROVED with the vein art graded 7 / 7 (v3.22: 6 / 6), closing v3.22's open V322-R1 / R2 / R3. Rollback: `G:\SH4 Mod Archive\2026-11-14\Mod Backups\winmm.dll.v3.22.0.before_v3.23.0` (a0b809fc). Plan / reviews / renders: `staging/feedback_v323/`.
- v3.24.0 (2026-11-16, main rig; md5 8d2ee44f; installed 2026-11-16 with the game closed; confirmed in game by the user 2026-11-16): **The user's v3.23.0 feedback** (the Screen Effects page, the vein auras and Old Film confirmed). Thermal: living bodies are redrawn with Z write on and before the cooling ones, so the player stays solid when the game's close-camera fade leaves no depth for her (item 1); a small depth bias for the heat redraw (the view-projection's z row c9 x 1.0003 during the redraw, restored after; `ThermalDepthBias`) against the cold / hot flicker reported after a quick save / load (item 4), with a log line if two bodies are drawn at the same spot. Spirit Vision: a white-gold soul fire for people: escort Eileen and Cynthia as companions, the trapped Cynthia (0x4EE376) and other living non-enemy models (items 2 / 3); the Room 302 peephole Eileen through a new hook on the scene-character draw 0x52AF34 (char id 0x133 / 0x12E while the peeping flag [0xFD5BE8] is set), souls only in that view. Review: plan approved by Codex in round 3 (one BLOCKED round for missing evidence, answered with `re_evidence.md`; 2 findings accepted); code inspections 2 of 2: the first asked for the bias tolerance to be stated and bounded (now in the criterion, key capped at 1 per mille), the second APPROVED. Rollback: `G:\SH4 Mod Archive\2026-11-14\Mod Backups\winmm.dll.v3.23.0.before_v3.24.0` (6d5c2eae). Plan / reviews / evidence: `staging/feedback_v324/`.
- v3.26.0 (2026-11-16, main rig; md5 2da13fb5; installed 2026-11-16 with the game closed; user-verified in game 2026-11-16: ghosts and rain "look good"): **Special mods round 1 of 5.** Spirit Vision ghosts are blood-rooted (`[ScreenEffects] SpiritGhost`, Blood default / White): a ps.1.1 ramp in the ghost redraw from a deep crimson (dark texels) to a pale rose white (bright texels), a crimson aura of soft sprites on the bones of the 8 nearest ghosts (limb ends brighter), and at every vein root a wound that seeps, drips down the body and drops blood that falls and leaves fading floor blots (one ray cast per drop from the veins' remaining budget; a pause-aware blood clock by frame continuity). Noise Type Rain is rain on glass (src/rain_sim.h + src/rain.inc): a fixed 120 Hz simulation on integer QPC ticks (bit-identical at any frame rate, native test test/rain_sim_test.c), drops that refract a checked copy of the frame (inverted, minified), rims / glints / caustics in the 2x-modulate layer, an additive glint for dark rooms, stick-slip sliders at random angles that merge and leave thinning trails that bead, micro-drops they wipe away; separate gameplay / Options-preview states. Review: plan approved by Codex in round 7 (5 + 2 granted; 23 findings accepted); code inspections 5 (1 + 4 granted, 17 findings accepted; the last 3 fixes uninspected by the user's choice). Rollback: `G:/SH4 Mod Archive/2026-11-14/Mod Backups/winmm.dll.v3.25.0.before_v3.26.0` (67f4b799). Plan / reviews / renders: `staging/v326_sv_rain/`.
- v3.27.0 (2026-11-17, main rig, session 15b; md5 462d7085; installed 2026-11-17 with the game closed; tested in game by the user 2026-11-17: the gameplay works (damage per type at the stated rates, a fog kill runs its own death, a downed ghost held, Eileen haunted), the flat art did not read in 3D): **Death Presence** (special mods round 2 of 5). Option 5 gets a **Special** page (hidden page LP_SPECIAL = 11, a copy of Button Config's record 0x654D88) with `[Gameplay] DeathPresence` Off / Weak / Strong: a black-violet fog round the player that drains every being in it on a pure, frame-rate-independent ramp (`src/dp_sim.h`, native test `test/dp_sim_test.c`), monsters die their own death at 0, ghosts are held down while inside, bosses and excluded types take nothing, companion Eileen takes a full haunting (HP + curse), Cynthia recoils, Room 302 hauntings clear slowly, enemies hesitate at the edge (one exe site 0x462987). Procedural textures only. Review: plan approved by Codex in round 5; code inspections 6 (2 + 4 granted by the user; 6, 6, 3, 4, 2, 3 findings, all edge cases after the third; the last three fixes uninspected). Rollback: `G:\SH4 Mod Archive\2026-11-14\Mod Backups\winmm.dll.v3.26.0.before_v3.27.0` (2da13fb5). Plan / reviews / RE: `staging/v327_death_presence/`.
- v3.27.1 (2026-11-17, main rig, session 16b; md5 867e8828; installed 2026-11-17 17:22 with the game closed; tested by the user the same evening: the swirls good, the effects vanished at a distance): **Death Presence in 3D.** A thin floor pool plus soft camera-facing smoke puffs filling a dome to hip height (depth-sorted, churning), a black halo and violet tendrils round her body, a stain on every victim that lingers after it leaves the fog (3 s half-life, `DpStainStep`), filter looks (Spirit Vision violet aura, Thermal cold void, night vision dark hole), companion Eileen's haunting at 1/4 of the enemy rate and a back-off so she keeps out of the fog, Cynthia's restart redirect on her follow test (0x4EBD16). Art judged by Codex from `tools/fog_preview.py` renders. Review: plan approved in round 9 (5 + 4 granted); inspections 4 (2 + 2 granted), the last 4-line fix uninspected. Lesson: an error grep hid a "not recognized" build failure, so build with the full path and check the dll's timestamp / md5. Rollback: `Mod Backups\winmm.dll.v3.27.0.before_v3.27.1` (462d7085). Plan / reviews: `staging/v3271_dp_art/`.
- v3.27.2 (2026-11-17, main rig, sessions 16b / 17b; md5 f8d340f1; installed 2026-11-17 20:28 with the game closed; tested by the user: still cut off at a distance and "halfway underground"): **Death Presence depth, cloak of death, filter looks.** The fog's sprites take the camera's own projection 0x12E3ED0 instead of 0xF4E760 (suspected of holding the last model's projection; the room log then showed both equal everywhere, and the real cause was the 1/128 depth factor, fixed in v3.27.3); the floor pool reaches her feet; the black outline only from toes to shoulders (head and upper chest free), smooth, with a hem that frays into the pool and trails behind her; Spirit Vision brighter, Thermal black, night vision pitch black with green-white filaments; companion Eileen's fit in the ghosts' crimson. Hidden `[ScreenEffects] DeathPresenceProbe=1` (floor squares at nine depth scales). Review: plan approved in round 4; inspection 2 APPROVED. Rollback: `Mod Backups\winmm.dll.v3.27.1.before_v3.27.2` (867e8828). Plan / reviews: `staging/v3272_dp_fix/`.
- v3.27.3 (2026-11-17, main rig, session 17b; md5 b84102e0; installed 2026-11-17 22:27 with the game closed; **confirmed in game by the user**: "looks very good across the board", the fog shows at a distance and in Building World 2nd stage 5 room 4): **the exact depth.** Death Presence's sprites, ribbons, pool discs and the probe are depth-tested at the depth the 3D pass really stores, z = P22 + P32 / w (P = the camera projection 0x12E3ED0, w = view depth), **without** v3.17's x 1/128 (fitted at one distance; no shader, matrix or wrapper step scales z: `staging/v3273_depth/re_depth.md`); floor puffs take the depth of a point half their radius nearer; from a steep top-down camera the cloak fades where it would cover her head / chest. Spirit Vision: the crimson tendrils start with dark blood clots instead of a flat edge; the fog brighter there. Hidden `DeathPresenceDepth=0` (the old x 1/128), `DeathPresenceProbe=2 / 3` (depth contour maps). Spirit Vision's own draws keep their calibrated 1/128. Review: plan approved in round 1; inspection 1 APPROVED. Rollback: `Mod Backups\winmm.dll.v3.27.2.before_v3.27.3` (f8d340f1). Plan / RE: `staging/v3273_depth/`.
- v3.28.0 (2026-11-18, main rig, session 17b; md5 ae3b58cc; installed 2026-11-18 02:46 with the game closed; **confirmed in game by the user**: the spirit flames "look great", the floor pool and the top-down halo fixed, fog visible at a distance): **Spirit Vision black heart, living spirit fires, black-fire trail.** Spirit Vision: every crimson haunting tendril of ghosts, the possessed companion Eileen (`SvEileenBones`) and the wheelchairs grows from one round black heart at the chest (corona, rim light, a lub-dub every 1.3 s running out along the roots; the mid-tendril blood shapes and root clots removed); soul fires are living flames (`src/spiritfire.inc`, `spiritfire_sim.h`, native test `test/sf_sim_test.c`: a 28-frame flame atlas baked once, three layers, embers, size from health, a flare and lashing on a hit, a guttering death; companions never go out); `SoulFadeSeconds` default 10 -> 6. Death Presence: a pronounced black fiery trail (v8c), the floor pool snaps to her after a room change or a jump of more than 150 u, wrapped beings fade in over 4 s. Built by Opus sub-agents in sandboxes (`a_tendrils`, `b_fire`, `c_trail`), merged by Claude. Review: plan approved in round 3 (round 2 asked for art changes); inspection 1 APPROVED. Rollback: `Mod Backups\winmm.dll.v3.27.3.before_v3.28.0` (b84102e0). Plan / designs / renders: `staging/v328_spirit/`.
- v3.29.0 (2026-11-18, main rig, session 17b; md5 B0D0225C; installed 2026-11-18 08:01 with `staging/v329_merge/install_v329.ps1`, the game closed; **installed, not yet tested in game**): **six gameplay features, all Off by default**, built by Opus sub-agents in sandboxes and merged one at a time by an Opus merge agent (`MERGE_NOTES.md`): After-image "Volt Trail" and Echo double (Special page), Enemy Copies and Enemies Follow (4 exe sites 0x4F0AF8, 0x450DB0, 0x419830, 0x486199; bone pool 4096), Enemy Health Bars and Supply Drops (Enemies page; drops through the game's helper 0x463150, a saved counter in unused bytes 0xFD0B98, at least 16 of 32 pickup slots kept free), the player combat pack Ammo Saver / Dark Pact / Grab Breaker / Tireless / Life Steal (sites 0x53DFAA / 0x53DFC6, 0x52D0E8, 0x53EE36, 0x5386BC; the 0x44F605 hit stub chains One-Hit Kill and the pack) and the Curse Meter (Player page). Each feature's design and the user's live checklist: `staging/special_mods/{r3_afterimage, r4_echo, r5_enemies, player_cheats/p1_combat, p2_hud, p3_drops}/PLAN.md`. Also the user's v3.28.0 feedback: the Death Presence trail violet / purple and thinner (x 0.6), the Spirit Vision heart's black fill opaque, `SoulFadeSeconds=6` written into the user's ini by the install script, and Thermal's depth bias also on **c12** (the skinned shader family's clip-z row: bodies flickered blue in stage 5 room 4 because v3.24.0 biased only c9, the rigid family's; `staging/v3291_feedback/thermal/RE_THERMAL.md`), with hidden `ThermalDepthBias` up to 10 and `ThermalRedrawDepth=0` for tests. SH4WM_VERSION says 3.29.0 again (v3.28.0 still reported 3.27.2). Review: plan approved in round 6 (5 + 1 granted); inspections 3 (2 + 1 granted; the last found only a stale md5 in the plan). Rollback: `Mod Backups\winmm.dll.v3.28.0.before_v3.29.0` (ae3b58cc). Plan / reviews: `staging/v329_merge/`.
- v3.30.0 (2026-11-18, main rig, session 18b; md5 46D1396F; installed 2026-11-18 20:23 with `staging/v330_feedback/install_v330.ps1`, the game closed; **installed, not yet tested in game**): **the user's v3.29.0 feedback** (special and player mods confirmed, enemy mods partly). Built by four Opus sub-agents in sandboxes and merged by an Opus merge agent (`staging/v330_feedback/merge/MERGE_NOTES.md`). **Echo Doubles**: Off / Shadow Copy (per-shadow random delay 0.15-0.6 s re-rolled every 2-6 s, eased) / Shadow Cast (while she attacks, each shadow dissolves and reforms beside an enemy on her floor within 1500 u, from a random side, and strikes; 75 / 60 / 50 % damage per shadow), new row **Echo Count** 1-3 (`EchoCount`), a shadow-soldier look (near-black body, violet rim and eyes, black-violet flames, smoke teleports), the 44 ms first-draw hitch pre-warmed; old Shadow / Twin / Mirror read as Shadow Copy. **After Shock** (was After-image): Off / Short 3 / Medium 5 / Long 8 copies (old On = Medium), one contact decision per enemy a tick, a frame guard. Henry audit: no Henry-specific fault; fixed for both bodies: a live body switch with an equal bone count (After Shock and Echo key the pose on the model file and skeleton) and a stun writing hp+0x1C (a double hit in one swing). **Supply Drops**: a session seed re-rolled at game start, New Game, every load (the load flag [0xFD5A74]) and quick load (`SupplyDropsSeed=Fixed` = v3.29). **Enemy Copies**: the bone pool grows to 4096 at the next room change (new site 0x4F0A5A, the bone reset in RoomLoadSetMark) because an in-game load never passes the world reset; reasons logged per missing copy. **Enemies Follow**: event dogs and risen Hummers follow; a temporary kill bit keeps a follower's home placement empty (no duplicate). **Revive Enemies** (new Enemies row, `EnemyRevive`): killed monsters and copies stand again on room re-entry (types 02 03 07 08 09 0A; never bosses, Walter, Wall Men, Wheelchairs, ghosts or story rooms; a follower's body revives only if its home placement qualifies); revived kills count for the rank. **Health bars**: ghost-true (a knock-down is the stagger gauge running out; rising heals 50 % / max/(n+3): `w4_hud/RE_GHOST_HEALTH.md`), a violet revive bar, sealed by the Sword of Obedience (shown 4 s after the pin in Locked Target), bevel look with a damage chip. **Curse Meter**: the ring of thorns. Review: plan approved by Codex in round 1; inspections 4 (2 + 2 granted by the user; 3, 2 and 1 findings, all fixed; the 4th APPROVED). Rollback: `Mod Backups\winmm.dll.v3.29.0.before_v3.30.0` (B0D0225C). Plan / reviews / renders: `staging/v330_feedback/`.
- v3.53.0 (2026-11-26, main rig, SH4 Refined Session 22; the Options layout rounds R1 + R2: two sandbox workers (W2 OL1 the words, then W3 OL2 the look) merged in that order by the merge agent, `staging/v353_layout/merge353_tools/MERGE_NOTES.md`; md5 5BDAC7F771A1C802C10073801358B616 (out_v353; merge E9ECCF75; Codex APPROVED on inspection 1) (merge353 out_m); **not yet installed or tested in game**): the plan `staging/options_layout/PLAN.md` (approved by the user). **OL1 the words:** every Options page, row, value and line in the plan's words on the rows where they are (`src/ol_words.h` generated from the plan's `layout_data.py`; page names Display, Controls, Interface, Presentation, Gameplay, Pause Menu; stock rows renamed through Refined's copies of their records; BGM / SE Volume keep their labels until R5; the Camera value reads "Eileen"; the Randomizer lines keep "Next room."); Room 302's Back line fixed, the Renderer 16-bit and Bloom lines no longer cut off, the unused Save Bank texts gone; the ini unchanged (screen words mapped). **OL2 the look:** the page strip, blue triangles on folder rows, "Parent > Page" titles, the dependent rows grey and inert with the reason first (the pause Camera rows too), the glyph lines; hidden `[Options] Look`; no strip clicks yet, the value arrows stay white on a grey row. **Merge:** the grey line is one line of at most 52 characters and one drawn line (the reason, then the row's own line, else the reason alone), the reasons and crumbs read OL1's names, titles are taken whole, value-less rows are not measured for arrows; one MSG_V53 block (OL1 V53 + 0 .. 89, 400 .. 409, 420; OL2 + 512 .. 551); no new exe site; 199 exports as v3.52.0.
- v3.54.0 (2026-11-26, main rig, SH4 Refined Session 22; the Options layout rounds R3 + R4, one sandbox worker (tag OL34), `staging/v354_layout/BUILD_NOTES.md`; md5 E364E634B47A8563A2D6E09CBA5F4E1A (out_v354; sandbox F0E2EB04; Codex APPROVED on inspection 1); **not yet installed or tested in game**): **R3** Option 5 becomes Gameplay with Player / Combat / Items / Enemies / Powers / Randomizer (Curse Meter and Enemy Health to Interface, Supply Drops to Items). **R4** the cycle Display, Controls, Interface, Sound, Effects, Gameplay: Option 4's rows to Display (Aspect Ratio, Display Mode, Monitor, Frame Rate) and Controls > Camera / Room 302 (the Fields of View, Room 302's rows); Option 6 becomes Interface > Menus with Menu Font and Start-Up; Controls > Camera holds the pause camera's settings (Camera, Distance, Side, Height, Tilt Down, Fixed Angles) and Steering the rest (the third level); Screen Effects becomes the top page Effects with Film Grain > and Shadows > (and Renderer, a PLAN section 4 fallback); v3.52's Sound page is top page 3 (Exit; Sound Mode lost its Enter); the hidden pages renumbered to PLAN section 6 (LP_MAX 22), every one on REQ_OPEN / REQ_BACK; the stock pages' special cases off on every rebuilt page (g_stockView 7), a stock row turns yellow by its own cfg byte wherever it sits; the stock pages' rows at their own pitch (10 / 11 rows). The stock rows stay until R5. Every ini key and written word as before; one MSG_V54 block; no new exe site; 199 exports.
- v3.55.0 (2026-11-26, main rig, SH4 Refined Session 22; the Options layout rounds R5 + R6, one sandbox worker (tag OL56), `staging/v355_layout/BUILD_NOTES.md`; md5 8C47349317D20CF203439562A55FD73C (out_v355; sandbox 25652C38, then Codex fixes OL56-001 / OL56-002; inspection 2 APPROVED); **installed 2026-11-26 17:12** (over v3.52.0; rollback `Mod Backups/winmm.dll.v3.52.0.before_v3.55.0`); **user QA 2026-11-26: items 1-11 pass** (requests and issues for v3.56: `staging/v356_feedback/USER_FEEDBACK_v355.md`); contains v3.53.0 and v3.54.0, which were never installed on their own): **R5** the stock rows on their plan pages (all R0-approved): Sound Mode, Music Volume, Effects Volume on Sound (the bar by the record's input: one exe patch at 0x525672, byte-checked, rolled back on failure, in SH4WM_Patches); Invert Look, Head Bob, Left / Right in Controls > Room 302; the Subtitles / Film Grain / Shadows switches row 0 of their folders (the three are pure folders now); Blood Colour on Gameplay; Renderer back on Display, Menu Font and Start-Up on Interface, Interface > Pause Menu again; every page at the plan's count (10 rows at most); a moved row keeps its cfg byte, its yellow and its "No"; hidden `[Options] StockMoves=0` = v3.54.0's places. **R6** the pause Extra Menu's "Eileen Outfit", its Camera page in the Options names (Tilt Down, Turn Speed, Invert Turn, Wall Zoom, Fixed Angles, Shoulder ...); a click on a page strip name opens that page (a slide, or the fade for the page in the same slot or from a folder page; needs Menus43); this README's pages, old -> new table and Settings notes in the new places. Every ini key and written word as before; one MSG_V55 block; one new exe site (0x525672); 199 exports.
- v3.56.0 (2026-11-26, main rig, SH4 Refined Session 22; eight sandbox workers (W8 SD356, W7 FR356, W5 PW356, W6 PV356, W1 MN356, W2 RI356, W4 ER356, W3 RE356) merged in that order by the merge agent, `staging/v356_feedback/merge356_tools/MERGE_NOTES.md`; md5 F31C4152FCB574373186BC0BDBFB0EE2 (out_v356; the merge out_m was C6A46D09 before the Codex fixes); Codex: inspection 1 REVISE (SD356-001: Background all four buffer sites or none), inspection 2 REVISE (SD356-002: the page protection and the rollback checked, a mix it cannot undo turns Background off for the session), both fixed, `staging/v356_feedback/PLAN-REVIEW-LOG.md`; **installed 2026-11-26 21:32** (over v3.55.0; rollback `Mod Backups/winmm.dll.v3.55.0.before_v3.56.0`), **play-tested 2026-11-27 with v3.57.0 (QA `staging/v356_feedback/QA_v356.md`: items 1-3, 6-11 pass; 4 / 5 the preview to change, 12 Reroll against Enemies Return, 13 a few slow seconds with powers together: the v3.58 round `staging/v358_feedback/`)**; QA `staging/v356_feedback/QA_v356.md`): the user's v3.55 QA and requests (`staging/v356_feedback/USER_FEEDBACK_v355.md`). **SD356 Sound**: an inaudible keep-awake signal (a 45 Hz tone at -72 dBFS) for the whole session, so a wireless headset is awake after Alt+Tab (hidden `[Sound] KeepAwake` / `KeepAwakeLevel`); Sound > Background Mute / Play (four one-byte exe sites 0x4095FE / 0x409B02 / 0x40A14C / 0x40A723: the buffers' global focus); a device watch says when a new sound device needs a restart (hidden `DeviceWatch`; following it live is not built). **FR356 Frame rate**: the final fight's contraption and the spear spot follow the rate (three sites 0x4E4CCE / 0x542529 / 0x54253F, byte-checked, all or none); the rate guard steps gameplay 180 -> 120 -> 60 when the play frames cannot hold the rate (hidden `[Window] RateGuard`; back up after 10 minutes clean at a room change, a rate left twice stays off for the session). **PW356 Powers**: After Shock dims the copies that cover the player in the Shoulder camera (30 %, hidden `AfterImageOtsFade`); at 120 fps and more the effect budget acts from 1.06 x the frame with two spark levels first (v3.55's levels 1 - 9 are 3 - 11) and tested blind steps (hidden `[Graphics] EffectBudgetHighRate`). **PV356 Effects preview**: from a pause, Thermal's heat and Spirit Vision's ghosts and soul fires (two GPU layers, the clean pause picture drawn back); with no paused game Refined's own preview scene, no game art (`[ScreenEffects] PreviewScene`, `PreviewLayers`). **MN356 Menus**: a mouse click opens a folder row (hidden `[Options] FolderClick`); every value its own line (`src/vlines.inc`, MSG_V56 + 0 .. 15); the pause row is "Eileen" again and each pause row is drawn once (hidden `[ExtraMenu] StockShadow`). **RI356 Randomizer items**: Specials (turned round), Room 302 Items Off / Shuffle / Random, Reroll Visits By Room / By World, no Seed / New Seed rows (the seed in small type), no "Next room."; v3.55's KeepSpecials / Room302 / PerVisit migrated once. **ER356 Enemies Return** Off / By Room / By World (By Room = v3.55's On; the Tremer site 0x48ACE5, hidden `ReturnTremers`). **RE356 Randomizer enemies**: Enemies Off / Same World / All Worlds with the victims' ghosts (Monsters migrated), Spawn Site Off / On for every enemy (Ghosts migrated). **The merge**: ONE world-visit rule for Reroll Visits, Enemies Return, Enemies and Spawn Site (`src/worldvisit_sim.h` / `worldvisit.inc`: a load is the game's own signal, gEnemyManager.stage -2 after every save's apply, proven to cover Quick Load, Load Game, the title's Load and Continue; one WvOnRoomLoad call per room load; tests `worldvisit_test` 369 305 + `worldvisit_visits_test` 33); the rate guard waits for the effect budget only while it can still shed; a pause capture is never a late frame for either; the page strip and the folder click share one mid-slide refusal; Randomizer > Eileen Outfit Off got its own line. One MSG_V56 block (MN356 V56 + 0 .. 15, SD356 + 112 .. 115); eight new exe sites (above), all in SH4WM_Patches; 199 exports; no save format change.
- v3.57.0 (2026-11-26, main rig, SH4 Refined Session 22; Options pass 1, one sandbox worker (tag OLP1), `staging/v357_olp1/BUILD_NOTES.md`; md5 C65DE6A69484847330EC52F747B726F7 (out_v357); Codex inspection 1 REVISE (OLP1-REV-001: a README settings row still said Reroll Visits; fixed, README only, not re-inspected), `staging/v357_olp1/PLAN-REVIEW-LOG.md`; **installed 2026-11-26 22:16** (over v3.56.0, which was not played first; rollback `Mod Backups/winmm.dll.v3.56.0.before_v3.57.0`), **play-tested 2026-11-27: items 1-7 pass (QA `staging/v357_olp1/QA_v357.md`)**; QA `staging/v357_olp1/QA_v357.md`): the Options Layout session's pass 1 (`staging/options_layout/ASSESSMENT.md`, Codex-reviewed there): 48 rows' descriptions in the conventions N1 - N3 / D1 - D9, three renames (Room 302 Left / Right -> **Turn or Strafe**, Quick Charge 1.5x / 2x / 3x -> **x1.5 / x2 / x3** with the ini still 1.5x / 2x / 3x, Randomizer Reroll Visits -> **Reroll**), the Frame Rate 120 / 180 lines ("experimental; needs a fast PC."), the 16 state lines outside the plan ("... See SH4Refined.log.", the Filter's " Press F7 in play for a new preview." as a second line), the old Option 5 Player folder record's line "Speed, healing and escapes." (Codex OLP1-002); Regeneration x1's line and Ghost Teleport's cooldown line say "seconds"; no row moves, no ini key or written value changes, no exe site.
- v3.58.0 (2026-11-27, main rig, SH4 Refined Session 22; eight sandbox workers (W8 SD358, W7 GX358, W6 GT358, W5 FI358, W4 EN358, W3 RZ358, W2 PV358, W1 MN358) merged in that order by the merge agent, `staging/v358_feedback/merge358_tools/MERGE_NOTES.md`; md5 FBF82500CB507CFA742792E8FB5AA38A (out_v358; the merge out_m was 00FA14DD before the Codex fixes); Codex: inspection 1 REVISE (SD358-001: a stale mute marker could clear a mute the user set in Windows; fixed: the marker lists the sessions Refined muted), inspection 2 REVISE (SD358-002: the marker write was not checked; fixed: written to a temp file, verified, then moved over, and no mute unless recorded), inspection 3 granted by the user REVISE (SD358-003: a failed mute whose marker cleanup also failed left a name that could undo a later user mute at the next start; fixed: entries are pending until the mute is confirmed, only confirmed ones are unmuted at a start, failed writes retried; not re-inspected), `staging/v358_feedback/PLAN-REVIEW-LOG.md`; **installed 2026-11-27 09:14 over v3.57.0 (rollback `Mod Backups/*before_v3.58.0`; lock 09:13, released 09:14), not yet tested in game**): the user's v3.56 / v3.57 QA (`staging/v358_feedback/USER_FEEDBACK_v357.md`). **SD358 Sound**: Background Mute mutes every sound at once behind other windows (Windows' sound session of the game, on every output device, the keep-awake tone included; only Refined's own mutes are undone; a next start unmutes a mute left by an exit behind other windows, the file `SH4Refined.muted`; WM_CLOSE unmutes first; hidden `[Sound] SessionMute`); Music / Effects Volume 0 is silent (the game's attenuation floor of -60 dB, -40 dB with Night Mode High, is gone for its 0; Room 302's stream 0x2774 no longer fades itself back up from 0: new site 0x40B2E7; hidden `ZeroSilent`). **GX358 Graphics**: Shadows Off is the game's Off at once (the game keeps a few rooms' shadows, as stock); the Shadows page's preview and Echo Doubles' ground discs follow it; powers starting together at 120 / 180 take the effect budget's start step (then fill back in), far-late frames step 0.25 s apart, the rate guard judges the last second when the budget has nothing left (hidden `[Graphics] EffectBudgetQuick`). **GT358 Ghost Teleport**: the fade's dark lines gone (her stock draw builds the pose only; after the actor models, call 0x4EF905, her body and weapon get a depth pre-pass and one blend through the game's pixel shader 5); Thermal's heat body and Spirit Vision's soul fire fade with her, her shadow from half opacity (hidden `[Gameplay] GhostTeleportFadeDraw`). **FI358 Reveal Method's portal**: sized to the arriving body (1.24 x its height from the first portal and up to 9 smaller ones plus two blot sets, nothing stretched), Refined's sticky strands (16; 8 beside a ghost's own) hold 3 s after it is in, the portal open until then; Jasper / Andrew / Richard get strands again (hidden `FadeInStrandSeconds` / `FadeInPortalPieces` / `FadeInStrands`). **EN358 Enemies**: victims the randomizer drew follow through doors and are judged by Pursuit; a per-door line of the followed ghosts; no health bar for an enemy the game does not draw yet (the Subway's event dogs before story flag 0x130), the bar fades in with the body; Walter downed keeps his bar and gets a revive bar from the game's own down timer. **RZ358 Randomizer**: the reroll no longer brings killed enemies back at a room change (bodies keep the type they died as, kill bits and saves keep them dead, Enemies Return alone decides); Extra Enemies' copies draw their own types (their files loaded, their own sites with Spawn Site; hidden `[Gameplay] CopyRecordGuard`). **PV358 Effects**: Effects > Preview Off / Stock / Live (`[ScreenEffects] Preview`, default Live; the stock picture `SH4Refined_preview_stock.dds` is a file in the game folder, never in the dll, never written by F7; v3.56's hidden PreviewScene migrated once); Effects > Filter Strength Low / Medium / High / Ultra (default High = v3.16 - v3.57's look; v3.15's unread value read once as High through the marker `FilterStrengthV`); Depth of Field and Blur Strength in the folder Effects > Depth of Field > (Effects 11 rows); the preview's Thermal heat layer writes the bodies' depth again. **MN358 Options**: one value box per page (values centred, both arrows at its edges on every row and value, the mouse rects with them, the volume bars' arrows through sites 0x5256B6 / 0x5256E2, the right-arrow rect's width set on the long-value path that v3.19 - v3.57 left stale, the pause Camera page's ">" at one place; hidden `[Options] ValueBox`); Fade In -> Reveal Method (Fade -> Fade In), Film Grain Strength Very High -> Ultra (the ini words unchanged), no "Next room." in Extra Enemies' lines, Item Stacks On's line; the randomizer's no-pickup-model stage line is a plain line. **MERGE**: the heat redraw keeps PV358's depth write and GT358's pre-pass; FiInit keeps FI358's keys and MN358's label; one MSG_V58 block (PV358 V58 + 16 .. 28); 199 exports; four new exe sites, none overlapping another, Playable Eileen's hooks or the cheat table; run_tests.py's 17 known FAIL lines unchanged.
- v3.52.0 (2026-11-26, main rig, SH4 Refined Session 22; seven sandbox workers (W4 SH352, W7 EP352, W2 HB352, W5 TP352, W6 QL352, W3 FX352, W1 SND352) merged in that order by the merge agent, `staging/v352_feedback/merge352_tools/MERGE_NOTES.md`; md5 9AC9B22AB768C125565D24FB5D985018 (out_v352; merge D270E181, then Codex fix SND352-001; Codex APPROVED on inspection 2); **installed 2026-11-26 12:20**; **user QA 2026-11-26: items 1-5, 8-10 pass** (6 the Otherworld Lens not noticed though the log shows it drawn at 70 %; 7 the turnstile floor shadows are baked into the textures, not a real-time shadow); rollback `G:/SH4 Mod Archive/2026-11-14/Mod Backups/*before_v3.52.0`): the user's v3.51 QA (`staging/v352_feedback/USER_FEEDBACK_v351.md`). **SH352 Shadows** (D5, D8): the Darkness row runs 25 -> 100 % (Left lighter), the ini unchanged; 4:1F and cut-scene 0x43 have no real-time shadow in the game (0x55F660 sets the darkness to 0); a reason logged for every composite that is not soft. **EP352 Enemy Pursuit** (QA 2): Forgetful's victims' ghosts give up at 1200 u for 3 s, after 8 s, or 3 s out of sight; after the rest a ghost stays calm until it senses you again (Normal 2000 u, Forgetful 1200 u); dives into a wall set back, ghosts never handed to the game by the quick guard. **HB352 Enemy Health Bars** (D1, D2): only for enemies the game draws in this room; a boss's bar at any distance, at the top edge for a tall body. **TP352 Ghost Teleport** (D4): every landing proven by her body and the floor map; freed from a wall after a warp. **QL352 Quick Load from the pause** (Q2): a fade to black inside the pause, the music paused until the loaded room runs (hidden `[ExtraMenu] QuickLoadInPause`). **FX352 Screen Effects** (D3, D6, D7): Depth of Field with a close third-person camera (only Room 302's first-person view and the views of another place skip it); Bloom Off / Low / Medium / High / Ultra, migrated once; the Otherworld Lens at 70 % (hidden `LensBase`). **SND352 Sound** (Q1): Option 1 > Sound Mono / Stereo / Headphones / Surround (`[Sound] Mode`), the Sound page (Stereo Width, Room Reverb, Night Mode), a smooth pan law, the Mono fold, the Options "No" discard fixed; eleven exe sites in four groups. **Merge rule:** the Sound mode keeper waits while Quick Load from the pause holds the sound, and the music comes back through the game's own path (Night Mode and Mono still apply). Text ids: the MSG_V52 block (V52 29907, MSG_END 29971; SND352 V52 + 0 .. 18, the rest unused); exports unchanged (199).
- v3.51.0 (2026-11-25, main rig, SH4 Refined Session 22; five sandbox workers (W5 MU351, W4 SN351, W1 TP351, W2 EP351, W3 PT351) merged in that order by the merge agent, `staging/v351_feedback/merge351_tools/MERGE_NOTES.md`; md5 F4D0E4860A15A08170F34AE5F6233BB2 (out_v351; merge 850861C1, then Codex fixes TP351-001 / PT351-001 / PT351-002); **installed 2026-11-26 00:31**; **user QA 2026-11-26: items 1-5 pass** (Forgetful victims still hard to shake: v3.52); rollback `G:/SH4 Mod Archive/2026-11-14/Mod Backups/*before_v3.51.0`): the user's v3.50 QA (`staging/v351_feedback/USER_FEEDBACK_v350.md`). **W5 MU351, the level music in the load list** (QA item 11): the Extra Menu's Storage Chest / Save Game / Load Game no longer bring the level music back (v3.32 - v3.50 called the game's "resume sound" and then its "overlay" pause: full volume for a moment, then 20 %); the pause's own silence goes on (hidden `[ExtraMenu] PauseSound`; `test/pausesound_test.c` 132 checks). **W4 SN351, the first Sniffers** (QA item 4): Subway room 2:2's three event dogs (and their copies) were no source placement; they roll once story flag 130 is set (238 sources, was 235), else stay as placed; a roll of the placement's own type rolls once more (hidden `[Randomizer] MonsterRollAgain`); the native test replays the user's 23 logged rolls exactly (`test/rzmon_sim_test.c` 1 130). **W1 TP351, Ghost Teleport and collision** (QA item 2): never behind an enemy on the other side of collision (a wall, a prop, a turnstile, a ghost in its wall): the next enemy in the cone in the open, else the warp ahead (hidden `GhostTeleportBehindCollision`; `test/gt_sim_test.c` 226). **W2 EP351, Enemy Pursuit inside the room** (QA item 9 failed: v3.50 acted only at doors): chasing monsters give up after 10 / 4 s out of their senses and go back to their own routine by the game's own AI states; victims' ghosts give up when far (2500 / 1500 for 12 / 6 s) or after a 25 / 12 s chase without touching you, then rest 8 / 15 s; one that gave up does not follow through the next door (hidden `PursuitInRoom`; `test/pursuit_room_test.c`). **W3 PT351, the tar portal** (QA item 7): Fade + Portal's portal is the game's own wall portal look (its draws 0x4868B0 / 0x486E80 from Refined's blocks after the game's pass 2, phase 0's step list entry 0x62BC98) with the ghost's own strands; the arriving ghost is held 2.5 s (40 % speed, no grab; hidden `FadeInPortalDraw` / `FadeInHold`; `test/fadein_sim_test.c` 314). **Merge:** a ghost in the tar portal's hold is not judged by Enemy Pursuit (its timers start when the hold ends; it never sets the state of, withdraws the requests of or moves a held ghost; `test/pursuit_room_test.c` TestPortalHold). Text ids: MSG_V51 = MSG_V50 + 64, MSG_END = MSG_V51 + 32 (none used). New exe site: none in code; one data site 0x62BC98 (4 bytes, byte-checked, put back on a failed write; data, so not in SH4WM_Patches, as EntityGate's 0x62BC94 / 0x62BCB0). Exports unchanged (199). No save format change. Tests: 45 native tests, logcheck_test, fontlib_test, check_exports pass; run_tests.py: the known 17 FAIL lines only.
- v3.50.0 (2026-11-25, main rig, SH4 Refined Session 22; six sandbox workers (W2 RM350, W3 RL350, W6 R302, W1 TP350, W5 EF350, W4 FI350) merged in that order by the merge agent, `staging/v350_feedback/merge350_tools/MERGE_NOTES.md`; md5 B3185A0C0E6BB5AB22C56FD2BC535400 (out_v350; merge E9AA41AB, then Codex fixes TP350-001 / FI350-001); **installed 2026-11-25 18:28; user QA 2026-11-25: items 1-8 and 10 pass, 9 Enemy Pursuit fails (acts only at doors; fixed in v3.51)**): the user's v3.49 QA (`staging/v350_feedback/USER_FEEDBACK_v349.md`). **W2 RM350, Randomizer Monsters** (QA items 4 / 5, "Monster randomizer does not work"; queue "monsters from all worlds"): a game played from saves holds the game's own enemy record of every room the save visited, and v3.49's RZ349-SAVE-001 rule kept every placement with a record SH4 Refined had not written as placed, also a record of the stock monster (the user's log: 7 room loads, 1 of 15 placements changed); now the game's own record of the stock monster in its room is adopted (rolled like a fresh placement, deleted when the roll gives another type, Refined's from then on), records of another type Refined did not write keep RZ349-SAVE-001's protection; new value **All Worlds**; the room line counts the adopted records and why each placement kept its stock type (`test/rzmon_sim_test.c` 1 100 checks). **W3 RL350, Randomizer looks** (QA item 6 "change per room, remove the unlock limiter", item 7): **Eileen Outfit** draws among every companion outfit (no Nurse unlock) and, with Change Each Visit On, again at every room load of her world (at most two rooms in a row alike; the final fight keeps its outfit), through the outfit engine's room-load path; **Ghosts Spots** now shows: v3.49 could swap two victims, moved most 400-600 units, and the game restored a victim from its own record half the time (EnemyGhostReconcile 0x482590); now 800 + away when the room allows, never at another ghost's place, and a standing record of the victim in the room moves with it (pinned / down ones stay) (`test/rzlooks_sim_test.c` 11 101). **W6 R302, Room 302 for Henry and Eileen** (QA item 9): Options > Option 4 > Room 302 (Look Down, Look Up, Look Feel, Walk Speed), taken over from Playable Eileen's Room 302 (its look limits, exact pitch clamp and look easing: sites 0x52DF74 / 0x52DCC6 / 0x52DD14; its walk speed through Movement Speed's own Room 302 operands); inert (rows grey) while Playable Eileen has no SH4PE_Room302Yield export (v1.24.4; PE v1.25.0 accepted `staging/v350_feedback/w6_room302/R302_CONTRACT.md`); new export SH4R_Room302 (@200); seeded from SH4PlayableEileen.ini [Room302] (`test/r302_sim_test.c` 137). **W1 TP350, Ghost Teleport through walls** (QA item 2, A1-A3): the warp ahead looks along her facing and +-10 / +-20 degrees, every 40 u, past the first thing she could not walk straight to, for the nearest standable spot whose floor a route of the room reaches (a flood of the room's floor by the player's own walking rules from its doors and her standing places, built once a room visit: Subway room 2:9's whole map is 2648 cells, 3.8 ms natively); behind the enemy she faces only (inside +-35 degrees), from twice as far (5000 u); the fade-out owns the Action button (v3.49's log: 7 warps called off by an examine); a quota per refusal kind, one line per room visit (`test/gt_sim_test.c` 202). **W5 EF350, Enemies** (C1, C4, C5): Enemies Follow "Victims Only" renamed Victims; Enemy Pursuit Relentless / Normal (default) / Forgetful (enemies lose track of you: out of their senses 10 / 4 s when you leave, downed when you leave, 2 / 1 doors without catching up); followers survive saves and loads (side file save\SH4Refined_follow.dat keyed by the save's enemy block; hooks 0x4FC1A4 / 0x4FC6ED, `[Gameplay] FollowSaveFile`); a load no longer reports a follower that came in after the save as "lost between the rooms" (`test/pursuit_sim_test.c` 88). **W4 FI350, Fade In** (C2, C3): a new Enemies row, `[Gameplay] FadeIn` Off / Fade / Fade + Portal (default): every enemy actor is remembered at its first visible draw of a room visit and one that may fade is drawn by the game's own see-through enemy draw 0x45C880 instead of ModelDraw over 0.8 s (the existing 0x52AEB5 / 0x52AF95 hooks, through a wrapper `ThermDrawFi`; the shadow joins at half in; the arriving victim's placeholder -> ghost take-over keeps the fade); Fade + Portal draws a procedural crimson vortex for each ghost that arrives after the room's first 1.5 s, in the scene pass with the exact depth; never bosses, Wall Men, mm, Wheelchairs, the Patient, Walter, event spawns, dead bodies, cut-scenes or a ghost from the game's own portal (`test/fadein_sim_test.c` 243). The Enemies page: Enemy Copies, Enemies Follow, Enemy Pursuit, Enemy Health Bars, Supply Drops, Revive Enemies, Fade In, Back (8 rows); Option 4 gets the Room 302 folder row (10 rows). Text ids: MSG_V50 = MSG_RZ1 + 32, MSG_END = MSG_V50 + 64 (FI350 V50 + 8 .. 15, EF350 V50 + 16 .. 23, R302 V50 + 32 .. 61; RM350 RZ1 + 5 / + 6). New exe sites: 0x4FC1A4, 0x4FC6ED (5 bytes each, byte-checked, in SH4WM_Patches, on no Playable Eileen hook or cheat table span); R302's three only when Playable Eileen yields. New export SH4R_Room302 (ordinal 200, DATA; 199 exports). No save format change. Tests: 43 native tests, logcheck_test, fontlib_test, check_exports pass; run_tests.py: the known 17 FAIL lines only.
- v3.49.0 (2026-11-25, main rig, SH4 Refined Session 22; seven sandbox workers (W2 GT349, W6 SH349, W1 SB349, W7 LK349, W5 EF349, W3 RZ349, W4 RZ349L) merged in that order by the merge agent, `staging/v349_feedback/merge349_tools/MERGE_NOTES.md`; Codex: 2 inspections (GT349-001 a warp out of an unfired trigger zone, RZ349-SAVE-001 the save filter's provenance; both fixed, the second fix not re-inspected); installed 2026-11-25 13:53, md5 5D6E6A53780FB0C595A4EAF0EB476883; user QA 2026-11-25: 1-3, 8-10 pass; Monsters / saves / Ghosts fixed in v3.50): the user's v3.48 QA (7 / 7 passed, one crash; `staging/v349_feedback/USER_FEEDBACK_v348.md`) and the queued items. **The v3.48 crash** (C0000005 at 0x43E775 in Subway room 2:3, `staging/v349_feedback/w2_teleport/CRASH_ANALYSIS.md`): a Ghost Teleport in room 2:2 jumped over the stock cut-scene trigger of event 00A ("Creatures I", story flag 0x130), so Cynthia's companion task survived and the washroom door recreated it in room 2:3, which does not hold her files, and her first animation tick read a NULL clip (Refined's cause; Playable Eileen not involved). **W2 GT349, Ghost Teleport** ("keep holding the combo to teleport continuously", "no 3 second cooldown"): held Run + Action warps again as soon as she has faded in; the hidden cooldown defaults to 0 (an old explicit value still applies); a warp's path may not cross or end in a walk-in event zone the game would still fire (the room's event list 0x13D8200, box / cylinder / sphere), and no warp (or the fade is called off) while a door is in use, a room change is pending, a script holds the player, an interaction event runs or the screen fades; logcheck reads the live chord lines again (`test/gt_sim_test.c` 152 checks). **W6 SH349, soft shadows for the whole game** (addition E2): every real-time shadow (every caster's stencil volume) meets in the one mask Soft and Darkness change since v3.40 / v3.42, so all were covered; the Tremers' wall shadow, a subtractive ribbon outside the mask, now gets the Darkness (hook 0x45F4CA, `[Shadows] TremerShadows` 1); baked shadows stay as painted; the ini texts say "every real-time shadow" (`test/shadows_sim_test.c` 438 140 checks, new `test/shribbon_test.c` 225). **W1 SB349, the save list** (QA item 2): the Load / Save list's cursor bar is the game's blue menu bar (the colour instructions of 56FDA0 at 0x56FDCF rewritten into 56FCC0 style 0's, 11 bytes, byte-checked; `[SaveBanks] ListBar` Blue / Yellow), and the pointer no longer vanishes on the Bank row (Save Banks now calls InputMouseCursorUnhide 0x405890 on the frames it skips the list's input 0x570F30; no patch site) (`test/savebanks_sim_test.c` 94). **W7 LK349, Room 302's right stick** (addition E3): `[Input] OmniLook` Room302 (default) / All / Off writes the analog right stick into gPad.stick[1] after its own self-check (the 0x5543C0 hook; no new site), so the game's on / off look and its reversal latch (the "sticky" look, worse as vanilla Henry under Playable Eileen v1.24.2) no longer reach Room 302's view; with X turns a forward walk no longer silences the right stick's turn; Playable Eileen's part (its v1.24.3 PadAxis gate) is written down for its session (`test/omni_sim_test.c` 245). **W5 EF349, Enemies Follow Victims Only** (addition E1): values 0 Off / 1 Monsters / 2 Victims Only / 3 Monsters + Victims (in memory; the ini keeps the names, old inis read unchanged); only the ghosts' path follows, monsters' pursuits end and monsters left elsewhere go home at the next room load (`test/enemies_sim_test.c` 287). **W3 RZ349, Randomizer Monsters** (spec C): `[Randomizer] Monsters` Off / This Room / This World (default): each source placement (235 of 783: Whitestool, Hummer, Gum Head (with Playable Eileen's crash guard), Twin Victim, Tremer, Sniffer Dog) rolls its own type at the room load through the same patch-and-restore stack (type / mode / y words), This World loading the files; never story / boss / event monsters or ghosts; saves never hold a randomized monster (the save filter re-aims `call 42BBB0` at 0x465468); off while the cheat table's 7700 is on (`src/rzmon.inc`, `src/rzmon_sim.h`, `test/rzmon_sim_test.c` 938, `tools/rzmon_scan.py`). **W4 RZ349L, Randomizer Eileen Outfit and Ghosts** (spec D): `[Randomizer] EileenOutfit` Off / Random (companion Eileen among the save's unlocked outfits, per world visit, through the outfit engine; with Playable Eileen the companion only) and `Ghosts` Off / Spots (victim ghosts at another valid spot of their room: their position words patched and restored at the same stack level, after the Monsters); no exe code patched (`src/rzlooks.inc`, `src/rzlooks_sim.h`, `test/rzlooks_sim_test.c` 6 661, `tools/rzl_scan.py`). The Randomizer page: Items, Keep Specials, Room 302, Change Each Visit, Monsters, Eileen Outfit, Ghosts, Seed, New Seed, Back (10 of 11 rows). Text ids: MSG_RZ1 = MSG_RZ0 + 32, MSG_END = MSG_RZ1 + 32 (RZ349: RZ0 + 28 .. 29 and RZ1 + 0 .. 4; RZ349L: RZ1 + 16 .. 23). New exe sites: 0x56FDCF (11 bytes), 0x45F4CA (5), 0x465468 (5), all byte-checked, in SH4WM_Patches, none on a Playable Eileen hook or a cheat table span. No new export (198; 199 = SH4R_StickAmount as v3.48.0), no save format change. Tests: 40 native tests, logcheck_test, fontlib_test, check_exports pass; run_tests.py: the known 17 FAIL lines only.
- v3.48.0 (2026-11-25, main rig, SH4 Refined Session 22; three sandbox workers (W1 OM348, W2 SB348, W3 RZ348) merged by the merge agent, w2 and w3 in two rounds (round 2: the user's changes after the first merge, `staging/v348_feedback/merge348_tools/MERGE_NOTES.md`); Codex: 2 inspections, 3 findings fixed (RZ-001 taken slots, RZ-002 the patch stack with `__finally`, RZ-003 the visit counter), the second APPROVED; installed 2026-11-25 07:32, md5 8CF8CD92BB2BE0EA147BE43530907E20; not yet tested in game): the user's v3.47 QA (7 / 7 passed; `staging/v348_feedback/USER_FEEDBACK_v347.md`) and the first queued item. **W1 OM348, Omni-Directional Movement** ("Rename Stick Movement to Omni-Directional Movement. Parameters should be Off/On. On = Analog ... So remove Direction please."): the Option 2 row "Stick Movement" is "Omni-Directional Movement", Off / On; On = v3.47's Analog in every respect (the 0x5543C0 hook, the self-check, the dead zone, `SH4R_StickAmount` 0.6..1, Room 302 / Playable Eileen's rules); Direction removed. `[Input] OmniMove` reads Off / On, 0 / 1 (any case, spaces around it); v3.46 / v3.47's Direction / Analog / 2 read as On and the first start that finds one writes it back once as On (`omni: [Input] OmniMove=... (v3.46 / v3.47) is On now ...`); anything else = Off, logged once. The label measured with the game's font metrics: 286 canvas px (font 0), inside the value arrow at x 344; logs and logcheck say "omni-directional movement" and still read v3.46 / v3.47 logs (`test/omni_sim_test.c` 180 checks; plan `staging/v348_feedback/w1_omni/PLAN.md`). **W2 SB348, the Bank row's batch scroll and mouse hold** ("batch scroll by 10 using Q/E ... RB/LB ...", "holding down the blue arrows with our mouse"; round 2: "I mistakenly said L1/L2 for PlayStation when I meant L1/R1"): on the highlighted Bank row Left / Right and the D-pad step 1 bank, Q / E and the shoulders (buttons 4 / 5: LB / RB, L1 / R1, L / R) 10 banks, clamped at the first / last bank; the triggers do nothing there (round 1's trigger reading, buttons 6 / 7 and the Z axis, removed: no device call left in Save Banks); keys, buttons and the left mouse button held on a blue arrow repeat with the row's accelerating repeat (`kSbRep`); on a slot Q / E / L1 / R1 stay 1 bank; no XInput, no new ini key, no exe patch; the per-step `save banks: bank N` line is verbose and quota-limited in the lists (Option 6's row keeps it) and one normal line per settled change (`bank 3 -> 27 ...`), logcheck's bank summary counts both (`test/savebanks_sim_test.c` 77 checks; plan `staging/v348_feedback/w2_banks/PLAN.md`). **W3 RZ348, the Randomizer, first part** (queue item 1, `staging/research_randomizer/REPORT.md`): a "Randomizer" folder on Option 5 under Special (page LP_RANDO 14) with Seed, New Seed, Items Off / Shuffle / Random, Keep Specials, Room 302, Change Each Visit (`[Randomizer]`, defaults Random / Off / In / PerVisit On / Seed 0 = rolled once at the first start and written, the user's answers); the 137 plain item pickups and (round 2: "allow weapons to be randomized too") Henry's 27 lone weapon pickups are randomized at each room load through the wrapped scene-script runner (EnmWrap, the call 52CA00 at 0x4F0AF8): patch-and-restore of their item words and (weapon spots) pickup-record words, so the model, the prompt and the gift follow the new item (round 2: "it should look like the item we are picking up"); Random never draws an owned weapon or one weapon twice in a room; with Change Each Visit (round 2: "a room should give different items on every visit") Random rerolls a room's untaken spots at every room visit and Shuffle at every world visit (an in-memory visit counter), else deterministic from (seed, stage, room, taken bit); nothing saved, no exe code patched (`src/rando_sim.h`, `src/randomizer.inc`, `test/rando_sim_test.c` 339 382 checks, `tools/rz_scan.py`; plan `staging/v348_feedback/w3_rando/PLAN.md`). Text ids: MSG_RZ0 = MSG_SB0 + 16, MSG_END = MSG_RZ0 + 32 (28 of 32 used); MSG_OM0 9 of 16. No new export (199 = SH4R_StickAmount as v3.47.0), no new exe patch site, no save format change. Tests: 37 native tests, logcheck_test, fontlib_test, check_exports pass.
- v3.47.0 (2026-11-24, main rig, SH4 Refined Session 22; five sandbox workers (W5 PX347, W2 DF347, W3 SB347, W1 CT347, W4 RG347) merged by the merge agent, `staging/v347_feedback/merge347_tools/MERGE_NOTES.md`; Codex: 3 inspections, 2 findings fixed, the third APPROVED; installed 2026-11-24, md5 1B913F7C4F2C6FB981EB5168224365C7, not yet tested in game): the user's v3.46 QA (`staging/v347_feedback/USER_FEEDBACK_v346.md`). **W5 PX347, the pixel checks** (the v3.46 log: every check failed once the read back worked under dgVoodoo2): the check was wrong, not the shaders: the test draw samples half a texel left of the texel centres on that backend (KB 194's half-pixel rule), and v3.44's 2-texel cells mixed each cell with its neighbour (every logged value = the shader's sim on that mix, within 1 level / 0.002 alpha). The checks now use 4 cells of 8 x 4 texels read at their centres, read the source back too (a Clear that did not land is named, the shader judged on what it held) and log the measured offset (`pixel check alignment (...)`); no look change (`test/px_model.h`; plan `staging/v347_feedback/w5_pxcheck/PLAN.md`). **W2 DF347, the depth of field eases in** ("the DoF should be eased into, but currently has a hard starting point"): every blur level is reached through four thin depth slices a quarter level apart (centred on v3.46's steps); the composite blends the next blur level over the one below at 25 / 50 / 75 % (bloom's proven up-step shader); far and near are drawn one after the other; the pause's depth map is strength-free, so the page's preview follows any strength. Draws a frame: Low 19 / Medium 27 / High 43 (were 6 / 8 / 12; about +0.05 to +0.15 ms). The log's first-frame line says "eased in"; logcheck reports it (plan `staging/v347_feedback/w2_dof/PLAN.md`). **W3 SB347, the Bank row** in the Menu Font: the label is drawn as `\hBank N` (the list's own escape for Latin glyphs; v3.8 .. v3.46 drew plain ASCII = the game's special monospace glyphs, which the HD font page cannot redraw); "Q/E L1/R1:" removed; the row centred over the list (bar 192..320, arrows 158..178 / 334..354); the mouse's hover box covers bar + arrows (v3.46: the bar alone, so moving onto an arrow dropped the row and the click then confirmed the list's slot); left arrow = previous bank, right arrow / bar = next, through the Q / E path and sounds; a click on the row while the list has the cursor selects the row instead of confirming a slot (`src/savebanks_sim.h`, `test/savebanks_sim_test.c`; one more byte-checked call, 0x5587F0 the text width; plan `staging/v347_feedback/w3_banks/PLAN.md`). **W1 CT347, Stick Movement on Option 2 (Control)** ("Move this to Option 2 (Control)", "I'd like Analog to work with Playable Eileen too!"): above Exit (row 7 of 8; a copy of Option 2's table, with Refined's Vibration levels when they are in; the stock 32-pixel rows), the Player page without it; Exit's change dialog / "No" / the yellow row / the live ini as before (PadChanged now asks OmChanged). New export **SH4R_StickAmount** (@199) = this frame's Analog push for Playable Eileen (0 Off / cannot act, 1 full, 0.6..1 Analog); Refined reads PE's proposed `SH4PE_StickAmountOn` for the row's line ("Eileen keeps her own speed" until then); contract for PE's session: `staging/v347_feedback/w1_control/CONTRACT_analog_pe.md`. GetDeviceState failures while the game window is not in front (the user's three anomalies when the taskbar came over the game, hr 8007001E) are one verbose focus-loss line per episode; failures with the game in front stay an anomaly (plan `staging/v347_feedback/w1_control/PLAN.md`). **W4 RG347, Refined's own Regeneration** (queue item 6 and the contract with Playable Eileen v1.23.2, `SH4PlayableEileen/staging/s59_regen/REGEN_CONTRACT.md`): Option 5 > Player > Regeneration Off / x1 / x1.25 / x2 (`[Gameplay] Regen`, hidden `RegenDelay`; seeded once from SH4PlayableEileen.ini), PE's rates and rules, its own aura check (0x10A3320 / 0x10A2EE0), dt = 1 / the tick rate; Refined heals Henry and Eileen with PE's own off (`SH4PE_RegenActive() == 0`), the row grey Off while PE's own is on, v3.46's front end with an older PE; the row is always on the Player page now (with Stick Movement gone: 10 rows with or without Playable Eileen). No exe patch: the 0x4EF982 tick and PlayerHealAmount 0x52D1D0 (byte-checked); Life Steal: 0x52D1D0 directly unless PE's guard acts (`test/regen_sim_test.c`; plan `staging/v347_feedback/w4_regen/PLAN.md`). Text ids: the v3.47 chain MSG_OM0 = MSG_FN0 + 16 (11 of 16), MSG_RG0 = MSG_OM0 + 16 (6 of 16), MSG_SB0 = MSG_RG0 + 16 (reserved, unused), MSG_END = MSG_SB0 + 16. No new exe patch site, no save format change. Tests: 36 native tests, logcheck_test, fontlib_test, check_exports (199 = SH4R_StickAmount) pass.
- v3.46.0 (2026-11-24, main rig, SH4 Refined Session 22; built in a sandbox by worker W1 OM346; Codex: 6 inspections, 5 findings fixed, the sixth APPROVED; installed 2026-11-24, md5 1614EA6916F02D552F7A20F67D637629, not yet tested in game): **Stick Movement** (the user's queue item 2: 360-degree movement; the user's answers of 2026-11-24): Option 5 > Player > Stick Movement Off / Direction / Analog below Movement Speed (`[Input] OmniMove`, default Off, live; hidden `OmniDeadZone=0.30`). One hook at 0x5543C0 (InputPollPad's call of InputNormalizeSticks 0x553D20) writes the real left-stick vector into gPad.stick[0].x / y (0x1236C44 / 0x1236C48) in gameplay only, |v| <= 1; strength / angle (the menus' direction bits) are never touched. Raw axes: GetDeviceState on the game's joystick or the AxisEvent call sites 0x407B15 / 0x407B28 (captured, then on to whatever 0x406F50 holds: Playable Eileen's PadAxis), picked and watched by a self-check against the game's own direction bits. The three sites are one byte-checked set with rollback (SH4WM_Patches), free of cheat-table spans (ct_conflicts --span) and Playable Eileen's 271 parsed patch ranges. Texts: a new MSG_OM0 block (11 of 16 ids). Logs: install, mode changes, bindings, `omni: self-check ...`, rate-limited anomalies; logcheck's "stick movement" line. Tests: `test/omni_sim_test.c`. Plan: `staging/v346_feedback/w1_omni/PLAN.md`.
- v3.45.0 (2026-11-24, main rig, SH4 Refined Session 21; md5 19D10073; installed 2026-11-24 with `staging/v345_feedback/install_v345.ps1`, the game closed; not yet tested): the first items of the user's queue (`staging/QUEUE.md` 5, 7, 8, 9); two Opus workers, merged by the host. **Texts:** Movement Speed's description says "the player" (Henry or Eileen). **Depth of field strength** Low (new, gentler: half-size blur only, from 2.2x the player's depth) / Medium (= v3.44's Low) / High; the user's v3.44 Low migrates to Medium once (marker `[ScreenEffects] DofStrengthV=345`; a failed ini write never changes the meaning of what is on disk). **GPU read back** (the pixel checks, the depth probe) through a lockable render target first (v3.43's pause-picture route that worked under dgVoodoo2 Direct3D 11), then the image surface; one log line names the working path, or turns the checks off ("not probed: no read back" once per room); a transient failure retries the room's probe (3 tries). **Log:** the items settings line only when a value changes. Three Codex inspections, 6 findings fixed (`PLAN-REVIEW-LOG.md`). Rollback: `Mod Backups\winmm.dll.v3.44.0.before_v3.45.0` (351B559A); real tree `*_v3.44.0.*.bak`. QA list: `staging/v345_feedback/QA_v345.md`.
- v3.44.0 (2026-11-23, main rig, SH4 Refined Sessions 20 / 21; md5 351B559A; installed 2026-11-23 00:13 with `staging/v344_feedback/install_v344.ps1`, the game closed; it also set `[Textures] MenuFont=EBGaramond` (the user's choice); **user-verified 2026-11-23: all 10 QA items pass**; the user's next requests: `staging/QUEUE.md`): the fixes from the user's v3.43 QA (`staging/v344_feedback/USER_FEEDBACK_v343.md`); four Opus workers, merged by the host. **Shaders rebuilt from proven ps.1.1 forms:** the soft-shadow composite and the colour grade used the `1-x` source modifier, which GOG's d3d8 translator accepts and then draws as no shader (soft shadows pitch black at any Darkness, grading unchanged); a start-up lint (`RfShaderLint`) checks all 23 shaders in use and a pixel check reads a test draw back (hidden `[ScreenEffects] PixelCheck`); Bloom Very High. **Depth of field in play:** its matrix was not 16-byte aligned for the game's SSE helper (every call faulted); its rows moved onto the Screen Effects page, previews lighter. **Ghost Teleport:** Run + Action, Richard's sound and see-through fade (0.5 s out, warp, 0.5 s in), a forward warp with no enemy near; refusals leave Action to the game; a menu-held press never pairs. **Menu Font** row on Option 1 (live switch); Renderer no longer offers Direct3D 11 (16-bit) (a live 16-bit conf is replaced by Direct3D 11 at start). Font tool: per-glyph budgets for untrusted fonts (`test/fontlib_test.py`). Six Codex inspections: 7 findings fixed, 3 by design (`PLAN-REVIEW-LOG.md`). Rollback: `Mod Backups\winmm.dll.v3.43.0.before_v3.44.0` (89E1ED15) + `SH4Refined.ini.before_v3.44.0`; real tree `*_v3.43.0.*.bak`. QA list: `staging/v344_feedback/QA_v344.md`.
- v3.43.0 (2026-11-22, main rig, SH4 Refined Session 20; md5 89E1ED15; installed 2026-11-22 with `staging/v343_feedback/install_v343.ps1` (the dll + a new `fonts\` folder), the game closed; **QA'd by the user 2026-11-22: Ghost Teleport, Quick Charge, bloom, lens pass; Darkness, colour grading and depth of field in play do not show -> v3.44**): the user's orders after the v3.42 QA (`staging/v343_feedback/USER_FEEDBACK_v342.md`); six Opus workers and an Opus merge agent. **Ghost Teleport** (Option 5 > Special; `[Gameplay] GhostTeleport`, default Off): hold Run standing still 0.4 s -> behind the nearest standing enemy within 2500 u, on a spot validated like Enemies Follow's and inside this room (its collision box and door lines; refused when the room's edges are not fully known), 3 s cooldown, Richard's warp sound + Echo smoke; no exe patch. **Quick Charge** 1.5x / 2x / 3x replaces Tireless (Option 5 > Player; 0x544D6E in the charge-rate function 0x544D40; Playable Eileen's melee charges scale through it; export `SH4R_QuickCharge`; a stale `Tireless=` line is removed once). **Depth of field** (Option 3 > Hide Icon, Enter: its page with a split preview; `[ScreenEffects] DepthOfField` Off / Cut-scenes / Always, `DofStrength` Low / High): depth slices in the stencil before the light grid (hook 0x4EFBCA), a once-per-room depth probe. **Bloom / Colour Grading (PS2, Otherworld, Apartment, Bleach) / Otherworld Lens** rows on the Screen Effects page (post.inc; bloom reset hook 0x402DF4). **HD font** (`[Textures] MenuFont` = Stock / EBGaramond / CormorantGaramond / CrimsonText, ini only, read at start): OFL glyph sheets drawn into the game's glyph cache (hook 0x5567F0); all game text. **Darkness** 100 / 85 / 70 / 55 / 40 / 25 % (the in-play value is read back and logged). Five Codex inspections (10 findings, all fixed: 7 in Ghost Teleport, fault-path restores in DoF / post, fonts packaging). Rollback: `Mod Backups\winmm.dll.v3.42.0.before_v3.43.0` (55365F2D), the inis / save\ `*before_v3.43.0`, delete `fonts\`. QA list: `staging/v343_feedback/QA_v343.md`.
- v3.42.0 (2026-11-22, main rig, SH4 Refined Session 20; md5 55365F2D; installed 2026-11-22 with `staging/v342_feedback/install_v342.ps1`, the game closed; **user-verified in game 2026-11-22: all pass except the shadow Darkness in play (the preview lightens, play does not) -> v3.43**): the user's v3.41 QA (`staging/v342_feedback/USER_FEEDBACK_v341.md`). **2D pixel alignment** (EG342): the game's 2D canvas edges sat exactly on pixel centres (clip +-1), so column 0 / row 0 dropped out through GOG's wrapper and dgVoodoo and the world showed through as a 1 px line in menus, memos, peepholes and the window view; Apply2D now shifts the 2D constants half a pixel up / left and BlackSides draws black quads past the screen edges (hidden `[Window] PixelAlign2D=1`). **Death Presence off in Room 302** (stages 1 and 0x0A; hidden `[ScreenEffects] DeathPresenceRoom302=1`). **Curse Meter = Off / On** (On = companion Eileen's ring; Refined never draws the player's ring, Playable Eileen v1.23.1 does; old On / Player -> Off, Both / Companion -> On). **Shadow Darkness** row on the Shadows page (100 / 85 / 70 / 55 %, default 85, `[Shadows] Darkness`, after Playable Eileen's ShadowDarkness; Stock and Soft; tints kept). **Echo Doubles / After Shock hidden in peep views** (the Death Presence gate's signals; drawing only). Codex: inspection 1 APPROVED. Rollback: `Mod Backups\winmm.dll.v3.41.0.before_v3.42.0` (F6723E0C) and the inis / save\ `*before_v3.42.0`. QA list: `staging/v342_feedback/QA_v342.md`.
- v3.41.0 (2026-11-22, main rig, SH4 Refined Session 20; md5 F6723E0C; installed 2026-11-22 11:20 with `staging/v341_feedback/install_v341.ps1`, the game closed; **user-verified in game 2026-11-22: all 10 QA items pass**): the user's v3.40 QA and later issues (`staging/v341_feedback/USER_FEEDBACK_v340.md`, screenshots in `user_screens\`). Five Opus sub-agents in sandboxes, merged by the host. **Shadow Edge = Stock / Soft** (SH341): Soft is 12 px at 720 lines (24 at 1440p, twice v3.40's Softer; old Soft / Softer read as it; hidden `SoftPx`, `SofterPx` gone), built so no draw stretches more than 2x (down in 2x steps, a Kawase blur at the smallest level, up in 2x steps; the v3.40 blocks came from one 4x bilinear stretch), a core curve keeps the inside as dark as Stock. **No pause hitch:** the Screen Effects page's pause picture stays a GPU copy (the 20-40 ms read back at every pause since v3.22 is gone; F7 still reads back), the Shadows preview's targets are kept across pauses and released before Reset / on a new device / when the address space is low. **Ghost copies** (EN341): where the game's room rule (0x464880) refuses ghosts, a ghost copy (types 0B / 0C / 0F) is made without its record at full HP, as the game's own ghosts are (the 4 ANOMALY lines of the v3.40 run); a pinned / downed one stays in the ledger. **Window views 4:3** (VW341): an inline hook at 0x54CC10 (the window views' draw) adds black sides at wide aspects (hidden `[Window] WindowView43=1`); BlackSides' rounding exact (a 1 px column of world showed at 6.8 % of sizes, e.g. 1366x768); Borderless placement checked; an edge-cover diagnostic logs which window covers a strip of the game's edges (hidden `EdgeCoverLog=1`; the user's thin edge lines are not drawn by the game, Refined or GOG's wrapper). **Death Presence gate** (DP341): hidden while the room geometry is off ([0xFD6C78] = 0: peepholes, door scope, wall hole, window views, Water Prison peeps), while peeping or a warp is pending; after a room change it waits until the player is still 0.30 s and re-anchors (the wisps stood in walkways). **Curse ring hand-over** (CM341): with Playable Eileen v1.23.1's `SH4PE_DrawsPlayerCurse()` = 1, Refined draws no player ring (Player greyed, Both = companion only, her ring kept off PE's); older PE: as before. Three Codex inspections (2 + 3 findings fixed; the third APPROVED). Rollback: `Mod Backups\winmm.dll.v3.40.0.before_v3.41.0` (F0C64B8C) and the inis / save\ `*before_v3.41.0`. QA list: `staging/v341_feedback/QA_v341.md`.
- v3.40.0 (2026-11-22, main rig, SH4 Refined Session 19; md5 F0C64B8C; installed 2026-11-22 with `staging/v340_feedback/install_v340.ps1`, the game closed; **installed; QA'd by the user 2026-11-22: preview, Reset, Stock pass; Soft / Softer too blocky -> v3.41.0**): the user's v3.39 list (`staging/v340_feedback/USER_FEEDBACK_v339.md`). **Shadow Edge** (`[Shadows] Edge` = Stock / Soft / Softer, default Stock): the stencil shadow mask (ShadowCompositeDraw 0x55F660) is downsampled 1-3 times with a 4-tap ps.1.1 box and drawn back once (A = x + x(1-x): the inside as dark as stock, Playable Eileen's ShadowDarkness included); hooks 0x55FE43 and 0x402E0D (render targets released before Reset); Soft about 3 px / Softer 6 px of edge at 720 lines, scaled; a hidden Shadows page (Enter on Option 3's Realtime Shadow) with a split-view preview of the paused scene (or a figure over preview.dds from the title). **Logs:** the previous session's log copied to `logs\` as "SH4Refined <date> <time> v<ver>.log", the newest 8 kept (hidden `[Log] Keep`), only files with Refined's own log header ever counted or deleted; logcheck.py prints a per-session / per-renderer table. **graphics\install** (merged from SH4 Graphics Plus): install_dgvoodoo.ps1 defaults to d3d11_flipdiscard (`-Variant`). Two Codex inspections (3 log-rotation findings fixed; the second APPROVED). Rollback: `Mod Backups\winmm.dll.v3.39.0.before_v3.40.0` (6319A907) and the inis / save\ `*before_v3.40.0`. QA list: `staging/v340_feedback/QA_v340.md`.
- v3.39.0 (2026-11-22, main rig, SH4 Refined Session 19; md5 6319A907; installed 2026-11-22 with `staging/v339_feedback/install_v339.ps1`, the game closed; **installed; QA'd by the user 2026-11-22: all 6 pass**): the user's v3.38.1 feedback (`staging/v339_feedback/USER_FEEDBACK_v3381.md`). Option 4 loses V-Sync / Anti-Aliasing / Anisotropic Filtering (GOG's Galaxy "DirectX Settings" tool, dxcfg.exe, sets them; old `[Graphics]` values are logged and ignored) and Performance (hidden `[Graphics] PerfOverlay=1`; the `perf:` log lines stay). New **Renderer** row (Direct3D 11 / Direct3D 11 (16-bit) / Direct3D 12 / Stock): swaps in SH4 Graphics Plus's pinned dgVoodoo configs from `dgVoodoo_variants\` by its manifest (md5s and the allowed d3d9.dll pins read from it), via dgVoodoo.conf.new + ReplaceFileA, at the next start; Custom / unavailable states greyed; renderer.inc reviewed by Graphics Plus. New **Opening** row (`[Window] Opening` = Full / Skip Logos (default) / Fast): no KONAMI / KCET logos and notice (0x42CD6A), Fast also no first-start movie (0x42CDA2) and the short title intro (0x42CDAB). `[ScreenEffects] PreviewKey` defaults to F7 when absent (the installer set this user's None to F7). Five Codex inspections (all findings fixed, one rejected with evidence). Rollback: `Mod Backups\winmm.dll.v3.38.1.before_v3.39.0` (BF12CA9A) and the inis / save\ `*before_v3.39.0`. QA list: `staging/v339_feedback/QA_v339.md`.
- v3.38.1 (2026-11-21, main rig, SH4 Refined Session 19; md5 BF12CA9A; installed 2026-11-21 14:41 with `staging/v338_feedback/hf3381/install_v3381.ps1`, the game closed; **installed; QA'd by the user 2026-11-21: "Everything looks good!"**): HF3381, the user's v3.38.0 QA ("the curse meter does not auto hide"): Hide Gauge's own 5 s timer was woken by every 0.4 % drift of the curse fill and of the target notch / next mark, so it never ran out; now only events wake it (the meter appearing, a stage crossed, a mark appearing, the left-alone glyph appearing or its danger changing, the lock changing); a move still brightens the meter. Codex APPROVED (no findings). Rollback: `Mod Backups\winmm.dll.v3.38.0.before_v3.38.1` (72E5F47F) and the inis / save\ `*before_v3.38.1`.
- v3.38.0 (2026-11-21, main rig, SH4 Refined Session 19; md5 72E5F47F; installed 2026-11-21 12:11 with `staging/v338_feedback/install_v338.ps1`, the game closed; **installed; QA'd by the user 2026-11-21: everything works but the curse meter's fade (v3.38.1); one crash at 14:07 in the sound path through Playable Eileen's dsound.dll**): the user's v3.37 feedback (`staging/v338_feedback/USER_FEEDBACK_v337.md`). Three Opus sub-agents in sandboxes, merged by the host. **Curse Meter:** both rings readable on dark backgrounds (backing, bone hairlines, lighter track, brighter fill / thorns; companion Eileen's target notch, next-spawn mark and left-alone glyph no longer halved while she is away; rest strength 0.70), "PLAYER" under the player's ring and "EILEEN" under the Companion-mode ring (the stroke font gained a Y), fading with the vanilla Option 3 Hide Gauge (0x1091AD9; an own 5 s + 1 s timer, woken by stage / mark / glyph / lock changes, held while Possessed). **Performance readout:** moves below the health HUD (Playable Eileen's [HUD] box or the stock gauge) when they would overlap. **Enemies Follow:** no timing give-ups; pursuits last the world visit (32 followers, a 128-entry copies' ledger), admitted only while the room has space (records, tasks, actors, bones, hit shapes 24 free, IK 64 free; unknown counts wait); a monster that came near, was hit / hit you or saw you follows at any distance; every ghost in the room follows; the story ghosts and their copies follow (Andrew not in stage 4 before 0x2DC; Richard / the Patient not at room 5:3 before 0x3BF); Wheelchairs out of Copies and Follow (a wandered one goes home); `DownKeep` defaults to 0 (the game's own downs: a hit halves the countdown; no bar dip). **Supply Drops:** Off / Rare / Normal / Abundant (Rare x0.5, Abundant x2; "Generous" reads as Abundant) with the user's class table (Normal: weak 11.45 / 0.5 / 0.05 %, strong 9.9 / 2 / 0.1, ghost 7.75 / 4 / 0.25, named ghost 5 / 4 / 3, Walter 7 / 4 / 1 % common / uncommon / rare), one draw per kill / full down, no falloff on repeated ghost downs, shares common 1:1:1, Medical Kit 2 : Holy Candle 1, rare (Saint Medallion, Ampoule, Silver Bullets, Sword of Obedience) 1:1:1:1, the owned-gun rule kept, rare drops never made way for (`staging/v338_feedback/w3_drops/DROP_LIST_v338.md`). Menus and the title stay at 30 fps (researched: 60 would double their animations and the paused play time). Review: five Codex inspections (6 real findings in Follow fixed, 2 shown not to apply), the fifth APPROVED. 28 native tests pass. Rollback: `Mod Backups\winmm.dll.v3.37.0.before_v3.38.0` (1325CC95) and the inis / save\ `*before_v3.38.0`. QA list: `staging/v338_feedback/QA_v338.md` (12 items).
- v3.37.0 (2026-11-21, main rig, SH4 Refined Session 19; md5 1325CC95; installed 2026-11-21 08:14 with `staging/v337_feedback/install_v337.ps1`, the game closed; **installed; QA'd by the user 2026-11-21: No Clip Fly, Extended Toolbelt, Stackable Items, the pause Camera page and the Performance readout work**): the user's 12 requests after the v3.36.1 run (`staging/v337_feedback/USER_FEEDBACK_v3361.md`). Three Opus sub-agents in sandboxes, merged by the host. **Options:** Option 5 = No Clip, Curse Meter, Extended Toolbelt, Stackable Items, Item Shine, Player >, Enemies >, Special >, Exit (row positions recorded, not fixed); One-Hit Kill tops Player; the highlight keeps the remembered row at a page switch (OptionsPage +0x30 and 0x1092238 + 4*slot). **Drops:** 64 anywhere / 10 a room / 16 pool records kept free; pickups and fades free a place at once; drop numbers renumbered before the 12-bit wrap. **No Clip Fly:** its own speed (2.5x run when running, 1x run when walking, hidden `NoClipFlySpeed`), stick read via 0x4F8610 / 0x4F8660, stops in < 0.1 s. **Curse Meter:** Haunted from FitsFrom whatever PE's Fits says (it had vanished with Fits=0), first stage "Clear", "On" shown as "Player", Player / Both greyed and skipped without PE. **Extended Toolbelt** (CT 7310 v6) and **Stackable Items** (CT 7360 v3) ported as live Option 5 rows (`tools/inv337_gen.py` -> `src/inventory_cave.inc`, 46 byte-checked sites, all-or-nothing): the attack bug fixed (page-2 weapons, PE's 80h..84h included, no longer count as broken: 0x544D80 / 0x5387EB / 0x53DACE); Off moves the weapon in hand to page 1 and the rest of page 2 into the Storage Chest (stays On when it does not fit); the stack guards (0x463607 steal cap, 0x4F6A30, 0x4F5DB2) always in. Turn the cheat table's 7310 / 7360 off with Refined's on. **Pause > Extra Menu > Camera:** the 12 camera settings (changes show on resume: the pause picture is frozen); Option 6 Camera is a plain OTS / Game switch. **Presentation:** a Performance overlay row (fps, frame ms, 1 % low, Refined ms, memory) and the Direct3D chain in Display Mode's description. Review: one batched Codex inspection, also of v3.36.1 TL3361 (INV337-001 medium fixed: a Toolbelt / Stackable Off checks every site before restoring any; the re-inspection APPROVED). 28 native tests pass. Rollback: `Mod Backups\winmm.dll.v3.36.1.before_v3.37.0` (8D51ED9E) and the inis / save\ `*before_v3.37.0`. QA list: `staging/v337_feedback/QA_v337.md` (17 items).
- v3.36.1 (2026-11-20, main rig, SH4 Refined Session 19; md5 8D51ED9E; installed 2026-11-20 21:40 with `staging/v336_feedback/install_v3361.ps1`, the game closed; **installed; QA'd clean by the user 2026-11-21, 1 h 39 min of play, copies x4 and Follow**): Enemy Copies / Enemies Follow may use all of the task pool's slots (keep 40 free of all 400; v3.36.0 kept 40 free of the first 300) when Playable Eileen is v1.20.1+ (it reads the live slot count 0x553A72) or not loaded; an older or unreadable PE keeps the reserve; hidden `[Gameplay] CopiesAllTasks=0` always keeps it; one start-up line says which. Tag `v3.36.1 TL3361`; pools test 648 ok, all native tests pass. The user said "lift it now"; Codex reviews it in the next batch. Rollback: `Mod Backups\winmm.dll.v3.36.0.before_v3.36.1` (AFCA16CE).
- v3.36.0 (2026-11-20, main rig, SH4 Refined Session 19; md5 AFCA16CE; installed 2026-11-20 20:53 with `staging/v336_feedback/install_v336.ps1`, the game closed; **installed, not yet tested in game**): **the bigger game pools** (the read-only audit `staging/pool_audit/REPORT.md`; the user chose its top four). Two Opus sub-agents in sandboxes, merged by the host. **Task pool** 300 -> 400 slots in place, into the draw packet buffer behind it (10 constants, only before the exe's init; every Refined task walker uses the live count; hidden `[Gameplay] TaskPool=400`, max 448); Enemy Copies / Follow keep their 260-task limit until Playable Eileen reads the live count (0x553A72). **IK pool** 160 -> 512 (hidden `IkPool`). **Hit shapes** 100 -> 356 (a block in this dll linked after the game's shapes at every rebuild; hidden `HitPool=356`, 0 = stock) and a stomp on a body with no shape no longer crashes (0x544B11). **Enemy records**: a full table evicts Enemy Copies' records before real monsters' (hidden `CopyEvictFirst=1`). Per-room `pools:`, `hit shapes:` and `enemy records:` log lines. Review: one batched Codex inspection, also of the v3.35.1 hotfix (IK-001 medium fixed: the IK functions are checked before the counts are written; the re-inspection APPROVED). Rollback: `Mod Backups\winmm.dll.v3.35.1.before_v3.36.0` (F17B4D37). QA list: `staging/v336_feedback/QA_v336.md` (and `staging/v335_feedback/QA_v335.md`).
- v3.35.1 (2026-11-20, main rig, SH4 Refined Session 19; md5 F17B4D37; installed 2026-11-20 20:30 with `staging/v335_feedback/install_v3351.ps1`, the game closed; **installed, not yet tested in game**): **hotfix**. v3.35.0's pursuing-victim check read a ghost's HP through the game's EnemyHpGet 0x44FD30, which faults on a victim the game has just re-made after it followed you (no HP block yet: fld [NULL] at 0x44FD3A); the fault turned Enemy Copies, Enemies Follow and Revive off for the session (the user: "copies not working", "ghost only followed me once"). Now a safe read of the HP block (work +0x34; none = not down). Tag `v3.35.1 HF3351`; enemies test 186 ok, all 23 native tests pass. Codex reviews it in the v3.36 batch (the user's choice). Rollback: `Mod Backups\winmm.dll.v3.35.0.before_v3.35.1` (CF339CB1).
- v3.35.0 (2026-11-20, main rig, SH4 Refined Session 19; md5 CF339CB1; installed 2026-11-20 20:07 with `staging/v335_feedback/install_v335.ps1`, the game closed; **installed, not yet tested in game**): **the user's v3.34.0 QA**. Four Opus sub-agents in sandboxes, merged by the host. **Ghosts**: a victim that followed or roamed in (mode word 4) follows again; victims are taken from 1500 and keep pursuing at later doors until downed, pinned, a load, the world left or 3 doors missed; a victim arriving after the room load gets its Enemy Copies; every full down of a ghost or ghost copy rolls a Supply Drop (5 % Normal / 10 % Generous, less for the same ghost again); a hit never shortens a downed ghost's revive countdown. **Supply Drops**: anyone's kills roll (Eileen, Echo shadows, After Shock, monsters; never cutscene / story removals); no world-visit budget; every drop fades 120 s of play after it lands (age in its record, saved; blinks its last 15 s), caps 10 anywhere / 4 a room; Item Shine shines through any enemy lying down. **Pickup pool**: the game's 32 dynamic pickup records move to 128 in Refined's dll (saves still hold 32: pin Swords, the game's records, then drops newest first) and a full pool no longer crashes (guard at 0x4F3F74 / 0x4F429C; hidden `[Gameplay] PickupPool=128`, 0 = stock pool, the guard always on). **Effects**: After Shock's blue glow is gone (arcs stay); every effect is made ready at a room's first frame (a bug threw away Echo's warm-up); an auto frame budget (6 ms at 60 fps; hidden `[Graphics] EffectBudget=1`, `EffectBudgetMs=6`) thins Echo, After Shock's copies and Death Presence's fog when a frame runs over and brings them back after 3 s calm; the log names every frame's top costs. **Curse Meter**: a target notch on companion Eileen's ring (hidden while PE's LockCurse holds it), a next-spawn mark and a 'left alone' sign; the stage name uses the small ring's compact stroke font. Start-up: PE's accepted hooks are no failed byte check. Review: one batched Codex inspection (run 1 refused as too large; run 2 one finding, rejected: the globals are declared in gameplay.inc). Rollback: `Mod Backups\winmm.dll.v3.34.0.before_v3.35.0` (1DE57EE5). QA list: `staging/v335_feedback/QA_v335.md`.
- v3.34.0 (2026-11-20, main rig, SH4 Refined Session 19; md5 1DE57EE5; installed 2026-11-20 14:22 with `staging/v334_feedback/install_v334.ps1`, the game closed; **user QA'd items 1-5 on 2026-11-20**: the Extra Menu, Supply Drops and the outfit row are back): **the user's v3.33.0 QA**. Three Opus sub-agents in sandboxes, merged by the host. **Playable Eileen coexistence**: PE hooks 0x5736D0 / 0x553AA0 / 0x4F55E0, functions Refined only calls; v3.33 refused them when PE patched first and left the Extra Menu, Supply Drops and the Eileen outfit out; Refined now calls through PE's hooks (a jmp into PE's dsound.dll listed in its SH4PE_Patches). **Echo** under Spirit Vision: dark specters (near-black, smoky, faint cold rim, pale eyes). **After Shock**: the cloak's lit outline strips are gone; it fades out at her edges. **No Clip**: a placed companion's feet land at once (the foot IK targets 0xFCE020 / 0xFCE070 move with her). **Enemies**: Revive brings back a story room's released event dogs (Subway 2:2 after their event; hidden `ReviveEventDogs=0`); Enemies Follow takes copies too (hidden `FollowCopies=0`), FollowMax 6 (1-8), a followed copy lives on in its new room; a wanderer is put on the floor (the log's "type 00" was a log bug). Review: one batched Codex inspection (run 1 blocked for missing PE / decompilation evidence, run 2 two low findings fixed: Echo's warm-up of the specter pass, README limits). Rollback: `Mod Backups\winmm.dll.v3.33.0.before_v3.34.0` (87CC3EEC). QA list: `staging/v334_feedback/QA_v334.md`.
- v3.33.0 (2026-11-20, main rig, SH4 Refined Session 19; md5 87CC3EEC; installed 2026-11-20 02:13 with `staging/v333_feedback/install_v333.ps1`, the game closed; **user QA'd 2026-11-20**: most items pass; the Extra Menu and Supply Drops were left out by a start-up race with Playable Eileen's hooks, fixed in v3.34.0): **the user's v3.32.0 QA**. Seven Opus sub-agents in sandboxes plus an Opus merge agent (`staging/v333_feedback/merge/MERGE_NOTES.md`). **Log**: levels (`[Log] Level` 1 normal, 2 verbose = default during QA, 3 trace), `ANOMALY:` lines for states a feature should never reach, a session header, room / load / session summaries, `tools/logcheck.py` to read it. **Echo**: with a weapon out every calm shadow stays see-through (the draw budget dropped shadows 2 / 3's depth pass); under Thermal / Spirit Vision / Night Vision every shadow is see-through in the filter's theme. **After Shock**: copies in line again (hidden `AfterImageFan` 0; 1..200 = the v3.32 fan); the orbs are one skin-tight electric cloak, the lightning kept. **No Clip** Off / On / Fly: On passes through everything horizontally while floors and stairs carry her (stairs fixed); Fly (only with an OTS camera) moves where the camera looks; companions are placed beside her ~2 s after a wall blocks them. **OTS camera** moved in from Playable Eileen (`camera.inc`; Option 6 > Camera replaces Save Bank; 19 `[Camera]` keys imported once from PE; Refined defers to PE's camera while PE still hooks 0x4F7080). **Menus**: Quick Save / Quick Load no longer flicker grey (music streams were counted as busy file reads); value arrows never overlap long values. **Enemies**: the first Sniffers (event dogs) are copied (hidden `CopyEventDogs=0`); Enemies Follow loads a follower's files in the next room when memory allows (hidden `FollowLoadFiles=0`); Supply Drops roll fresh on every life of a revived monster, every roll logged. New exe sites 0x4F7080, 0x50629D, 0x4CEA5A, 0x4CEA76 (camera), 0x525802 (grey Fly). Review: plan approved by Codex in round 2 (camera load order fixed); inspections 2 (follower file list and drop-life table fixed; two findings rejected on evidence). Rollback: `Mod Backups\winmm.dll.v3.32.0.before_v3.33.0` (69FC7296). QA list: `staging/v333_feedback/QA_v333.md`.
- v3.32.0 (2026-11-19, main rig, SH4 Refined Sessions 18-19; md5 69FC7296; installed 2026-11-19 22:29 with `staging/v332_feedback/install_v332.ps1`, the game closed; user-verified in game 2026-11-20: 14 of 16 pass, No Clip a soft pass (stairs, props), the companion warp failed for Eileen; fixes in v3.33.0): **the user's v3.31.0 QA**. Six Opus sub-agents in sandboxes plus an Opus merge agent (`staging/v332_feedback/merge/MERGE_NOTES.md`). **Echo**: calm shadows stand 1.5x farther and see-through, solid while you strike; no crouch / reach leak on pickups (stairs); a tight coat of black flames with purple wisps, softer eyes. **After Shock**: visible from behind (copies fan sideways on the view axis; hidden `AfterImageFan`), no broken trail (copies spaced along the travelled path), a speed-driven aura. **Drops**: copies roll Supply Drops like their type (a session ledger, reset only on a world reset or a load; hidden `CopyDrops=0`). **Curse Meter** value Both (a small EILEEN ring beside yours; grey with LOCKED while PE's `[Ghost] LockCurse=1`). **Item Shine** through downed ghosts (hidden `[Items] ShineThroughDowned=0`). **No Clip** (Special row, `[Gameplay] NoClip`, Off by default): walls, doors and props pass, floors and stairs carry her (props under 60 are stepped onto), stock collision near walk-in exits / grabs / Room 302, companions placed beside her after 3 s blocked. **Pickup** under a downed ghost within 3 s of attacking it (exe 0x53890B; hidden `[Gameplay] DownedPickup=0`). **Extra Menu** opens / closes in place from the pause, the world frozen (hidden `[ExtraMenu] Seamless=0`). **Options descriptions** fit 4:3 (wrap, tighter, else shortened; hidden `[Window] DescFit=0`). Review: plan approved by Codex in round 2; inspections 3 (2 + 1 granted; findings: copy ledger reset, DescFit switch fixed; No Clip low-prop probe kept by design; a review-sheet caption). Rollback: `Mod Backups\winmm.dll.v3.31.0.before_v3.32.0` (D32623EB). QA list: `staging/v332_feedback/QA_v332.md`.
- v3.31.0 (2026-11-18, main rig, session 18b; md5 D32623EB; installed 2026-11-18 23:39 with `staging/v331_feedback/install_v331.ps1`, the game closed; user-verified in game 2026-11-19 (all pass, Henry skipped)): **the user's v3.30.0 feedback** ("Marvelous work!"). Built by three Opus sub-agents in sandboxes plus a frame-rate change by Claude, merged by an Opus merge agent (`staging/v331_feedback/merge/MERGE_NOTES.md`). **Echo Doubles**: while you place a Holy Candle or pick up an item the shadows replay your last attack at a nearby enemy (else they stand idle); one large black flame per shadow (8 sprites, was ~62); the eyes follow the head; a per-shadow anchor heading, so spins and idle turns no longer swing them round you; cost cut (3 body redraws a frame instead of 9, no face-morph re-upload per redraw, a guard reacting to one slow frame; v3.30 logged 10-22 ms spikes with 3 shadows at 180). **Enemy Copies**: downed / pinned ghost copies come back as they were (kept by Refined, never saved; copies also made beside a downed original), flying Hummers' copies keep their height, and the four story ghosts (Cynthia, Jasper, Andrew, Richard) are copied too (never Andrew in stage 4 before his pin event or Richard in room 5:3; copies kept out of saves through 0x465438 and out of the rare drops; hidden `CopyStoryGhosts=0`). **Depth**: in rooms where the game's light grid (0x4EFBDC -> 0x433C70) overwrites the depth buffer, the player, her weapon and the companions are redrawn depth-only before Death Presence and Echo draw, so those effects no longer cover you (hidden `[ScreenEffects] PlayerDepthRestore=0`). **Frame Rate**: Options offers 30 / 60; 120 / 180 need the hidden `[Window] HighFrameRates=1` (a saved 120 / 180 runs as 60). Review: plan approved by Codex in round 2; inspections 6 (2 + 4 granted by the user; 1, 2, 1, 1, 1 code findings and one docs finding, all fixed). Rollback: `Mod Backups\winmm.dll.v3.30.0.before_v3.31.0` (46D1396F). Plan / reviews / renders: `staging/v331_feedback/`.
- v3.25.0 (2026-11-16, main rig, session 13b; md5 67f4b799; installed 2026-11-16 with the game closed; **confirmed in game by the user 2026-11-16**: glyphs, blood colours, revert, movement, medallion, regen; One-Hit Kill with melee and the vanilla guns, not with Playable Eileen's SMG (accepted)): **The user's list after v3.24.0.** (1) Button Config: a DualSense's Create / Options buttons show glyphs (Options = three bars, Create = a drawn pill with three rays, both in the PlayStation disc style; the user approved the preview); DualShock 4 / 3 keep their words (DS3's Options button now reads "Start"); with PadNames=PlayStation forced, the Sony model is still detected. (2) Blood Colour: 10 values through the game's three blood readers (splash switch 0x43008E, burst switch 0x430AC3, pool colour tables via 0x4777C4 / 0x4777EE; Red / Green / Purple unchanged), a bounded getter / input for an edited save's value, and a fallback that keeps a saved colour at 0..2 whenever the readers or the 10-value row are not in. (3) Option 5: Player / Enemies hidden pages (generalised page requests) with Movement Speed (0x534BEB travel step, 0x5352C2 playback, Room 302's 10.5 operands), Medallion Durability (wear calls 0x53AB76 / 0x53AB92), Regeneration (Playable Eileen's own ini keys, only while it is loaded) and One-Hit Kill (the hit-record site 0x44F605, the player's attack object 0x10A4FE0 only). `[Gameplay]` is read again while the game runs. All sites free of cheat-table AOBs (ct_conflicts 0) and Playable Eileen patches, each feature installed all-or-nothing with rollback. Review: plan approved by Codex in round 6 (11 findings, all taken; one extra round granted by the user); code inspected 7 times by fresh Codex sessions (the user asked for 5 extra), the last with no code finding. Rollback: `G:\SH4 Mod Archive\2026-11-14\Mod Backups\winmm.dll.v3.24.0.before_v3.25.0` (8d2ee44f). Plan / reviews / RE: `staging/feedback_v325/`. Special mods (Dark Fog Aura, After-image stun as by the Stun Gun, Echo double, Enemy multiplier x2) are planned for later rounds.
- v3.21.0 (2026-11-15, main rig; md5 d160447d; seen in game 2026-11-15 (the user's 17-item list)): **The user's v3.20 test feedback, two lists.** (1) Option 1's submenu arrows ride with their page during a page turn (they used the rest rect; now + the page draw's slide offset, 0x525990 arg 5). (2) The Screen Effects preview filter runs at the page backdrop (0x526A53) under the page text, so the text stays readable under every filter (Spirit Vision outlined it); gate: the committed page or the running slide's target. (3) Spirit Vision auras rebuilt as soft camera-facing sprites from a procedural atlas: hauntings crackle with red / black veins, dark mist, fluttering ash and embers, slow blood blobs; candles and the medallion pulse a gold glow with floating orbs; no columns, floor rings or spheres; a near-camera fade (Room 302's candle no longer fills the screen). (4 / 5) Dead enemies (the game's EnemyIsDead 0x45BB90) cool in Thermal over `ThermalCoolSeconds` (30) and lose their soul fire over `SoulFadeSeconds` (10), with a few embers. (6) The trapped Cynthia behind the Subway train door (event record 0x62A6FC, her own draw call at 0x4EE376, before the hooked model loop) is hot in Thermal. Subtitles: Fade In fades the whole line in (and out when Exit is None); Tremble removed; Glitch cuts letters into shifting, stretched blocks with red / blue fringes; Flicker replaced by Blood Drip (letters soak red from the bottom, drips run down); Typewriter slower (14 letters / s) with a cursor. Plan / reviews / notes: `staging/feedback_v321/`.
- v3.20.0 (2026-11-15, main rig; installed md5 b3e5ca79; Typewriter, Wave, Embers, Paper Peel, Burn Out seen in game): **Animated subtitles.** Two rows on the Subtitle Style page: **Effect** (None, Typewriter, Fade In, Tremble, Wave, Glitch, Flicker, Embers) and **Exit** (None, Fade Out, Burn Out, Paper Peel); `[Subtitles] Effect` / `Exit`. Per-letter animation at the existing glyph hooks (shadow, glyph and movie triangles), embers through the game's rect queue, exits replayed from a copy of the ended line (string, place, colour, alpha) at the 0x4EFBFD hook, a pause-aware clock (movies on real time), the page preview on a 4 s cycle. Plan / notes / reviews: `staging/subtitles_v320/`.
- v3.19.0 (2026-11-15, main rig; installed md5 6b90a64b; arrows, long values, monitor names and the Grey conversion seen in game): **Subtitle colours, monitor names, submenu arrows.** Subtitle Colour: White, Yellow, Cyan, Green, Red (replaces Grey; an old Colour=Grey reads as Red), Pink, Orange, Amber, Ice Blue. Option 4 Monitor: the primary first, then by Windows' display number, named Main / Secondary / Tertiary / Quaternary Display. The game's own right arrow (0x523620) follows the labels of Brightness, Gamma, Button Config, Subtitles and Noise Effect (rows that open a sub-screen), placed with the game's text metrics (font [0x1261B98], widths at +0x10, kerning at +0xF0, letter spacing [0x1261C20]); their descriptions no longer say "press Enter". Values longer than 14 characters (the monitor names, Night Vision Mono) move their column left like the game's Dolby Digital row (hooks 0x52570E draw, 0x525CE3 mouse rects) so both arrows stay inside the menu and clickable. Plan / build notes / reviews: `staging/menus_v319/`.
- v3.18.0 (2026-11-15, main rig; installed md5 1953bcc4; pause filter, auto-capture, page backdrop, Rain, Scan Roll, colours and the night-vision highlights seen in game): **Screen Effects v3.** Noise Colour row (a one-constant-per-instruction ps.1.1 that keeps the darkening neutral and pushes the brightening toward the colour; Stock type + a colour draws a replica of the game's grid); Noise Types Scan Roll and Rain; the grain previews on the Screen Effects page (level 48 x strength; 0x567040 only after 0x5736D0(0xF000F1DF)); `preview.dds` behind that page through a hook at 0x526A53 that skips the Options hallway picture (0x524010, texture F000F1E8) while ours is drawn, and an auto-capture of the clean pause picture ([0x13DC3A0] read back before the filter: an image surface, else a lockable non-multisampled render target under MSAA; `Interp_OwnBegin/End` keep interpolation out); the filter stays behind the pause / Extra / Playable Eileen menus and the quit prompt (overlay snapshot after 0x4F8BA0 in QsTick, whose dispatcher is now always installed); Noir removed; the night visions add a highlight pass. Plan / RE / build notes / reviews: `staging/screen_effects_v3/`.
- v3.17.1 (2026-11-15, main rig; installed md5 6315d713; footprints checked in game): **Spirit Vision / Thermal fixes from play testing.** Aura columns are drawn for every damage source, also when the player is above or below it (only the floor rim keeps the +-400 rule). Thermal: any non-enemy model with a full skeleton (30+ bones: story characters such as Cynthia seen behind the train door) is hot; Gum Heads, Twin Victims, Sniffer Dogs (small paws) and Walter leave footprints. Footprints are now drawn before the frame's first character model, without the depth test (live: the game's depth buffer cannot be matched for floor decals, as Item Shine found in v3.3.2), so every character covers them and the camera angle no longer culls them; prints behind a collision wall are skipped (segment test 0x561CD0). Spirit Vision's depth now uses the camera's own projection (cam+0xA0 = 0x12E3ED0) x 1/128 and pulls its floor rims along the camera ray. Notes: `staging/spirit_vision/BUILD_NOTES.md`.
- v3.17.0 (2026-11-15, main rig; installed md5 ad24fc73; Thermal heat / footprints and the depth test seen in game; real ghost columns, portals, candles, medallion, Eileen's fit and soul flames not yet seen live): **Spirit Vision + Thermal living heat.** Ghost Vision is renamed Spirit Vision. Damage areas (snapshot 0x10A1EC0) are tracked from frame to frame and drawn as 3-shell swirling columns, centred on the source +-400 (the damage test's height; Room 302: feet up 350) with a floor rim; a downed ghost (class work+0x3C 9 / 0x0A) previews its area from its get-up countdown and grows it back over 1.8 s. Wall blood portals (task update 0x487650, no model) get a vortex in the wall plane; Holy Candles (holy snapshot 0x10A28E0) a gold sphere of their 400 reach, fading with the burn; medallion-repelled sources (rate 0) turn gold and a ward flares on the player; Eileen's fit source (R 800 at her point, state 0x13) is bile-coloured; living enemies get a soul flame at the chest bone (`[obj+0xC0] + 0x40 b`). At most 16 columns, 8 portals and 8 candles a frame. **Depth:** the depth buffer is valid and reversed at 0x4EFBFD; live probes showed GvXform's w is the view depth (fit against the view matrix 0xF4E720) and the stored depth matches the projection with the near plane / 128, so hidden parts draw dim (ZFUNC LESS) and visible parts full (GREATEREQUAL); hidden `[ScreenEffects] SpiritTest` 1-3 are the probes. Thermal: the player and the companions are hot; footprints every 40 units fade over 6 s. The ghost sprite fallback now sits at chest height (was knee height). Plan / RE / build notes / reviews: `staging/spirit_vision/`.
- v3.16.0 (2026-11-15, main rig; installed md5 191376a3; all filters rendered in game; ghost glow / damage rings and Thermal ghost hiding not yet seen live): **Screen Effects v2.** The hidden Subtitle Style / Screen Effects pages open and close with the stock sub-screen fade (row input returns 4 -> state 5, hook 0x526CD2; Cancel 0x5261B4; page keys act as Back). Filter Strength, High Contrast and Otherworld removed; each filter has one fixed strength. The filter now runs as a scene pass at 0x4EFBFD (inside the Item Shine hook, before Item Shine, the HUD and text), so interaction subtitles and the HUD draw on top unfiltered; the gameplay gate is mode [0x1236D4C] == 6 and sub-mode [0x1236D50] == 2 (the RE's 1 was wrong, found live); capture frames are still filtered after the post pass at 0x55AC29. New: animated Old Film, Noir, Night Vision (soft vignette), Night Vision Mono, Thermal (ghosts 0x0B-0x0F skipped at the model draw 0x52AEB5 / shadow 0x52AF95, wall ghosts and Room 302 hauntings pre-draw 0x5473A0, enemy ambient boost at 0x4273A6, Jasper's flame emitters at 0x47DFC3; gameplay only, cutscenes get the palette), Ghost Vision (lift / edge / grade ps.1.1 passes; ghosts collected at 0x52AEB5 and redrawn through 0x55C650 with a glow shader through a draw-call override at 0x4292EF / 0x429338, sprite fallback; damage areas from the snapshot at 0x10A1EC0 drawn as rings with the Item Shine projection), and a Noise Type row (Stock, Fine, Coarse, Colour, Film, VHS). Live ps.1.1 findings through GOG's wrapper: `_x4` with r0.a tricks and `texreg2gb` silently draw black / nothing, as do two-constant instructions; only tex / dp3 / sub / mul(_x2) / add(_sat) / mad / lrp with one constant each are proven. Ghost Vision tuned live (threshold 0.20, lift 0.35) to drop wall-texture speckle. Plan / RE / build notes / reviews: `staging/screen_effects_v2/`.
- v3.15.0 (2026-11-15, main rig; installed md5 d12c0969; all 9 filters verified in game): **Screen Effects page.** Enter on Option 3's Noise Effect row slides to a hidden page (the Subtitle Style pattern). Noise Strength scales the level the game passes to its grain grid and dust (0x432FBE / 0x432F9E -> 0x567040); filters are drawn right after the game's own full-screen post pass (0x55A260, called at 0x55AC24, which stays stock: it uses the stack slot above its arguments as scratch, found when a C wrapper drew black frames; the hook is a naked stub at 0x55AC29): the game's frame copy 0x4036E0, one hand-assembled ps.1.1 colour pass (grayscale / tint / saturation / contrast) and multiply-blended vignette / scanline textures, all device state saved and restored; gated to gameplay / real-time cutscenes and the page's own preview (same-frame stamp). Live finding: GOG's shader translator silently drew nothing for a ps.1.1 instruction that reads two constant registers (`mad_x2_sat r0, r0, c4, c5`, accepted by CreatePixelShader); split into two one-constant instructions. Phase 1: separate noise styles (TV static, VHS, chromatic jitter) are a later step. Plan / RE / build notes / reviews: `staging/screen_effects/`.
- v3.14.0 (2026-11-14, main rig; installed md5 581dc1e1; tested in game by log and by the user on a DualSense (all levels good); Codex plan approved, inspection finding rejected with evidence): **Vibration strength.** The PC game rumbles on / off only (one constant-force effect); GOG's dinput8.dll emulates it by sending `SendForceFeedbackCommand(0x80000000 | v << 8 | v)` (v = 32 / 33, every 100 ms) to its own device, written straight into the pads' motor bytes (13 % of the range). Refined hooks that vtable slot (22) on the game's DirectInput devices and scales both motor bytes for Medium (x 128 / 33) and High (x 255 / 33), keeping the 32 / 33 alternation; Option 2's Vibration row is a copy with four values (Off = cfg+0x0B 0; the level in the ini; yellow / prompt / No like the other rows). The DirectInput8Create hook now also installs with Enable=0 (the non-exclusive change stays Enable=1). Hidden test key: `[Input] VibrationTest=1`, F10 in gameplay plays the game's rumble 1. Logged live: Low `80002121`, Medium `80008080`, High `8000FFFF`; the 0.4 s rumble lasts 0.5 s at 180 fps (lengths are right). Plan / RE / reviews: `staging/vibration/`.
- v3.13.0 (2026-11-14, main rig; installed md5 94a162a0; tested in game; Codex plan + code approved): **Subtitle Style submenu.** Enter on Option 3's stock Subtitles row slides to a hidden Subtitle Style page (the stock Options handler's unused "slide mode 2", started by a row input returning 1; Back and the page keys return); Option 3's table is a copy whose Subtitles row also opens it (Left / Right unchanged). Subtitles are restyled at the message window's four layout calls (0x50C114, 0x50C0F7 state 8, 0x50B230, 0x50B324: colour, a box in the renderer's own rect queue, the glyph range recorded) and the glyph pass's quad calls (0x559B64 shadow, 0x559D34 glyph, 0x559C94 / 0x559CAE movie triangles: scaled around the block's bottom centre, outline ring with a vertex-budget guard); stale ranges are dropped by comparing their first / last glyph entries. All `E8 rel32` call rewrites; stock settings draw byte-for-byte as the game. Hidden test key: `[Subtitles] Test=1` shows a two-line subtitle on F11 in gameplay. Plan / RE / reviews / live shots: `staging/subtitles/`.
- v3.12.0 (2026-11-14, main rig; installed md5 8627e8c6; tested in game): **Option 4 V-Sync, Anti-Aliasing and Anisotropic Filtering rows; V-Sync-aware rate cap.** The three rows answer GOG's dxcfg.ini keys (`[general] vsync`, `[enhancements] antialiasing` / `anisotropic`) through the existing hook on the wrapper's GetPrivateProfileStringW; Default passes the real value through. Changed rows are yellow and make the exit ask; "Yes" applies V-Sync and anisotropic with one device reset (the wrapper re-reads its config at every Reset) and saves Anti-Aliasing for the next start (in-game test: switching MSAA by a Reset during play crashed GOG's wrapper at the next pause, ori_d3d8.dll+11E41), "No" keeps everything. Tested in game (live checks in `staging/graphics_rows/PLAN-REVIEW-LOG.md`): pass-through at Default, yellow / prompt / No / Yes, wrapper values read back, AA Off vs 8x edges, pauses after a Reset, AA at next start. With V-Sync in force (the row, or Default with dxcfg.ini `vsync=enabled`) gameplay is capped at the game monitor's refresh in windowed / borderless too (was: full screen only), in both frame modes; interp.c's own refresh cap is replaced by that one. Plan / reviews / live checks: `staging/graphics_rows/`.
- v3.11.0 (2026-11-14, main rig; installed md5 08a8181e; tested in game: Auto with a DualSense -> PlayStation, Xbox, Nintendo, PadGlyphs=0): **Button Config shows pad glyphs.** The pad column draws button glyphs of the detected family (Xbox, PlayStation, Nintendo; Kenney "Input Prompts" 1.5A, CC0) through the game's own inline-icon path: the icon table (0x5C5E98) is copied into the dll with 32 new icons (0x20..0x3F; six readers repointed) whose cells lie in the empty bottom half of the icon atlas (fontdata_ex.bin entry 0), written with LockRect at mip levels 0..3 only where that half is still empty or ours (a texture pack's art there is left alone and names are shown). The PC port never linked that atlas to its texture (every inline icon drew untextured); Refined links it when the record is exactly the stock one. Glyphs are 24 texels in 32-texel cells so they fit the row pitch; PlayStation Create / Share / Options and numbered buttons stay text. `[Input] PadGlyphs` (1). Art bake: `tools/gen_glyphs.py` (pure Python, deterministic) -> `src/glyphs_data.h`. Plan / reviews: `staging/button_glyphs/`.
- v3.10.0 (2026-11-14, main rig; installed md5 3fe0572c; tested in game): **Button Config shows readable pad names.** Option 2 > Button Config named pad bindings "Button04", "Ana Y-", "Pov U"; the row draw's table reader (0x51784B) now points at Refined's names for the connected pad family (`[Input] PadNames` = Auto / Xbox / PlayStation / Nintendo / Generic; Auto = Raw Input pads that GOG's goginput.ini remaps, or XInput; mixed families -> Generic). Also fixed: buttons 16-30 were labelled one too high and button 31 read "Move Forward"; the pad column moved from x 394 to 376 so long names fit; the screen's "Default" now sets GOG's own pad layout (it used old PS2-adapter values). Glyph icons are a later step. Plan / RE / reviews: `staging/button_config/`.
- v3.9.0 (2026-11-14, main rig; installed md5 bdc848a4; user-confirmed in game): **Eileen's outfit engine is built in.** The cheat table's Eileen Outfit v4 (entry 7100) is translated into `src/eileenoutfit.inc` at the same 9 sites (file-id remap 0x57342F / 0x5733F0, world list 0x4F3490, room loader 0x4F3514, model build 0x445BDB, her task 0x44ADE0, cutscene ids 0x4F1C16, cloth 0x4F2100, slot load 0x4FC62A); Playable Eileen's stage-list stubs at 0x4F3496 coexist as with the table. The Extra Menu's Eileen row (Default / Dress / Nurse) now works without Cheat Engine. The walking companion changes at once (`Instant`); room-scene Eileens (e.g. the Hospital bed, stage 7 room 10) change at the next room load. `[EileenOutfit]` keys. The table's 7100 stays off when Refined owns it (backup `SILENT HILL 4.CT.bak-before-refined-outfit`). Plan / reviews: `staging/eileen_outfit/`.
- v3.8.0 (2026-11-14, main rig; installed md5 307648cf, tested in game without Cheat Engine): **Save Banks are built in.** The cheat table's Save Banks v7 (entry 6040) is translated into `src/savebanks.inc` at the same 8 sites (save path 0x407D2D, catalog read 0x56F158 / 0x4FBEB0, catalog write guard 0x4FBDC0, list input 0x570F30, list draw 0x56FF20, list bar 0x56FDA0, delete 0x56F440). Bank 1 = `save\sh4.sav`, bank N = `save\sh4_bankN.sav` (same files as the table). Save and load lists show "Q/E L1/R1: Bank N"; Q / E / L1 / R1 switch at once. Before the first delete in a bank its file is copied to `save\backup\<file>.<YYYYMMDD-HHMMSS-mmm>`. Option 6 gets Save Banks (Off / On) and Save Bank (1-99, "-" while Off); both apply at once and follow the v3.5.1 rules. Not ported: the table's extra delete inputs (the save list's own Del still deletes). The cheat table stays off for 6040 / 6041 when Refined owns banks (table backup `SILENT HILL 4.CT.bak-before-refined-savebanks`). Plan / reviews / test: `staging/save_banks/`.
- v3.7.0 (2026-11-14, main rig; installed md5 6a6422b3, tested in game without Cheat Engine): **the pause menu's Extra Menu is built in; Option 6 (Extra Menu) switches its rows.** The cheat table's Pause Menu+ v11 (entry 6017) is translated into `src/extramenu.inc` at the same 8 sites (pause draw 0x4F8A30, mouse 0x4F8F2E / 0x4F8F5D, Down 0x4F8FAC, confirm 0x4F8FE9, cancel 0x4F8EC1 / 0x4F8ED3, item box 0x512235); its engine runs in the 0x4EF982 dispatcher shared with Quick Save. Rows: Storage Chest, Save Game, Load Game, Quick Save (Refined's disk quick save, inside the pause), Quick Load, Eileen outfit (only while the cheat table's Eileen Outfit hands its API through the new export `SH4R_Extra`), Back; the Playable Eileen row through its SH4PE_Menu export. Option 6 rows follow the v3.5.1 rules. `[ExtraMenu]` keys. The cheat table now recognises Refined: Pause Menu+ and its row switches stay off quietly (table backup `SILENT HILL 4.CT.bak-before-refined-extramenu`). Save Banks stay in the table (Stage B). Plan / RE / reviews: `staging/extra_menu/`.
- v3.6.0 (2026-11-14, main rig; installed md5 3d86a6a4, tested in game): **Quick Save (F5) / Quick Load (F9), kept after a restart.** The game's own snapshot (0x4FC0E0, a full save-slot image) is written with a 64-byte header to `save\quick\qs1..3.bin`: three rotating slots, a new save replaces an invalid or the oldest one, written to a temp file and renamed, "Quick saved" shown only when the file is on disk. F9 loads the newest valid one through the title-Load path (the cheat table's sa_restart, ported). Guards refuse menus / overlays, cutscenes, interactions, doors, fades, file loads, message boxes, death, Henry busy, the Building World elevator ride, the final stage 9, the hole tunnel, the Wall Man fight room (stage 0x0B room 7), the Subway Wall Men room, Room 302 views. Not with Ctrl / Alt / Shift (the cheat table's Teleport moved to Ctrl+F5 / Ctrl+F9). Hook: 0x4EF982 (first call of the game tick); keys polled from the HUD hook. `[QuickSave]` keys. After a quick load the title's Continue resumes the loaded game. Plan / RE / reviews: `staging/quick_save/`.
- v3.5.2 (2026-11-14, main rig; installed md5 8a2dcfc5, tested in game): **save titles name the world the save was made in.** The user: saving outside Room 302 named the save after the last world ("every save after the Apartment World is Return from Apartment World"). The slot title is a code picked at save time by 0x4D03C0, which only looks at how Henry last came back to Room 302; the hook on its only call (0x56F76F) asks the game's own 0x4D02E0 with the current stage [0xFD5A90] / room [0xFD5A94] in a world (stages 2-8, 11) and keeps the stock answer in Room 302, the hole tunnel, the final stage or anywhere without a title. In game: a save at stage 11 room 5 is "Return from Below Building World" (stock: "Return from Apartment World"); a Room 302 save keeps its stock title. `[Saves] WorldTitles=0` turns it off. Existing slots keep their titles until saved again. Plan / RE / review: `staging/save_names/`.
- v3.5.1 (2026-11-14, main rig; tested in game with v3.5.2): **all Refined rows behave like vanilla rows.** 1st / 3rd Person FOV, Frame Rate and Item Shine are compared with their values when Options opened: yellow when changed, the exit asks "Do you want to change the options?", Yes keeps, No puts them back (ini included; hook on the discard's `call 524320` at 0x526B8D). An Item Shine change typed into the ini from outside while Options is open becomes the new baseline.
- v3.5.0 (2026-11-14, main rig, session 9b; installed md5 79d6b9b7, agent-tested in game: `staging/options_pages/live/RESULTS.md`): **Options 4 (Presentation) and 5 (Gameplay); Options 1-3 stock again.** The user: move our settings out of the stock pages. Option 1 / 2 / 3 show the stock rows only (Resolution keeps its six sizes per shape: row 0's value table 0x654CCC). New Option 4 (Presentation): Display Mode, Monitor, Aspect Ratio, 1st Person FOV, 3rd Person FOV, Frame Rate (renamed from FOV First / Third Person). Option 5 (Gameplay): Item Shine. Option 6 (Extra Menus) is defined but not in the page cycle until it has rows. The menu object only has 3 page widgets, so the pages are virtual: logical page L in physical slot L mod 3, the target slot refilled just before each slide (free 5521E0, init 5521B0, rects 5525D0); hooks 525B7A (switch), 526439 (slide commit), 5262B2 (mode-2 guard), 523E4B (init: stock pages, Option 1); the 13 stock page checks read `g_stockView` (7 on the new pages). Aspect / Display Mode / Monitor changes still raise the exit dialog through the Resolution row's change test (523AA0). Changed rows are coloured per page (the row draw's call to 523AA0 is answered for the page in that slot; found in the in-game test). Ini keys unchanged. Plan, reviews and proof: `staging/options_pages/` (claudex-loop: plan APPROVED by Codex in round 4; two fresh Codex inspections, 2 low findings fixed, 1 rejected with disassembly evidence).
- v3.4.0 (2026-11-10, main rig): **`[Textures] Pool`.** 8x replacement textures (11-22 MB each) were MANAGED, so each also cost its full size in the 2 GB address space (913 MB peak in the user's log). `Pool=1` creates them in D3DPOOL_DEFAULT (SYSTEMMEM staging texture + UpdateTexture, staging released) and registers them in the game's own object list as 0x417480 does; `Pool=0` (default) is v3.3.5. Device reset (static RE, `staging/texpool/REPORT.md`): the game's only Reset is 0x402CD0 (lost device from the main tick 0x41507E, size / mode re-apply from 0x4149B5); before it, 0x417680 Releases every DEFAULT texture in its lists 0xED5774 / 0xED57B4 and erases the node but leaves the record's pointer, and GOG's wrapper (ori_d3d8.dll 1.0.0.6361) remakes the D3D9 side of every DEFAULT texture that is still alive, empty. Hooks at 0x402E17 (an extra reference on each live DEFAULT replacement over the game's release) and 0x402E2E (after Reset: list nodes back, every texture refilled from its file; on failure a MANAGED copy of the file, else the stock texture from the bank record, so no record points at a released texture; after a failed Reset all of them become MANAGED and Pool is off for the session). The summary line names the pool; each reset logs a `textures: device reset N:` line. Not yet seen in game.
- v3.3.5 (2026-11-10, main rig): **Item Shine; pickups, pause, companions; FOV First Person 100-120; Room 302 views.** (1) "Item Sparkle" is called "Item Shine" in Options, the ini and here; the keys are `[Items] Shine*`, and an old `Sparkle*` key is converted once (at start, and when the ini changes while the game runs; the old key wins, so a rollback to v3.3.4 and back keeps the last value). New ini files also get the `[Items]` comments. (2) Pickups: taking an item runs the generic pickup event in game phase 0 with the HUD gate closed (0x4D06EF -> 0x4F5090, types 4 and 7), the "got it" message box, and only then the taken bit (0x4D1180 -> 0x4F3A90 at 0x4D11B9). v3.3.4 stopped at the closed HUD gate, so every shine vanished mid-flash at the press. Phase 0 now draws whatever the gate says, and an item that disappears while it flashes finishes that flash where it lay. (3) Pause: an overlay request closes the HUD gate and the same frame is copied as the paused picture (0x4F9151 -> 0x55A160(1); frame end 0x55ABF0 -> 0x55A5C0; each overlay frame draws the copy, 0x55AE20 -> 0x55A8C0). That frame is now drawn with the shine, so it stays visible in the paused picture, frozen; the shine clock stops in phase 2 and play goes on from the same point of each flash. (4) The companions Eileen ([0xFCE6B4]) and Cynthia ([0xFD5A50]) are not in the 0x45E760 list the body test used (`ShineDepthTest` stays off): they now hide the shine like the player and monsters (position [[task+0x408]+0x18]+0x10, skipped while hidden, +0xE4 bit 2). (5) FOV First Person offers Default, 100, 105, 110, 115, 120 (older values moved once, see the table). (6) Room 302: while the window look-out (event record 0x69843C), the window examine text (0x61C3FC), the door scope (0x61B790), the story peepholes (0x61B6B8 / 0x61B86C) or the blocked hole (0x61CBF4) run, found in the running event list 0xFD198C or as the look-at target being turned to ([0x13D7B14], [0x13D7B08]), the first-person angle eases to the game's own and back. Static finding: the look-out and peephole views set their own render camera and angle every frame (0x54BE40: 45 deg at 0x54C002; door scope 60 deg at 0x4D848B), so FOV First Person reaches only the views that keep the first-person camera (the turn, the examine text); Hor+ in wide shapes still shows more at the sides of those own-camera views at any FOV setting. No new hook: the guard runs from the HUD call hook 0x4EFBFD. Not yet seen in game.
- v3.3.4 (2026-11-09, main rig): **sparkle self-check.** v3.3.3 still drew no sparkle in the user's session (stage 5 room 18: the items that were flashing were all counted off screen). Every `items:` log line is now followed by `items: check:`, which gives the viewport, where Eileen's chest projects, the counts too far / between flashes / behind the camera, and each item's screen spot, distance and w. The full Room 302 AI texture pack (598 keys, 4x) is installed.
- v3.3.3 (2026-11-09, main rig): **sparkles draw again; characters in front hide them.** The user: "Items are currently failing to sparkle at all with setting On. Even with SparkleDepthTest=0." The log shows the whole session ran with `SparkleDepthTest=1` (the ini was edited after the game closed): v3.3.2's depth-buffer test hid every sparkle (inferred depth mapping or buffer state wrong). It is now off by default and experimental. `SparkleBodies` (1) instead tests each sparkle against upright capsules (feet to 330 up, radius 45) of the player and every actor of the room list (0x45E760, position [[actor+0x408]+0x10]) that are nearer to the camera. `[Items]` is read again whenever SH4Refined.ini changes (checked once a second). The item log line also gives the frame's outcome (drawn, behind walls, behind characters, off screen), every 30 s while items are near.
- v3.3.2 (2026-11-09, main rig): **Item Sparkle hides behind Eileen and props, and flashes instead of shining.** The user (screenshot: sparkles drawn over Eileen): "prevent the effect from rendering through objects like Eileen" and "make it flash every ~7 seconds for about ~2 seconds". Each sparkle now carries the depth the game's world sprites get (projected GS z through the native 2D constants, c2.z = 1/2^24; the 3D pass projects with 0x41D510, the same screen matrix as 0x41D4C0) from a point 40 units toward the camera, and is drawn with ZENABLE / LESSEQUAL and no depth writes. Each item flashes on its own moment of a `SparkleEvery` (7 s) cycle for `SparkleFor` (2 s): a sine envelope, the star growing and turning a quarter. New keys `SparkleDepthTest` (1), `SparkleEvery`, `SparkleFor`.
- v3.3.1 (2026-11-09, main rig): **Item Sparkle crash fixed; its row moved to Options > Others.** The user: "Item sparkle crashes the game". v3.3.0 called IDirect3DDevice8 vtable 84 / 85 as Set / GetPixelShader; they are GetStreamSource / SetIndices (the pixel shader pair is 88 / 89, as `interp.c` lists). Handing SetIndices a stack address corrupted the game's vertex buffers: the crash at 0x415F0B (Lock through a freed VB of the ring 0xF3D9A8), right after the pass's own first fault. Fixed, and stream 0 is now saved and put back (D3D8 DrawPrimitiveUP clears it); the fault log names the address. The user: "Move item sparkle to the 3rd option (others) menu": page 2 gets a writable copy of its table (0x654E50) with Item Sparkle as row 7 and Exit at 8 (count 0x65503C / 0x655048, Exit row 0x65506C, tables 0x655060 / 0x65507C / 0x655088), rows 28 apart; page 0 is back to its v3.3.0 rows without it.
- v3.3.0 (2026-11-09, main rig): **FOV First Person / FOV Third Person, Item Sparkle, texture packs.** (1) The user: "Split Field of View option into FOV first person and FOV third person". The FOV setter 0x4F70A0 now looks at its caller: Room 302's first-person camera (0x500829 / 0x500B9E, 55 deg vertical) uses `FOVFirstPerson`, the world rig (0x50629D) `FOV`; two Options rows. `FOVFirstPerson` starts as a copy of `FOV`. (2) The user: a Resident-Evil-style sparkle on items in the world. `items.inc`: every frame of gameplay with the HUD allowed, the room's type-4 (item) and type-8 (memo) objects whose enabled bit is set (the game's own "visible and takeable" test: story flags, difficulty, taken bit, memo flag) get an additive star drawn with the game's device at the HUD call 0x4EFBFD, projected with the game's 0x55BF50 / 0x421820, hidden behind walls by the camera collision query 0x561CD0, faded beyond `SparkleRange`. Not in Room 302. Options > Others > Item Sparkle (v3.3.1); `[Items]` keys. Research: `staging/item_sparkle/RE.md`. (3) The `[Textures]` hook of the 3.2.4-texpack prototype is part of the release (off by default; `CModding/SH4HDTextures/texpack/REPORT.md`). In-game behaviour of (1) and (2) not yet seen.
- v3.2.4 (2026-11-09, main rig): **wall shadows and other world effects stay on the walls in wide shapes.** The user: with an Aspect Ratio above 4:3 "enemy wall decals like the ghost portals and leech shadows ... hover off the walls". These effects are projected on the CPU through the (Hor+) world camera (0x55BF50, 0x468560 and helpers) and drawn through the flush 0x564210 in shader space 0, which Menus43 draws with the 4:3 interface constants: every vertex was pulled toward the middle by 1 / S. 23 more flush call sites now use the game's own constants (section "v3.2.4 WORLD EFFECTS", each checked for `call 564210` at start): the Tremer wall shadow 0x45F3F5, the Water Prison type-01 ribbon 0x45F1C5, Wall Man quads 0x46064D / 0x4607A8, the Room 302 wall ghost 0x46185D / 0x461BB7, flames 0x460B04 / 0x460D66, eleven particle effects in 0x430000-0x43D000 (+ 0x4337BD's full-canvas strips), the SMG muzzle flash 0x445461 and event effect 0x54DAC3 / 0x54DEF1. 4:3 unchanged (S = 1); GOG's own 16:9 sizes and Menus43=0 never had the offset.
- v3.2.3 (2026-11-07, main rig): **Holy Candle crash at 60+ fps fixed.** Setting a candle down in a world crashed at 0x55C3B4. The placement handler (state 106, 0x540110) acts on the EXACT clip frame 28 (`cmp eax,edi; jne` at 0x54024D): it places the candle and frees the candle object 0x10A40B0. Clip time runs in 30 Hz frames, so at N fps frame 28 lasts N/30 ticks, and the second tick read the freed NULL object. A latched hook (hkCandlePlace, true-60 install) now fires the action once per placement and never with a NULL object. It is the only exact-frame test on a player clip in the exe (0x417F50 + cmp/je|jne scan); the four 30-fps target frames were already in kT60Bytes.
- v3.2.2 (2026-11-07, main rig): **Options > Monitor names screens by place.** The user has two screens and the row showed "Display 3": Windows numbered them DISPLAY2 / DISPLAY3 and the row used that GDI number. The list is now sorted left to right, the primary reads "Main Display" and the others "Display n" by place; the choice is still found by device path, and a hand-typed Monitor=n still means DISPLAYn.
- v3.2.1 (2026-11-06, main rig): **the weapon ropes step at 30 Hz.** The user at 60-180: the chain and purse teetered
  and spun from her idle and breathing. v3.2's d^s and g x s^2 are right, but the stock rope is calm mainly because the
  length projection after each Verlet step (0x443EB0) removes energy in proportion to the angle swept per tick, and
  that fades as dt shrinks (live at 180: chain tip 40 deg mean, 1225 u/s against the hand, with her standing still).
  `RopeCall` replaces the `call 0x443FF0` at 0x4445A3 (purse) and 0x4449DF (chain). Above 30 the stock solver runs only
  on 30 Hz gate ticks, with the stock constants, from the mod's copy of the rope. Every tick draws
  lerp(from, pos, g_gateAcc / rate) moved with the hand. The purse's wall push and its node-2 clamp are carried into
  the copy. hkRopeDamp / hkRopeSag are installed only with hidden `RopeStep30=0`. Simulated on the recorded hand path
  (Playable Eileen `Notes/session8/framerate_bugs.md`), the drawn rope at 60 / 120 / 180 matches stock 30 (chain 11.1
  deg / 247 u/s, purse 6.9 / 50). Merged on the main rig from the shared v3.2 source (v1.8 files in
  `history/main_rig_v1.8/`); the unmodified v3.2 rebuilds to the shipped code byte for byte.
