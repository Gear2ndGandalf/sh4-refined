@echo off
rem v3.36 TP336: builds and runs the native test of src\pools_sim.h (test\pools_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 pools_test.c /Fo"out\\" /Fe"out\pools_test.exe" || exit /b 1
out\pools_test.exe
