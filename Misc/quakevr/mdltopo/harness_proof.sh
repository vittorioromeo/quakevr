#!/bin/bash
# harness_proof.sh [model] -- proves compare.py: self (0), renumbered (0), diagonal flip (0), broken / dropped (non-zero)
PY="/c/Program Files/Blender Foundation/Blender 5.2/5.2/python/bin/python.exe"
T="$(cd "$(dirname "$0")" && pwd)"
OUT=${OUT:-$T/../../../scratch/mdltopo}
M=${1:-$T/../../../quakevr/progs/v_shot.mdl}
S=$(basename "$M" .mdl)
"$PY" $T/make_variants.py "$M" $OUT/variants
echo "== self";    "$PY" $T/compare.py "$M" "$M"
echo "== renum";   "$PY" $T/compare.py "$M" $OUT/variants/${S}_renum.mdl
echo "== flip";    "$PY" $T/compare.py "$M" $OUT/variants/${S}_flip.mdl
echo "== broken";  "$PY" $T/compare.py "$M" $OUT/variants/${S}_broken.mdl --out $OUT/variants/${S}_broken_diff.png
echo "== drop";    "$PY" $T/compare.py "$M" $OUT/variants/${S}_drop.mdl --out $OUT/variants/${S}_drop_diff.png
