@echo off
rem v3.56 MERGE (RI356 and ER356 wrote one each): builds and runs the native tests of src\worldvisit_sim.h, the one world-visit rule of Enemies Return and the Randomizer's Reroll Visits: test\worldvisit_test.c (ER356: part 1, the load signal and the kinds) and test\worldvisit_visits_test.c (RI356, adapted: part 2, the visit serials and room counts, and both parts together), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 worldvisit_test.c /Fo"out\\" /Fe"out\worldvisit_test.exe" || exit /b 1
cl /nologo /O2 /MT /W3 worldvisit_visits_test.c /Fo"out\\" /Fe"out\worldvisit_visits_test.exe" || exit /b 1
out\worldvisit_test.exe || exit /b 1
out\worldvisit_visits_test.exe
