@echo off
rem v3.39 PR339: builds and runs the native test of the Renderer row (test\rend_test.c over src\renderer_sim.h, src\renderer.inc
rem on its own folder test\out\rd_game) and of the Opening setting (src\opening_sim.h), x86 /O2 /W3 as build.cmd. Optional
rem argument: the game folder (read only: its variant files' md5s against its manifest).
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 rend_test.c /Fo"out\\" /Fe"out\rend_test.exe" || exit /b 1
out\rend_test.exe %1
