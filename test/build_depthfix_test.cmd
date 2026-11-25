@echo off
rem v3.31 DEPTH331: builds and runs the native depth-restore test (test\depthfix_sim_test.c over src\depthfix_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 depthfix_sim_test.c /Fo"out\\" /Fe"out\depthfix_sim_test.exe" || exit /b 1
out\depthfix_sim_test.exe
