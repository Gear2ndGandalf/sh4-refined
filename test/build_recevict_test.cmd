@echo off
rem v3.36 HE336: builds and runs the native test of src\recevict_sim.h (test\recevict_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 recevict_test.c /Fo"out\\" /Fe"out\recevict_test.exe" || exit /b 1
out\recevict_test.exe
