@echo off
rem v3.40 LG340: builds and runs the native test of the session-log rotation (test\logrotate_test.c over src\logrotate_sim.h
rem and src\logrotate.inc, on its own folder test\out\lr_game), x86 /O2 /W3 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 logrotate_test.c /Fo"out\\" /Fe"out\logrotate_test.exe" || exit /b 1
out\logrotate_test.exe
