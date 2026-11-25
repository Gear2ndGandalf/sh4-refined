@echo off
rem v3.36 HE336: builds and runs the native test of src\hitpool_sim.h (test\hitpool_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 hitpool_test.c /Fo"out\\" /Fe"out\hitpool_test.exe" || exit /b 1
out\hitpool_test.exe
