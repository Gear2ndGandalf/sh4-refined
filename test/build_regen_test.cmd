@echo off
rem v3.47 RG347: builds and runs the native test of src\regen_sim.h (Refined's own Regeneration: rates, the wait, the gates, who heals) and test\regen_sim_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 regen_sim_test.c /Fo"out\\" /Fe"out\regen_sim_test.exe" || exit /b 1
out\regen_sim_test.exe
