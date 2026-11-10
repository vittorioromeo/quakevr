#!/bin/bash
# flatfg2h_test.sh <agent> [letgo] -- flat-screen play: the off hand lets go of a weapon held in both hands the same way for a
# melee weapon as for a gun (his note 2026-11-10: "when holding a melee weapon with two hands and force-grabbing, the off
# hand doesn't seem to detach first. It should animate like when holding a gun with two hands"; vr_flat_melee_letgo,
# vr_flatsight.cpp keptGrip). Headless, fast mode, -flatscreenmode; [letgo] sets vr_flat_melee_letgo (default 1; 0 the old
# jump, for the before pictures). Three runs, each ending on its own last line (-QuitOn):
#   1. vr_flatitems_test 9 (QC): the rocket launcher, then the axe, in both hands: E on a brick (the grip let go before the
#      brick flies, the hand off it then, the flight after the palm's turn and the grip's detach), then the shove; the E strip
#      (8 screenshots from the press, every 4 frames: the launcher's, then the axe's) composed into scratch/flatfg2h_<letgo>.png.
#   2, 3. the launcher, then the axe, by the keys: H (drop), G (the quick grenade), 3 (the swap), each from the two-handed hold.
# Each action's drawn off hand (vr_flat_fg_report's "drawn off hand: most N units a frame", view::drawnHandPose, off the main
# hand) is checked: the axe's no more than the launcher's for the same action and a unit (a hand that jumps off the grip is
# 10 and more). The axe's grip (hs1, up the handle) is taken with vr_2h_angle_threshold -1 and the threshold reset once it
# holds (a swing's end gives it the same hold at rest). Prints "flatfg2h_test: N passed, M failed".
AGENT=${1:?agent}
LETGO=${2:-1}
source "$(dirname "$0")/qvrtest.sh"
QT_NAME=flatfg2h_test
S=$TREE/scratch; mkdir -p "$S"
F="^flatitems|^flatfg2h|flatfgrab: drawn off hand|two hands:|Assertion|ENGINE CRASH|Host_Error"
BASE="map vrtesthall; wait60; developer 1; god; notarget; setpos 0 300 25 0 90 0; wait5; vr_flat_melee_letgo $LETGO"
run() # run <tag> <script> <quit-on> [extra run.sh args...]
{
    local tag=$1 script=$2 quit=$3; shift 3
    bash "$KIT/run.sh" "$AGENT" -ExtraArgs "-flatscreenmode" -Timeout 240 -QuitOn "$quit" -Script "$script" -Filter "$F" "$@" > "$S/fg2h_$tag.log" 2>&1
}

run qc "$BASE; vr_flatitems_test 9" "flatitems: [0-9]+ passed" -Clean -Out "flatfg2h_$LETGO.png"
grep -a "FAIL" "$S/fg2h_qc.log" | cut -c1-200
expect_verdict "$(cat "$S/fg2h_qc.log")" "flatitems" "E and the shove from the launcher's and the axe's two-handed hold"

# The keys: each from the hold, its window, the drawn hand's most a frame (the report before it resets the probe).
key() { echo "vr_mock_key $1 down; wait2; vr_mock_key $1 up"; }
REP="vr_flat_fg_report; wait2"
THROW="+offhandattack; wait3; -offhandattack; wait10; +offhandattack; wait50; -offhandattack; wait90" # (G's grenade thrown)
for w in launcher axe; do
    if [ $w = launcher ]; then
        GIVE="impulse 160; wait90"
    else
        GIVE="vr_2h_angle_threshold -1; impulse 152; wait60; reset vr_2h_angle_threshold; wait2"
    fi
    SC="$BASE; give r 10"
    for k in h g 3; do
        SC="$SC; impulse 150; impulse 170; wait5; $GIVE; echo flatfg2h key $w $k; vr_flatplay_status; $REP; $(key $k); wait36; $REP"
        [ $k = g ] && SC="$SC; $THROW"
    done
    run $w "$SC; echo flatfg2h keys done; toggleconsole; quit" "^flatfg2h keys done"
done

# The drawn off hand's most a frame after each action: the report that follows the action's own line.
most() # most <log> <marker regex>
{
    awk -v m="$2" '$0 ~ m {on=1; n=0; next} on && /flatfgrab: drawn off hand/ {n++; if (n == 2) {match($0, /most [0-9.]+/); print substr($0, RSTART + 5, RLENGTH - 5); exit}}' "$1"
}
mostQc() # mostQc <action>: the QC run's, the report right after its "flatfg2h: <weapon>, <action>:" line
{
    awk -v m="^flatfg2h: $1, $2:" '$0 ~ m {on=1; next} on && /flatfgrab: drawn off hand/ {match($0, /most [0-9.]+/); print substr($0, RSTART + 5, RLENGTH - 5); exit}' "$S/fg2h_qc.log"
}
cmp() # cmp <what> <launcher's> <axe's> [share]: the axe's within the launcher's (times share, 1) and a unit
{
    local ok
    ok=$(awk -v g="$2" -v a="$3" -v k="${4:-1}" 'BEGIN{print (g != "" && a != "" && a + 0 <= g * k + 1.0) ? 1 : 0}')
    check "$ok" "$1: the axe's off hand leaves the grip as the launcher's (drawn, most a frame: axe ${3:-?}, launcher ${2:-?})"
    echo "flatfg2h_test: $1 launcher ${2:-?} axe ${3:-?}"
}
for a in E shove; do
    cmp "$a" "$(mostQc launcher $a)" "$(mostQc axe $a)"
done
for k in h g 3; do
    held=$(awk -v m="^flatfg2h key axe $k" '$0 ~ m {on=1; next} on && /two hands:/ {print; exit}' "$S/fg2h_axe.log" | grep -c "(held")
    check "$held" "key $k: the axe held in both hands at the press"
    # (H drops the weapon: no grip is left to ease off, the free hand goes to its guard at once for both, the axe's from
    # further down: within a quarter more.)
    share=1; [ $k = h ] && share=1.25
    cmp "key $k" "$(most "$S/fg2h_launcher.log" "^flatfg2h key launcher $k")" "$(most "$S/fg2h_axe.log" "^flatfg2h key axe $k")" $share
done
verdict
exit $fail
