# graphics: the dgVoodoo2 renderer settings and scripts

SH4 Refined can run the game through **dgVoodoo2 2.87.6 by Dege** (https://github.com/dege-diosg/dgVoodoo2): GOG's
Direct3D 8 wrapper -> dgVoodoo2's `d3d9.dll` -> Direct3D 11 or 12. It is optional; no SH4 Refined feature needs it.
The player files are in `..\Install\` (`d3d9.dll`, `dgVoodoo.conf` = the Direct3D 11 settings, `dgVoodoo_variants\`).

## What is here

- `install\variants\*.conf`: the dgVoodoo2 settings made for Silent Hill 4, each dgVoodoo2's own default
  configuration (`..\third_party\dgVoodoo2-2.87.6\dgVoodoo.stock.conf`) with a few keys changed;
  `install\variants\variants.csv` lists each one, its md5, what it changes and what was measured.
  - `d3d11_flipdiscard` ("Direct3D 11", the default): Direct3D 11, flip-discard presentation.
  - `d3d11fd_rt16` ("Direct3D 11 (16-bit)"): the same with 16-bit render targets.
  - `passthru` ("Stock"): dgVoodoo2 passes every call to the system Direct3D 9.
  - `d3d11`, `d3d11fd_rt10`, `d3d12_10bit`, `d3d12_hdr`: tested, not offered in the menu.
- `install\dgVoodoo.conf`: the Direct3D 12 settings (`d3d12.conf` in the player's `dgVoodoo_variants\`).
- Scripts (PowerShell; `-Game <folder>`, `-WhatIf`; every file is md5-checked, nothing foreign is overwritten):
  - `install_dgvoodoo.ps1`: copies `d3d9.dll` and the chosen conf into the game folder. Put `..\Install\d3d9.dll`
    next to the script first.
  - `install_variants_folder.ps1`: writes `<game>\dgVoodoo_variants\` and its `manifest.csv`, which the Renderer row
    in Options > Option 4 reads.
  - `switch_dgvoodoo_conf.ps1`: replaces `dgVoodoo.conf` by another pinned variant.
  - `uninstall_dgvoodoo.ps1`: removes our `d3d9.dll` and conf (to the Recycle Bin).

Copying `..\Install\` by hand gives the same result as the two install scripts.

## Measured (on the author's PC, an AMD graphics card; untested on NVIDIA / Intel)

- Direct3D 11 (flip discard): room-entry hitches like the stock game, steady pacing, about 1.7 ms from present to display.
- Direct3D 11 (16-bit): about 1.7 ms more from present to display; finer dark shades.
- Direct3D 12: freezes of 260-530 ms when entering rooms and about 200 MB more of the game's 2 GB address space.
- Stock: measured like the system Direct3D 9.
