@echo off
rem v3.56 PV356: builds and runs the native Effects-preview test (test\preview_sim_test.c over src\preview_sim.h), x86 /O2
rem as build.cmd. Usage: build_pv_test.cmd [--dump DIR]  (the dump feeds tools\pv_mockup.py)
rem v3.59 PV359: then tools\pvlayer_check.py on the sample F7 set the test wrote (out\pv359_sample.dds / .heat / .spirit):
rem the Python reader of the layer files agrees with pvlayer_sim.h
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 preview_sim_test.c /Fo"out\\" /Fe"out\preview_sim_test.exe" || exit /b 1
out\preview_sim_test.exe %* || exit /b 1
py -3 -I ..\tools\pvlayer_check.py out\pv359_sample.dds --expect both || exit /b 1
