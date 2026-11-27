@echo off
rem v3.58 GX358: builds and runs the native test of the effect budget quick rules and the rate guard quick window (test\gx358_test.c over src\budget_sim.h and src\rateguard_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 gx358_test.c /Fo"out\\" /Fe"out\gx358_test.exe" || exit /b 1
out\gx358_test.exe
