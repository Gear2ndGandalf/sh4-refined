@echo off
rem v3.33 MENU333: builds and runs the native test of src\qs_avail_sim.h (the Extra Menu Quick Save / Quick Load rows) and test\qs_avail_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 qs_avail_test.c /Fo"out\\" /Fe"out\qs_avail_test.exe" || exit /b 1
out\qs_avail_test.exe
