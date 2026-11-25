@echo off
rem v3.31 ENEMIES: builds and runs the native Enemy Copies / Enemies Follow rule test (test\enemies_sim_test.c over
rem src\enemies_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 enemies_sim_test.c /Fo"out\\" /Fe"out\enemies_sim_test.exe" || exit /b 1
out\enemies_sim_test.exe
