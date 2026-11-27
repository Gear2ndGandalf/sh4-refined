@echo off
rem v3.56 FR356: builds and runs the native test of src\rateguard_sim.h (the rate guard's step logic) and src\t60final_sim.h (the final room's per-tick values), test\fr356_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 fr356_test.c /Fo"out\\" /Fe"out\fr356_test.exe" || exit /b 1
out\fr356_test.exe
