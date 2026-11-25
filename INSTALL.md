# Installing SH4 Refined

You need **Silent Hill 4: The Room from GOG** (GOG v7, `SILENT HILL 4.exe` md5 `15a85581eca3d0664089c440b0acada1`).
GOG Galaxy installs it to `C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4` unless you chose another folder.

## What is in `Install\`

| File | What it is | Needed? |
|---|---|---|
| `winmm.dll` | SH4 Refined v3.49.0 (md5 `5D6E6A53780FB0C595A4EAF0EB476883`; not yet play-tested) | yes |
| `fonts\` | the HD menu font sheets (EB Garamond, Cormorant Garamond, Crimson Text) and their OFL licences | for the Menu Font row |
| `d3d9.dll` | dgVoodoo2 2.87.6 by Dege, its `MS\x86\D3D9.dll`, unchanged (md5 `CAA75C55D9B3121D17433BD3B38C7BB2`) | optional renderer |
| `dgVoodoo.conf` | dgVoodoo2 settings for SH4 ("Direct3D 11", the default) | with `d3d9.dll` |
| `dgVoodoo_variants\` | the settings the Renderer row switches between (Direct3D 11, Direct3D 11 16-bit, Direct3D 12, Stock) and `manifest.csv` with their md5s | with `d3d9.dll` |

**Not included, because they belong to the game:** GOG's `d3d8.dll` and `ori_d3d8.dll` (GOG's DirectX 8 wrapper),
`dinput8.dll`, `dxcfg.exe` / `dxcfg.ini` and every other file of the game. Keep them as GOG installed them; SH4
Refined and dgVoodoo2 work on top of them.

## Install

1. Close the game.
2. Copy everything inside `Install\` into the Silent Hill 4 folder (the folder with `SILENT HILL 4.exe`). Say yes to
   replace.
   - Without the dgVoodoo2 renderer (Steam Deck / Linux, an older graphics card, or your own dgVoodoo2 setup): copy only
     `winmm.dll` and the `fonts` folder.
3. Start the game. On the first start SH4 Refined writes `SH4Refined.ini` (all settings) and `SH4Refined.log`.
   The new rows are in Options: Option 1 (Aspect Ratio, Menu Font), Option 2 (Control), Option 3 (Screen Effects,
   Subtitle Style, Shadows: press Enter on Noise Effect, Subtitles or Realtime Shadow), Option 4 (Presentation:
   Display Mode, Monitor, Frame Rate, FOV, Renderer, Opening), Option 5 (Gameplay) and Option 6 (Extra Menu).

Notes:
- If you used **SH4Windowed.asi** (with Ultimate ASI Loader), remove `scripts\SH4Windowed.asi` first; SH4 Refined
  does the same and stays inactive while that one is loaded. Its `scripts\SH4Windowed.ini` values are imported once.
- With **Playable Eileen**, use plugin v0.82 or later (older versions have their own windowed mode, and SH4 Refined
  then stays inactive). v1.24.0 or later follows Omni-Directional Movement.
- V-Sync, anti-aliasing and anisotropic filtering are set in GOG's own tool (GOG Galaxy > Manage > DirectX Settings);
  SH4 Refined does not change `dxcfg.ini`.
- A Renderer change applies at the next game start. The row is greyed out when `d3d9.dll` or the variants are missing.

## Files SH4 Refined writes

- `SH4Refined.ini`, `SH4Refined.log`, `SH4Refined.prev.log` next to the game.
- `preview.dds`: the picture behind the Screen Effects page (saved at the first pause in gameplay, or with F7).
- Quick saves in `save\quick\` (`qs1.bin` .. `qs3.bin`); Save Banks 2 to 99 as `sh4_bankN.sav` (bank 1 is
  the game's own `sh4.sav`).
- The Renderer row copies one file of `dgVoodoo_variants\` over `dgVoodoo.conf` (written as `dgVoodoo.conf.new` first
  and checked); it never touches `d3d9.dll`.

## Uninstall

1. Close the game.
2. Delete `winmm.dll` and the `fonts` folder. If you installed the renderer, also delete `d3d9.dll`, `dgVoodoo.conf`
   and the `dgVoodoo_variants` folder.
3. If you like, delete `SH4Refined.ini`, `SH4Refined.log`, `SH4Refined.prev.log` and `preview.dds`.

Your saves stay and load in the stock game. Two things to know: a Blood Colour other than Red / Green / Purple shows
a wrong colour in the stock game until you set Blood Colour there again; saves in Save Banks 2 and up and the quick
saves are only reachable with SH4 Refined.

## Updating

Close the game and copy the new `Install\` over the old files. Your `SH4Refined.ini` is kept.
