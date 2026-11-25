@echo off
rem Builds out\winmm.dll (x86, static CRT): the SH4 Window Mod, a winmm.dll proxy for the SILENT HILL 4 game folder.
rem src\winmm.def and src\winmm_exports.inc come from tools\gen_exports.py (run it again only on a new Windows build
rem whose SysWOW64\winmm.dll exports change). Usage: build.cmd [outdir]   (default out)
rem v2.1 prototype: src\interp.c = FRAME INTERPOLATION; its native test: test\build_test.cmd
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
set OUT=%~1
if "%OUT%"=="" set OUT=out
if not exist "%OUT%" mkdir "%OUT%"
rc /nologo /fo "%OUT%\winmm.res" src\winmm.rc || exit /b 1
cl /nologo /LD /O2 /MT /W3 src\sh4windowmod.c src\interp.c /Fo"%OUT%\\" /Fe"%OUT%\winmm.dll" /link /NOLOGO /DEF:src\winmm.def /NODEFAULTLIB:winmm.lib user32.lib gdi32.lib "%OUT%\winmm.res" || exit /b 1
echo built %OUT%\winmm.dll
