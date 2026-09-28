@echo off
rem Builds ..\quakevr\progs.dat. Needs FTEQCC: set FTEQCC=path\to\fteqcc64.exe, or have fteqcc64 on PATH.
rem Then checks that TrenchBroom's entity definitions cover every spawn function (Misc\trenchbroom\fgdgen.py
rem --check, needs Python 3 on PATH; skipped without it; set NO_FGD_CHECK=1 to skip it).
setlocal
if "%FTEQCC%"=="" set FTEQCC=fteqcc64
cd /d "%~dp0"
"%FTEQCC%" -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
if errorlevel 1 exit /b 1
if not "%NO_FGD_CHECK%"=="" exit /b 0
where python >nul 2>nul
if errorlevel 1 (
    echo build.bat: no Python: skipped the TrenchBroom FGD check ^(Misc\trenchbroom\fgdgen.py --check^)
    exit /b 0
)
python ..\Misc\trenchbroom\fgdgen.py --check
