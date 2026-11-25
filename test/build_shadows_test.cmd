@echo off
rem v3.40 SH340: builds and runs the native softer-shadows test (test\shadows_sim_test.c over src\shadows_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 shadows_sim_test.c /Fo"out\\" /Fe"out\shadows_sim_test.exe" || exit /b 1
out\shadows_sim_test.exe
