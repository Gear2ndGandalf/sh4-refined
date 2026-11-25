@echo off
rem HUD BARS (p2_hud): builds and runs the native bar / curse-meter state test (test\hudbars_sim_test.c over
rem src\hudbars_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 hudbars_sim_test.c /Fo"out\\" /Fe"out\hudbars_sim_test.exe" || exit /b 1
out\hudbars_sim_test.exe
