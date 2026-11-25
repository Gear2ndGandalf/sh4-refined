@echo off
rem v3.27.0: builds and runs the native Death Presence state test (test\dp_sim_test.c over src\dp_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 dp_sim_test.c /Fo"out\\" /Fe"out\dp_sim_test.exe" || exit /b 1
out\dp_sim_test.exe
