@echo off
rem v3.53 OL1: builds and runs the native test of src\ol_words.h (the Options layout's words: the plan's limits, the screen / ini word maps' round trip, the composed lines) and test\ol_words_test.c, x86 /O2 as build.cmd.
rem src\ol_words.h is generated: py -3 tools\gen_ol_words.py (from staging\options_layout\tools\layout_data.py).
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 ol_words_test.c /Fo"out\\" /Fe"out\ol_words_test.exe" || exit /b 1
out\ol_words_test.exe
