@echo off
rem v3.32 DESC332: builds and runs the native Options description fit test (test\descfit_sim_test.c over
rem src\descfit_sim.h), x86 /O2 as build.cmd. Optional argument: a copy of the game's data\fontdata_kr.bin (read only)
rem to check the game's font 1 metrics as well.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 descfit_sim_test.c /Fo"out\\" /Fe"out\descfit_sim_test.exe" || exit /b 1
out\descfit_sim_test.exe %1
