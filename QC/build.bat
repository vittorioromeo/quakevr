@echo off
rem Builds ..\quakevr\progs.dat. Needs FTEQCC: set FTEQCC=path	oteqcc64.exe, or have fteqcc64 on PATH.
setlocal
if "%FTEQCC%"=="" set FTEQCC=fteqcc64
cd /d "%~dp0"
"%FTEQCC%" -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
