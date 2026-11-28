@echo off
rem v3.59 CT359: builds and runs the native test of src\omnilatch_sim.h (the control-direction latch's release rule, its
rem triggers, the ini texts of [Input] OmniLatch / [Gameplay] LockOnWalk, Lock-On Walk's step gate) and test\omnilatch_sim_test.c,
rem x86 /O2 as build.cmd.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /MT /W3 omnilatch_sim_test.c /Fo"out\\" /Fe"out\omnilatch_sim_test.exe" || exit /b 1
out\omnilatch_sim_test.exe
