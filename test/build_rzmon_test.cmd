@echo off
rem v3.49 RZ349: builds and runs the native test of src\rzmon_sim.h (the Randomizers Monsters: the texts, the type table, the scan of the enemy placements on the stock exe, the plans, the words in the scripts, the descriptor transform, the records and the save filter, the word stack) and testzmon_sim_test.c, x86 /O2 as build.cmd.
rem Reads C:\ClaudeCode\CModding\SH4Decomp\orig\sh4.exe (read only); another path as the first argument.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 rzmon_sim_test.c /Fo"out\\" /Fe"out\rzmon_sim_test.exe" || exit /b 1
out\rzmon_sim_test.exe %1
