@echo off
rem v3.58 MN358: builds and runs the native test of src\valbox_sim.h (test\valbox_test.c: the Options value box: one arrow x per page for every row and value, centred values, the volume bars' arrows, the mouse rects and clicks at 4:3 .. 32:9), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 valbox_test.c /Fo"out\\" /Fe"out\valbox_test.exe" || exit /b 1
out\valbox_test.exe
