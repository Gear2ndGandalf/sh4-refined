@echo off
rem v3.51 MU351: builds and runs the native test of src\pausesound_sim.h and the sound pause model (test\pausesound_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 pausesound_test.c /Fo"out\\" /Fe"out\pausesound_test.exe" || exit /b 1
out\pausesound_test.exe
