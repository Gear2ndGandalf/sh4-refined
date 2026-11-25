@echo off
rem v3.37 INV337: builds and runs the native test of src\inventory_sim.h and of the generated cave src\inventory_cave.inc
rem (test\inventory_test.c), x86 /O2 as build.cmd. Linked at 0x400000, fixed, no ASLR: the test maps the game's
rem inventory and the exe's item tables at their real addresses.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 inventory_test.c /Fo"out\\" /Fe"out\inventory_test.exe" /link /DYNAMICBASE:NO /BASE:0x400000 /FIXED || exit /b 1
out\inventory_test.exe
