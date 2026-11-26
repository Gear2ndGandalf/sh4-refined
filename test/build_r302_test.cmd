@echo off
rem v3.50 R302: builds and runs the native test of src\r302_sim.h (Room 302's look and walk for Henry and Eileen: the hand-over rule,
rem the rows' values and seed, the look step against a transcription of Playable Eileen v1.24.4's, the walk factor, the export layout)
rem and test\r302_sim_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 r302_sim_test.c /Fo"out\\" /Fe"out\r302_sim_test.exe" || exit /b 1
out\r302_sim_test.exe
