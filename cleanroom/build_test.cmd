@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>nul
cd /d "%~dp0"
cl /W4 /nologo test_pair.c
if errorlevel 1 exit /b 1
"%~dp0test_pair.exe"
exit /b %errorlevel%
