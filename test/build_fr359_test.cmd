@echo off
rem v3.59 FR359: builds and runs the native test of src\cutrate_sim.h (cut-scenes and Room 302's views at the gameplay rate: the phase rule, the stream clock, the camera-cut judge) and src\wv359_table.h (the window view's per-tick values), test\fr359_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 fr359_test.c /Fo"out\\" /Fe"out\fr359_test.exe" || exit /b 1
out\fr359_test.exe
