# SH4 Refined

**Current version: v3.48.0 (tested by the author)**

SH4 Refined is a fan-made enhancement mod for the PC version of **Silent Hill 4: The Room** sold by GOG. It is a
single `winmm.dll` that the game loads from its own folder: no loader, no installer and no changes to the game's exe
or data files. It runs the game in a proper window or borderless at any aspect ratio, raises the frame rate, adds
screen effects, softer shadows, an HD menu font, analog movement, many optional gameplay modifiers and a pickup
randomizer, all switched from new pages in the game's own Options menu and saved in `SH4Refined.ini`. The optional
[dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2) renderer by Dege is included as well.

> Unofficial fan mod. Not affiliated with or endorsed by Konami or GOG. You need your own copy of the game;
> no game files are included in this repository.

## Features at a glance

**Display, window and frame rate**
- Windowed (resizable, snaps to standard sizes) or Borderless; choose the monitor; no freeze or minimise on Alt+Tab;
  the mouse is captured only while the game is in front.
- Aspect Ratio row: 4:3, 16:10, 16:9, 21:9 and 32:9 with a wider 3D view (Hor+); menus, text, HUD and movies stay 4:3
  in the middle, full-screen pictures get black sides.
- Frame Rate 30 or 60 (a true logic rate, with hundreds of timing fixes), or frame interpolation as an alternative mode.
- Separate first- and third-person field of view; Opening: skip the logos (and, with Fast, the long title intro).
- Renderer row (with the included dgVoodoo2): Direct3D 11, Direct3D 11 (16-bit), Direct3D 12 or Stock.

**Screen effects**
- Noise strength, type (Fine, Coarse, Film, VHS, Scan Roll, rain on the lens) and colour.
- Filters: CRT, Vivid, Sepia, Old Film, Night Vision, Thermal (living bodies glow), Spirit Vision (ghosts and their
  hauntings seen through walls), Vignette; live preview on the page.
- Depth of field, bloom, colour grading and an Otherworld lens; ten blood colours.

**Shadows**
- A Shadows page with soft shadow edges, shadow darkness and a split-view preview.

**Menus and fonts**
- HD menu font: EB Garamond, Cormorant Garamond or Crimson Text (or the game's own).
- Subtitle Style: size, colour, edge, box and animated effects (typewriter, fade, wave, glitch, blood drip, embers ...).
- Button Config shows readable pad names and Xbox / PlayStation / Nintendo button glyphs.

**Controls**
- **Omni-Directional Movement**: the left stick walks at any angle, and a lighter push walks slower (Off = the game's
  own 8 directions).
- Vibration strength (Off / Low / Medium / High).

**Gameplay options** (all Off by default)
- Player: Movement Speed, Medallion Durability, Regeneration, Ammo Saver, Dark Pact, Grab Breaker, Quick Charge,
  Life Steal, Curse Meter.
- Enemies: One-Hit Kill, Enemy Copies, Enemies Follow, Enemy Health Bars, Supply Drops, Revive Enemies.
- Special: Death Presence (a draining fog around you), After Shock (electric after-images), Echo Doubles (shadow
  copies that fight with you), Ghost Teleport.
- Also on the Gameplay page: No Clip, Extended Toolbelt (20 items on two pages), Stackable Items.

**Randomizer**
- Item and weapon pickups: Shuffle or Random, Keep Specials, Room 302 in or out, new items each visit, a seed you can
  replay. Nothing is written into your saves.

**Quality of life**
- Quick Save (F5) / Quick Load (F9), kept after a restart; Save Banks (up to 99 banks of save slots).
- Saves are titled after the world they were made in.
- A pause Extra Menu: item box, save, load, quick save / load, Eileen's outfit and camera settings
  (including an over-the-shoulder camera).
- Item Shine on items and memos lying in the worlds.
- Fixes: a controller-connect crash in GOG's input layer; GOG's display wrapper drawn sharp at every size.

**Logs**
- `SH4Refined.log` (and `SH4Refined.prev.log` for the run before) says what was applied and why, with a performance
  summary every few minutes.

Every option, its ini key and its limits are described in [docs/FULL_README.md](docs/FULL_README.md).

## Requirements

- **Silent Hill 4: The Room, GOG version** (GOG v7, December 2025 build): `SILENT HILL 4.exe` with
  md5 `15a85581eca3d0664089c440b0acada1`. Other versions are not supported; the mod checks the game's bytes before it
  changes anything and leaves a feature off when they differ.
- Windows 10 or 11. The dgVoodoo2 renderer needs a Direct3D 11 graphics card; leave it out on Steam Deck / Linux.
- Optional: the author's **Playable Eileen** mod (a separate `dsound.dll` plugin). SH4 Refined works with or without
  it; Playable Eileen v1.24.0 or later follows Omni-Directional Movement.

## Install

1. Close the game.
2. Copy everything inside [`Install\`](Install) into your Silent Hill 4 folder (the one that holds
   `SILENT HILL 4.exe`) and say yes to replace.
3. Start the game. The new rows and pages are in the game's Options (Option 1 to Option 6); `SH4Refined.ini` is written on the first start.

To play without dgVoodoo2, copy only `winmm.dll` and the `fonts` folder. Details, the file list and uninstalling:
[INSTALL.md](INSTALL.md).

## Uninstall

Close the game and delete `winmm.dll` and the `fonts` folder (plus `d3d9.dll`, `dgVoodoo.conf` and
`dgVoodoo_variants\` if you installed the renderer). Your saves stay. See [INSTALL.md](INSTALL.md).

## Build from source

Visual Studio 2022 with the C++ desktop tools (x86) is enough; no other libraries.

```
build.cmd [outdir]            -> out\winmm.dll (x86, static CRT)
test\build_<name>_test.cmd    -> builds and runs one native test (about 40 of them)
```

Some tests and tools read your own copy of the game exe (for example `test\build_rando_test.cmd <path to exe>`); the
inventory test needs `test\inventory_rdata.h`, which `python -I tools\inv337_gen.py --exe <your exe>` writes. The
Python tools need Python 3 (some previews use NumPy, Pillow and SciPy). Build notes: [docs/FULL_README.md](docs/FULL_README.md#build).

## Versions

The current version is named at the top of this page. Every release, newest changes included, is described in the
history of [docs/FULL_README.md](docs/FULL_README.md#history).

## Credits

SH4 Refined is by Gear2ndGandalf, built with Claude Code (Anthropic) and reviewed with OpenAI Codex. With thanks to
Konami and Team Silent for the game, GOG for the PC release, Dege for dgVoodoo2, ThirteenAG (WidescreenFixesPack),
vallfrr (sh4vita, the idea behind the frame interpolation), Kenney (Input Prompts), the authors of EB Garamond,
Cormorant Garamond and Crimson Text, and the SH4 decompilation project. The full list: [CREDITS.md](CREDITS.md);
licences of included third-party files: [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt).

## Licence

SH4 Refined's own code and files: [MIT](LICENSE). Third-party files keep their own licences
([THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt)). Silent Hill is a trademark of Konami; no game assets are included.
