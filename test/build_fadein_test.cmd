@echo off
rem v3.50 FI350: builds and runs the native test of src\fadein_sim.h (Fade In: texts, who fades, the source and skip rules, the fade / shadow / portal curves and the draw clock, the room table, Revive notes, the room line) and test\fadein_sim_test.c, x86 /O2 as build.cmd.
rem Reads C:\ClaudeCode\CModding\SH4Decomp\orig\sh4.exe (read only: the bytes and calls of 0x45C880, the pre-draw callbacks, the hook sites); another path as the first argument.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 fadein_sim_test.c /Fo"out\\" /Fe"out\fadein_sim_test.exe" || exit /b 1
out\fadein_sim_test.exe %1
