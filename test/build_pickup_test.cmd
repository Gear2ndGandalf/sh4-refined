@echo off
rem v3.32 PM332: builds and runs the native test of src\pickup_sim.h (test\pickup_sim_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 pickup_sim_test.c /Fo"out\\" /Fe"out\pickup_sim_test.exe" || exit /b 1
out\pickup_sim_test.exe
