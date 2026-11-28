@echo off
rem v3.59 FT359: builds and runs the native test of the enemy bars' looks under the special filters (test\hbtheme_sim_test.c
rem over src\hbtheme_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 hbtheme_sim_test.c /Fo"out\\" /Fe"out\hbtheme_sim_test.exe" || exit /b 1
out\hbtheme_sim_test.exe
