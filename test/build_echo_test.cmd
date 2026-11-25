@echo off
rem v3.30 ECHO: builds and runs the native Echo double state test (test\echo_sim_test.c over src\echo_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 echo_sim_test.c /Fo"out\\" /Fe"out\echo_sim_test.exe" || exit /b 1
out\echo_sim_test.exe
