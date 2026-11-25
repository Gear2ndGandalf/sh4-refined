@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /W3 /MT test_matrix.c /Fo"out\\" /Fe"out\test_matrix.exe" || exit /b 1
out\test_matrix.exe
