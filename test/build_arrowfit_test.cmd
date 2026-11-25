@echo off
rem v3.33 MENU333: builds and runs the native test of src\arrowfit_sim.h (the Options value arrows) and test\arrowfit_test.c, x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 arrowfit_test.c /Fo"out\\" /Fe"out\arrowfit_test.exe" || exit /b 1
out\arrowfit_test.exe
