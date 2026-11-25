@echo off
rem v3.33 CAM333: builds and runs the native test of the over-the-shoulder camera (test\camera_sim_test.c over
rem src\camera_sim.h, with Playable Eileen's camera code transcribed beside it), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 camera_sim_test.c /Fo"out\\" /Fe"out\camera_sim_test.exe" || exit /b 1
out\camera_sim_test.exe