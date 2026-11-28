@echo off
rem v3.59 SD359: builds and runs the native test of src\faultwhere_sim.h (test\faultwhere_test.c: Direct3D / driver modules, the place of a fault, the codes noted, two Direct3D faults in a second), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 faultwhere_test.c /Fo"out\\" /Fe"out\faultwhere_test.exe" || exit /b 1
out\faultwhere_test.exe
