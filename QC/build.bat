@echo off
rem Builds ..\quakevr\progs.dat. Needs FTEQCC: set FTEQCC=path\to\fteqcc64.exe, or have fteqcc64 on PATH.
rem Then the QC's checks with Tools\qvr_checks (..\build\qvr_checks\qvr_checks.exe, built by the Visual Studio build
rem or the kit's build.sh; skipped without it), and that TrenchBroom's entity definitions cover every spawn function
rem (Misc\trenchbroom\fgdgen.py --check, needs Python 3 on PATH; skipped without it; set NO_FGD_CHECK=1 to skip it).
setlocal
if "%FTEQCC%"=="" set FTEQCC=fteqcc64
cd /d "%~dp0"
"%FTEQCC%" -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
if errorlevel 1 exit /b 1
set CHK=..\build\qvr_checks\qvr_checks.exe
if not exist "%CHK%" (
    echo build.bat: no %CHK%: skipped the progs format, release QC and precedence checks
    goto fgd
)
"%CHK%" --tree .. progs_format quakevr\progs.dat
if errorlevel 1 exit /b 1
rem The release flavour's QC (-DQVR_NO_TESTS, to a work folder: progs.dat is not touched) compiles with no error or
rem warning (qvr_checks release_qc; ~3 s; set NO_RELEASE_QC_CHECK=1 to skip it).
if "%NO_RELEASE_QC_CHECK%"=="" (
    "%CHK%" --tree .. --fteqcc "%FTEQCC%" release_qc
    if errorlevel 1 exit /b 1
)
rem No expression that reads differently under C's operator priorities (docs\vr-port\CODE_STYLE.md, QuakeC; ~10 s;
rem set NO_PREC_CHECK=1 to skip it).
if "%NO_PREC_CHECK%"=="" (
    "%CHK%" --tree .. --fteqcc "%FTEQCC%" qc_precedence
    if errorlevel 1 exit /b 1
)
:fgd
if not "%NO_FGD_CHECK%"=="" exit /b 0
where python >nul 2>nul
if errorlevel 1 (
    echo build.bat: no Python: skipped the TrenchBroom FGD check ^(Misc\trenchbroom\fgdgen.py --check^)
    exit /b 0
)
python ..\Misc\trenchbroom\fgdgen.py --check
