#!/bin/bash
# flatswap_test.sh <agent> [shots-prefix] -- flat-screen play's swap key (3) with every two-handed hold and with carried
# things (his words 2026-11-11; Quake/vr/vr_flatswap.cpp, QC vr_wpnholster.qc VR_FlatSwapHands, QC vr_flatswap_test.qc).
# Headless, fast mode, -flatscreenmode; each run ends on its own verdict line (-QuitOn).
#   weapons  vr_flatswap_test 1: each weapon alone in the right hand swaps the two-handed way, ends in the left
#   things   vr_flatswap_test 2: props with weapons and each other: each hand ends with what the other held
#   melee    vr_flatswap_test 3: each melee weapon (AD's too, a second run with AD's data) swapped both ways, the stance
#            on and off: the weapon and both hands seen every frame (the engine's watch, vr_flat_swap_vis_faults)
# With a shots prefix, also the strips (vr_flatswap_test 11..17: the rocket launcher, the super shotgun, the nailgun, the
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
run melee 3 || true
grep -aq "flatswap_test: [0-9]* passed, 0 failed" "$S/melee.log" || fails=$((fails + 1))
# (The axe given at the console with the spawn's axe in the hip holster, the stance off: one swap watched. Until
# 2026-11-12 the axe was eased in from that holster, turned 177 degrees, out of sight for 9 frames: vr_drawblend.cpp.)
H="$BASE; vr_flat_melee_2h 0; impulse 152; wait90; vr_flat_swap_debug 1; vr_mock_key 3 down; wait2; vr_mock_key 3 up"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$H" -QuitOn "^flatswapvis: done" -Timeout 120 \
    -Filter "^(flatswapvis|Host_Error)" > "$S/holster.log" 2>&1
grep -a "flatswapvis: done" "$S/holster.log" | cut -c1-120 | sed "s/^/holster: /"
grep -aq "flatswapvis: done, [0-9]* frames, 0 faults" "$S/holster.log" || fails=$((fails + 1))
# (AD's six melee weapons, vr_flatswap_test 4: 48 checks with AD's data mounted (vr_ad_mounted): on AD's start map (the
# spot: `map start`, the player's spawn; in vrtesthall it says "AD's data not mounted" and checks nothing). Slow: its 12
# cases take about 4 minutes at the default fast pace, hence the long timeout.)
bash $KIT/run.sh $AGENT -Games "-game ad -game quakevr" -ExtraArgs "-flatscreenmode -nomapindex -noaddons" \
    -Script "map start; god; notarget; developer 1; wait5; vr_flatswap_test 4" -QuitOn "$V" -Timeout 600 -Filter "$FILTER" > "$S/melee_ad.log" 2>&1
grep -a "FAIL\|Assertion\|Host_Error" "$S/melee_ad.log" | cut -c1-200
grep -a "$V" "$S/melee_ad.log" | tail -1 | sed "s/^/melee_ad: /"
grep -aq "flatswap_test: [0-9]* passed, 0 failed" "$S/melee_ad.log" || fails=$((fails + 1))
if [ -n "$PREFIX" ]; then
    for k in 11:rl 12:ssg 13:ng 14:axe 15:sgprop 16:gl 17:adplasma; do
        run "strip_${k#*:}" "${k%%:*}" "-Out ${PREFIX}_${k#*:}.png -Clean"
        grep -a "composed" "$S/strip_${k#*:}.log"
    done
fi
echo "flatswap_test.sh: $fails run(s) failed"
exit $fails
