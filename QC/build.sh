#!/bin/sh
# Builds ../quakevr/progs.dat. Needs FTEQCC: FTEQCC=/path/to/fteqcc, or fteqcc on PATH.
# Then checks that TrenchBroom's entity definitions cover every spawn function (Misc/trenchbroom/fgdgen.py
# --check, needs Python 3; skipped without it, NO_FGD_CHECK=1 skips it).
set -e
cd "$(dirname "$0")"
"${FTEQCC:-fteqcc}" -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
if [ -z "$NO_FGD_CHECK" ]; then
    PY=$(command -v python3 || command -v python || true)
    if [ -n "$PY" ]; then
        "$PY" ../Misc/trenchbroom/fgdgen.py --check
    else
        echo "build.sh: no Python: skipped the TrenchBroom FGD check (Misc/trenchbroom/fgdgen.py --check)"
    fi
fi
