@echo off
rem v3.33 LOG333: builds and runs the native test of the gameplay health log's pure part (test\loghealth_test.c over
rem src\loghealth_sim.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 loghealth_test.c /Fo"out\\" /Fe"out\loghealth_test.exe" || exit /b 1
out\loghealth_test.exe
