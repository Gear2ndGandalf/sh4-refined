@echo off
rem v3.59 TV359: builds and runs the native test of Thermal Vision's rules (test\thermal_sim_test.c over src\thermal_sim.h:
rem the light grid's depth, who walks, the footprint shapes and their mirror), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 thermal_sim_test.c /Fo"out\\" /Fe"out\thermal_sim_test.exe" || exit /b 1
out\thermal_sim_test.exe
