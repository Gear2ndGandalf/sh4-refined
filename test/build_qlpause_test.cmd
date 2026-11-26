@echo off
rem v3.52 QL352: builds and runs the native test of src\qlpause_sim.h: the Extra Menu's Quick Load inside the pause in a model of the game's frame (test\qlpause_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 qlpause_test.c /Fo"out\\" /Fe"out\qlpause_test.exe" || exit /b 1
out\qlpause_test.exe
