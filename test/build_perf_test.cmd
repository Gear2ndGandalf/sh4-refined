@echo off
rem v3.37 CP337: builds and runs the native test of the performance overlay's pure part (test\perfhud_sim_test.c over
rem src\perfhud_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 perfhud_sim_test.c /Fo"out\\" /Fe"out\perfhud_sim_test.exe" || exit /b 1
out\perfhud_sim_test.exe
