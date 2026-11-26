@echo off
rem v3.50 EF350: builds and runs the native test of src\pursuit_sim.h (Enemy Pursuit's lose-track rules, the followers' side file) and test\pursuit_sim_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 pursuit_sim_test.c /Fo"out\\" /Fe"out\pursuit_sim_test.exe" || exit /b 1
out\pursuit_sim_test.exe
