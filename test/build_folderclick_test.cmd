@echo off
rem v3.56 MN356: builds and runs the native test of src\folderclick_sim.h (test\folderclick_test.c: a mouse click on a folder row's label, triangle or arrow place opens it at every shape 4:3 .. 32:9, window size and row count; a combo row on its label; refused mid-slide, the next click opens), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 folderclick_test.c /Fo"out\\" /Fe"out\folderclick_test.exe" || exit /b 1
out\folderclick_test.exe
