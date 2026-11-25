@echo off
rem v3.46 OM346: builds and runs the native test of src\omni_sim.h (Stick Movement: dead zone, Direction / Analog maps, bindings, the self-check) and test\omni_sim_test.c, x86 /O2 as build.cmd.
rem v3.48 OM348: Omni-Directional Movement, Off / On (On = v3.47's Analog): the OmniMove reader (new and old spellings), the map, the export.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 omni_sim_test.c /Fo"out\\" /Fe"out\omni_sim_test.exe" || exit /b 1
out\omni_sim_test.exe
