@echo off
rem v3.56 PV356: builds and runs the native Effects-preview test (test\preview_sim_test.c over src\preview_sim.h), x86 /O2
rem as build.cmd. Usage: build_pv_test.cmd [--dump DIR]  (the dump feeds tools\pv_mockup.py)
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 preview_sim_test.c /Fo"out\\" /Fe"out\preview_sim_test.exe" || exit /b 1
out\preview_sim_test.exe %*
