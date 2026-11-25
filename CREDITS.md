# Credits

SH4 Refined is by **Gear2ndGandalf** (idea, design, direction and all play testing). The code was written with
**Claude Code** (Anthropic) and reviewed with **OpenAI Codex**.

Many people and projects helped, most of them without knowing it. Thank you. Being listed here does not mean that
anyone below endorses SH4 Refined. Licences of the files that are included: [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt).

## The game

- **Konami** and **Team Silent** (Konami Computer Entertainment Tokyo) for *Silent Hill 4: The Room*.
- **GOG** for the PC release this mod is made for, with its DirectX 8 and input wrappers that SH4 Refined works on top
  of (not included here).

## Included in this repository

- **Dege**, [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2) 2.87.6: `Install\d3d9.dll`, unchanged, the optional
  renderer. The `dgVoodoo.conf` files are dgVoodoo2's own default settings, adjusted for Silent Hill 4.
- **Kenney** ([kenney.nl](https://www.kenney.nl)), *Input Prompts* 1.5A (CC0): the Xbox, PlayStation and Nintendo
  button glyphs in Button Config (`src\glyphs_data.h`, made by `tools\gen_glyphs.py`).
- **The EB Garamond Project Authors**, **the Cormorant Project Authors** and **the Crimson Text Project Authors**, the
  three menu fonts (SIL Open Font License 1.1), taken from the [Google Fonts](https://github.com/google/fonts)
  repository (`third_party\fonts\`, sheets in `Install\fonts\`).

## Code and ideas this mod builds on

- **ThirteenAG**, [WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack) (MIT): the 4:3 menus in
  wide shapes are based on its Silent Hill 4 part; its fixes were also studied for the frame-rate work.
- **vallfrr**, [sh4vita](https://github.com/vallfrr/sh4vita): the idea behind the frame interpolation mode (keep the
  30 Hz logic, replay each frame's Direct3D calls with blended shader constants). No sh4vita code is included: the
  pairing was written clean-room from a functional description (`cleanroom\SPEC.md`).
- **Austin Appleby**, MurmurHash3 (public domain): the hash mixing steps used by the frame interpolation.
- The **Silent Hill 2: Enhanced Edition** project: its "true 60" approach on a fixed-tick engine was the model for SH4
  Refined's true frame-rate mode.
- The **SH4 decompilation project** ([github.com/Gear2ndGandalf/sh4-decomp](https://github.com/Gear2ndGandalf/sh4-decomp)):
  its Ghidra output and notes were used to find and check game functions and tables.

## Sister projects by the same author

- **Playable Eileen**: SH4 Refined started as its windowed mode (up to plugin v0.81) and still works hand in hand
  with it (shared exports, Regeneration rates, the over-the-shoulder camera, analog walking).
- **The SH4 cheat table** (a Cheat Engine table for the GOG version, built on a second PC): its research and scripts
  became SH4 Refined features: Save Banks, the pause Extra Menu (Pause Menu+), Eileen's outfit, Extended Toolbelt and
  Stackable Items (`tools\inv337\`). Its earlier **SH4Windowed.asi** (with Ultimate ASI Loader) is the windowed mode
  SH4 Refined replaced.
- **SH4 Graphics Plus**: the dgVoodoo2 research, measurements and settings, merged into SH4 Refined.

## Tools

- Microsoft **Visual Studio 2022** (MSVC, x86) builds the dll and the native tests.
- **Python 3** with **NumPy**, **Pillow** and **SciPy** for the generators, previews and log checks.
- **Ghidra** (through the decompilation project's output), **Cheat Engine** (the cheat-table scripts), **PresentMon**
  (the renderer measurements) and PowerShell (the dgVoodoo2 install scripts in `graphics\install\`).
