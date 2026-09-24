#!/bin/sh
# Builds ../quakevr/progs.dat. Needs FTEQCC: FTEQCC=/path/to/fteqcc, or fteqcc on PATH.
set -e
cd "$(dirname "$0")"
"${FTEQCC:-fteqcc}" -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
