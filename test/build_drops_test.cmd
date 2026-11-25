@echo off
rem SUPPLY DROPS: builds and runs the native test of the pure part of src\drops.inc (test\drops_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 drops_test.c /Fo"out\\" /Fe"out\drops_test.exe" || exit /b 1
out\drops_test.exe
