#!/bin/bash
# flatswap_test.sh <agent> [shots-prefix] -- flat-screen play's swap key (3) with every two-handed hold and with carried
# things (his words 2026-11-11; Quake/vr/vr_flatswap.cpp, QC vr_wpnholster.qc VR_FlatSwapHands, QC vr_flatswap_test.qc).
# Headless, fast mode, -flatscreenmode; each run ends on its own verdict line (-QuitOn).
#   weapons  vr_flatswap_test 1: each weapon alone in the right hand swaps the two-handed way, ends in the left
#   things   vr_flatswap_test 2: props with weapons and each other: each hand ends with what the other held
# With a shots prefix, also the strips (vr_flatswap_test 11..15: the rocket launcher, the super shotgun, the nailgun, the
# axe, the shotgun with a brick in the left hand), each composed into scratch/<prefix>_<name>.png.
AGENT=${1:?agent}
PREFIX=$2
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatswap
mkdir -p "$S"
FILTER="^(flatswap_test|flat swap|handslot|Host_Error|ENGINE CRASH)|Assertion"
BASE="map vrtesthall; god; notarget; developer 1; wait5; setpos 0 300 25 0 90 0; wait5"
V="^flatswap_test: [0-9]+ passed"
run() { local tag=$1 ask=$2 extra=$3
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode $extra -Script "$BASE; vr_flatswap_test $ask" \
            -QuitOn "$V" -Timeout 180 -Filter "$FILTER" > "$S/$tag.log" 2>&1
        grep -a "FAIL\|Assertion\|Host_Error" "$S/$tag.log" | cut -c1-200
        grep -a "$V" "$S/$tag.log" | tail -1 | sed "s/^/$tag: /"; }
fails=0
run weapons 1 || true
grep -aq "flatswap_test: [0-9]* passed, 0 failed" "$S/weapons.log" || fails=$((fails + 1))
run things 2 || true
grep -aq "flatswap_test: [0-9]* passed, 0 failed" "$S/things.log" || fails=$((fails + 1))
if [ -n "$PREFIX" ]; then
    for k in 11:rl 12:ssg 13:ng 14:axe 15:sgprop; do
        run "strip_${k#*:}" "${k%%:*}" "-Out ${PREFIX}_${k#*:}.png -Clean"
        grep -a "composed" "$S/strip_${k#*:}.log"
    done
fi
echo "flatswap_test.sh: $fails run(s) failed"
exit $fails
