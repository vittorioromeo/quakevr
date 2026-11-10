#!/bin/sh
# Builds ../quakevr/progs.dat. Needs FTEQCC with the fte32 flag: FTEQCC=/path/to/fteqcc, or fteqcc on PATH (on Windows:
# ../Tools/fteqcc/fteqcc64.exe by default; elsewhere build it with Tools/fteqcc/fte32.patch).
# Then the QC's checks: Tools/qvr_checks (build/qvr_checks/qvr_checks.exe, built by the Visual Studio build or the
# kit's build.sh: Windows only; elsewhere only the progs format is checked), and TrenchBroom's entity definitions
# cover every spawn function (Misc/trenchbroom/fgdgen.py --check, needs Python 3; skipped without it, NO_FGD_CHECK=1
# skips it).
set -e
cd "$(dirname "$0")"
# On Windows the repo's own FTEQCC is the default (Tools/fteqcc/README.md: speed-patched, byte-identical output).
if [ -z "$FTEQCC" ] && [ -f ../Tools/fteqcc/fteqcc64.exe ]; then
    case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) FTEQCC=../Tools/fteqcc/fteqcc64.exe ;; esac
fi
"${FTEQCC:-fteqcc}" -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
PY=$(command -v python3 || command -v python || true)
CHK=../build/qvr_checks/qvr_checks.exe
[ -x "$CHK" ] || CHK=
# The 32-bit progs format (QC/defs.qc's fte32 pragma, a flag of our FTEQCC: Tools/fteqcc/README.md): a 16-bit
# progs.dat fails the build. Prints the globals count.
if [ -n "$CHK" ]; then
    "$CHK" --tree .. progs_format quakevr/progs.dat
else
    sh ../Tools/qvr_checks/progs_format.sh ../quakevr/progs.dat
fi
# The release flavour's QC (-DQVR_NO_TESTS, to a work folder: progs.dat is not touched) compiles with no error or
# warning (qvr_checks release_qc; ~3 s; NO_RELEASE_QC_CHECK=1 skips it).
if [ -z "$NO_RELEASE_QC_CHECK" ] && [ -n "$CHK" ]; then
    "$CHK" --tree .. --fteqcc "${FTEQCC:-fteqcc}" release_qc
fi
# No expression that reads differently under C's operator priorities (docs/vr-port/CODE_STYLE.md, QuakeC; qvr_checks
# qc_precedence, ~10 s; NO_PREC_CHECK=1 skips it).
if [ -z "$NO_PREC_CHECK" ] && [ -n "$CHK" ]; then
    "$CHK" --tree .. --fteqcc "${FTEQCC:-fteqcc}" qc_precedence
fi
[ -n "$CHK" ] || echo "build.sh: no build/qvr_checks/qvr_checks.exe (Windows): skipped the release QC and precedence checks"
if [ -z "$NO_FGD_CHECK" ]; then
    if [ -n "$PY" ]; then
        "$PY" ../Misc/trenchbroom/fgdgen.py --check
    else
        echo "build.sh: no Python: skipped the TrenchBroom FGD check (Misc/trenchbroom/fgdgen.py --check)"
    fi
fi
