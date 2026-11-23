#!/bin/bash
# flatswim_test.sh <agent> -- flat-screen play's swimming strokes (-flatscreenmode; Quake/vr/vr_flatswim.cpp;
# docs/vr-port/FLATPLAY.md, "Swimming"). Headless, fast mode; each run ends on its own last line (-QuitOn), and
# flatswim_check.py prints the checks and "flatswim: N passed, M failed".
#   dirs     vrtesthall's pool (x 224..544, y -416..-96, water -128..-8): still (treading), forward, back, right, left (the
#            pool's diagonals, room for a full stroke), up (+jump: QC's 100 u/s) and down (+movedown at cl_upspeed 100): each
#            a probe of the free hands' paths and palms over the stroke (vr_flatswim_probe .. vr_flatswim_report)
#   twohand  the nail gun on land (the free hand on its hotspot: the stance), in the pool (one hand: the free hand swims),
#            and the trigger held there (the gun hand's stroke gives way, blends back after), then back on land (the
#            stance again)
# Screenshots: flatswim_shots.sh.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatswim
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(flatswim|  stance)"
run() { local tag=$1 script=$2
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$script; echo flatswim_test: $tag done; toggleconsole; quit" \
            -QuitOn "^flatswim_test: $tag done" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
BASE="map vrtesthall; god; notarget; wait5"
# (setpos turns noclip on: the noclip after it turns it off. The probe starts once the stroke has eased to the move:
# vr_flat_swim_turn_time 0.35 s, 25 frames; and spans 72 frames, most of a stroke at 0.9 a second.)
at() { echo "echo flatswim_test: case $1; setpos $2; noclip; wait20"; }
PROBE="vr_flatswim_probe; wait72; vr_flatswim_report"
run dirs "$BASE; cl_upspeed 100; $(at still '384 -256 -64 0 90 0'); wait20; $PROBE; \
$(at forward '240 -400 -64 0 45 0'); +forward; wait30; $PROBE; -forward; \
$(at back '528 -112 -64 0 45 0'); +back; wait30; $PROBE; -back; \
$(at right '240 -400 -64 0 135 0'); +moveright; wait30; $PROBE; -moveright; \
$(at left '528 -112 -64 0 135 0'); +moveleft; wait30; $PROBE; -moveleft; \
$(at up '384 -256 -100 0 90 0'); +jump; wait12; vr_flatswim_probe; wait45; vr_flatswim_report; -jump; \
$(at down '384 -256 -24 0 90 0'); +movedown; wait20; vr_flatswim_probe; wait50; vr_flatswim_report; -movedown"
NG="vr_test_give_wid 6; impulse 149; give n 200; wait90"
REP="vr_flatswim_report; vr_flatplay_status"
run twohand "$BASE; setpos 0 300 25 0 90 0; noclip; wait5; $NG; echo flatswim_test: case land; $REP; \
$(at water '384 -256 -64 0 90 0'); wait40; echo flatswim_test: case water; $REP; \
+attack; wait4; echo flatswim_test: case firing; vr_flatswim_report; wait20; echo flatswim_test: case fired; vr_flatswim_report; \
-attack; wait20; echo flatswim_test: case hold; vr_flatswim_report; wait60; echo flatswim_test: case back; vr_flatswim_report; \
$(at land2 '0 300 25 0 90 0'); wait90; echo flatswim_test: case land2; $REP"

python "$HERE/flatswim_check.py" "$S"
exit $?
