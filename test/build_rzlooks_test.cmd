@echo off
rem v3.49 RZ349L: builds and runs the native test of src\rzlooks_sim.h (the Randomizer's looks: Eileen Outfit's draw and override rules, the victim ghosts of the stock scripts, the spot pick, patch-and-restore) and test\rzlooks_sim_test.c, x86 /O2 as build.cmd.
rem Reads C:\ClaudeCode\CModding\SH4Decomp\orig\sh4.exe (read only); another path as the first argument.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 rzlooks_sim_test.c /Fo"out\\" /Fe"out\rzlooks_sim_test.exe" || exit /b 1
out\rzlooks_sim_test.exe %1
