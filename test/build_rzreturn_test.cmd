@echo off
rem v3.58 RZ358: builds and runs the native test of the rule a reroll redraws WHAT stands where, never WHETHER the dead are back (src\rzmon_sim.h's plan, save filter and copies' draws with src\enmreturn_sim.h and src\worldvisit_sim.h, played against a model of the game's enemy state: Enemies Return x Reroll x kills, room changes, world entries, Quick Save / Quick Load) and test\rzreturn_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 rzreturn_test.c /Fo"out\\" /Fe"out\rzreturn_test.exe" || exit /b 1
out\rzreturn_test.exe
