@echo off
rem v3.41 VW341: builds and runs the native views test (test\views_sim_test.c over src\views_sim.h: the black sides beside the 4:3 picture, the Borderless edge cover), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 views_sim_test.c /Fo"out\\" /Fe"out\views_sim_test.exe" || exit /b 1
out\views_sim_test.exe
