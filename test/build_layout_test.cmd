@echo off
rem v3.54 OL34: builds and runs the native test of src\layout_sim.h (test\layout_sim_test.c: the Options page tree, its slots, Back / Exit, the row placement against the plan's layout_data.py through ol_words.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 layout_sim_test.c /Fo"out\\" /Fe"out\layout_sim_test.exe" || exit /b 1
out\layout_sim_test.exe
