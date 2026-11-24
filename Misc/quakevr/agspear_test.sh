#!/bin/bash
# agspear_test.sh <agent> -- an armed grunt with the spear (spearfix; ROUND26.md "spearfix"): vr_agrunt_test 230
# (QC/vr_armedgrunt_spear_test.qc), headless, fast mode: held two-handed, its head at its enemy, its thrust hurts you; and
# the engine's vr_agholds_report line `agpole ...: both on the shaft` (each hand's grip within 3.5 units of the shaft).
# An eyeshot of its hold: <gamedir>/eyeshots (the first of the run). Ends `agspear: N passed, M failed`.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -Timeout 300 -Script "map vrtesthall; god; developer 1; wait5; setpos 0 300 25 0 0 0; wait5; vr_agrunt_test 230" \
    -QuitOn "^agtest: test 230 over" -Filter "(^agtest|^agpole)")
echo "$out"
check $(grep -aq "^agtest: test 230 over: [0-9]* passed, 0 failed" <<<"$out" && echo 1 || echo 0) "the spear grunt's QC checks (vr_agrunt_test 230)"
check $(grep -aq "^agpole .*: both on the shaft" <<<"$out" && echo 1 || echo 0) "both its hands on the spear's shaft (agpole)"
verdict
exit $fail
