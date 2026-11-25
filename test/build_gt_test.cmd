@echo off
rem v3.43 GT343: builds and runs the native test of Ghost Teleport's rules (test\gt_sim_test.c over src\teleport_sim.h and
rem src\echo_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 gt_sim_test.c /Fo"out\\" /Fe"out\gt_sim_test.exe" || exit /b 1
out\gt_sim_test.exe
