@echo off
rem v3.28 FIRE: builds and runs the native spirit fire test (test\sf_sim_test.c over src\spiritfire_sim.h), x86 /O2 as
rem build.cmd. Usage: build_sf_test.cmd [--dump DIR]  (the dump feeds tools\flame_preview.py)
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 sf_sim_test.c /Fo"out\\" /Fe"out\sf_sim_test.exe" || exit /b 1
out\sf_sim_test.exe %*
