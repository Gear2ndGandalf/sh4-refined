@echo off
rem v3.48 RZ348: builds and runs the native test of src\rando_sim.h (the Randomizer: ini texts, the script scan on the stock exe, the plans, patch-and-restore) and test\rando_sim_test.c, x86 /O2 as build.cmd.
rem Reads sh4.exe in this folder (a copy of your stock GOG v7 exe, read only); another path as the first argument.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 rando_sim_test.c /Fo"out\\" /Fe"out\rando_sim_test.exe" || exit /b 1
out\rando_sim_test.exe %1
