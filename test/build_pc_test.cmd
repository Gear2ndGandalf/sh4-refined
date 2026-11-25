@echo off
rem PLAYER PACK: builds and runs the native test of the five Player-page cheats' rules (test\pc_rules_test.c over
rem src\pc_rules.h), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 pc_rules_test.c /Fo"out\\" /Fe"out\pc_rules_test.exe" || exit /b 1
out\pc_rules_test.exe
