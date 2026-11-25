@echo off
rem v3.29 AFTERIMAGE: builds and runs the native after-image state test (test\ai_sim_test.c over src\ai_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 ai_sim_test.c /Fo"out\\" /Fe"out\ai_sim_test.exe" || exit /b 1
out\ai_sim_test.exe
