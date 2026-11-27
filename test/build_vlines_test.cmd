@echo off
rem v3.56 MN356: builds and runs the native test of src\vlines_sim.h and the plan's value lines in src\ol_words.h (test\vlines_test.c: the byte-mode compare, the text-id ring, the decision, the stock-word order rule, every line in house style, every named value with its own line), x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 vlines_test.c /Fo"out\\" /Fe"out\vlines_test.exe" || exit /b 1
out\vlines_test.exe
