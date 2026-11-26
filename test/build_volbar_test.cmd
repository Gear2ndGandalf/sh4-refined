@echo off
rem v3.55 OL56: builds and runs the native test of src\volbar_sim.h (test\volbar_test.c: the 0x525672 volume-bar stub run in the game's own jl / cmp / jg sequence, its patch bytes, the stock exe's bytes), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 volbar_test.c /Fo"out\\" /Fe"out\volbar_test.exe" || exit /b 1
out\volbar_test.exe
