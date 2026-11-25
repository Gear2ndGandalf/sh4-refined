@echo off
rem v3.32 NC332: builds and runs the native test of No Clip's rules (test\noclip_sim_test.c over src\noclip_sim.h),
rem x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 noclip_sim_test.c /Fo"out\\" /Fe"out\noclip_sim_test.exe" || exit /b 1
out\noclip_sim_test.exe
