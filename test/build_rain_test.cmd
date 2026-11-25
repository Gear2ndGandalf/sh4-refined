@echo off
rem v3.26.0: builds and runs the native rain simulation test (test\rain_sim_test.c over src\rain_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 rain_sim_test.c /Fo"out\\" /Fe"out\rain_sim_test.exe" || exit /b 1
out\rain_sim_test.exe
