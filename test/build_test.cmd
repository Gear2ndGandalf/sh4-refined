@echo off
rem Builds and runs the native test of src\interp.c (x86, like the dll).
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>nul || exit /b 1
cd /d "%~dp0"
if not exist out_test mkdir out_test
cl /nologo /Od /Zi /W3 /MT test_interp.c /Foout_test\ /Fdout_test\ /Feout_test\test_interp.exe /link /NOLOGO user32.lib || exit /b 1
out_test\test_interp.exe
