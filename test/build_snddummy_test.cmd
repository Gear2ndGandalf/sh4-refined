@echo off
rem v3.59 SD359: builds and runs the native test of src\snddummy_sim.h (test\snddummy_test.c: the lost-device guard's stand-in buffers, every method through its vtable, the cursor, the pool, the game's own refill), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 snddummy_test.c /Fo"out\\" /Fe"out\snddummy_test.exe" || exit /b 1
out\snddummy_test.exe
