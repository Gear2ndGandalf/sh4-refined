@echo off
rem v3.32 PM332: builds and runs the native test of src\em_seam_sim.h and the hand-over tick model (test\em_seam_test.c), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 em_seam_test.c /Fo"out\\" /Fe"out\em_seam_test.exe" || exit /b 1
out\em_seam_test.exe
