@echo off
rem v3.56 ER356: builds and runs the native test of src\enmreturn_sim.h (Enemies Return Off / By Room / By World, played against a model of the game's kill bits, bodies, hole, saves and loads) and test\enmreturn_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 enmreturn_test.c /Fo"out\\" /Fe"out\enmreturn_test.exe" || exit /b 1
out\enmreturn_test.exe
