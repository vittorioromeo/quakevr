#!/bin/bash
# flatmove_test.sh <agent> -- flatscreen play's crouch speed and swimming (-flatscreenmode; docs/vr-port/FLATPLAY.md,
# "Crouched, slower" and "Swimming"), headless, through the client's own keys (+forward, +flatcrouch, +jump; the look
# set with setpos) and measured by vr_flatmove_test (QC/vr_flatmove_test.qc). Prints PASS/FAIL, exits 1 on a failure.
#   crouch  running (cl_alwaysrun 1, as he plays) on the start map, standing and with Left Ctrl held: the crouched top speed
#           over the standing one is the headset's (its mock head at 1.69 m and at 0.9 m, the stick full ahead), within
#           0.01: the same rule and tunables (vr_crouch_speed, vr_crouch_speed_scale). A flat-screen player's eyes stand
#           at Quake's viewheight, under the tallest crouched box's top, and slowed him standing (260 of 320; crouched 240).
#   swim    vanilla swimming in e1m4's deep water: half a second of +forward looking 60 degrees up rises, looking 60 down
#           sinks, and +jump looking level rises (Quake's own swim up), each more than the headset's slow stick
#           (vr_swim_stick_speed) would give; the headset's own swimming is left as it is (its stick looking up rises
#           under three quarters of the flat-screen's rise in the same half second: 38 of 78).
#   ledge   the start map's pool: swimming at its low edge (+forward) lifts him out onto dry ground by Quake's jump out of
#           the water (FL_WATERJUMP), which the headset leaves to the hands (vr_water_jump 0, vr_climb).
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
Q='^flatmove: (speed|swim|out|timed out)'
run() { local tag=$1 flat=$2 script=$3; local a=(); [ "$flat" = flat ] && a=(-ExtraArgs "-flatscreenmode")
        bash $KIT/run.sh $AGENT "${a[@]}" -QuitOn "$Q" -Timeout 120 -Filter "^flatmove" -Script "$script; wait600; toggleconsole; quit" > "$S/fm_$tag.log" 2>&1; }
val() { grep -a "^flatmove: $2" "$S/fm_$1.log" | head -1 | sed "s/.*$2 \([-0-9.]*\).*/\1/"; }

# 1. crouch speed
P="map start; wait60; god; notarget; cl_alwaysrun 1; vr_stamina_speed 0; wait10"
run fstand flat "$P; +forward; vr_flatmove_test 1"
run fcrouch flat "$P; +flatcrouch; wait40; +forward; vr_flatmove_test 1"
H="map start; wait60; god; notarget; vr_stamina_speed 0; vr_mock_look 0 90"
run hstand vr "$H; vr_mock_hand head 0 1.69 0; wait30; vr_mock_stick off 0 1; vr_flatmove_test 1"
run hcrouch vr "$H; vr_mock_hand head 0 0.9 0; wait30; vr_mock_stick off 0 1; vr_flatmove_test 1"
fs=$(val fstand speed); fc=$(val fcrouch speed); hs=$(val hstand speed); hc=$(val hcrouch speed)
check $(awk -v a="$fs" -v b="$fc" -v c="$hs" -v d="$hc" 'BEGIN{exit !(a>0 && c>0 && b<a && (b/a-d/c)<0.01 && (d/c-b/a)<0.01)}' && echo 1 || echo 0) \
    "crouch: flat-screen crouched $fc of standing $fs, the headset's $hc of $hs (the same share within 0.01)"
check $(awk -v a="$fs" -v c="$hs" 'BEGIN{exit !(a-c<1 && c-a<1)}' && echo 1 || echo 0) \
    "crouch: flat-screen standing runs at the headset's standing speed ($fs, $hs): not slowed as if crouched"

# 2. swimming by look and jump (e1m4's deep water, 128 units of open water over and under him: vr_flatmove_test 8 found it)
W="map e1m4; wait60; god; notarget; vr_stamina_speed 0"
run swup flat "$W; setpos 296 1228 650 -60 0 0; noclip; wait5; +forward; vr_flatmove_test 2"
run swdown flat "$W; setpos 296 1228 650 60 0 0; noclip; wait5; +forward; vr_flatmove_test 2"
run swjump flat "$W; setpos 296 1228 650 0 0 0; noclip; wait5; +jump; vr_flatmove_test 2"
run swvr vr "$W; setpos 296 1228 650 0 0 0; noclip; vr_mock_look -60 0; wait5; vr_mock_stick off 0 1; vr_flatmove_test 2"
up=$(val swup "swim dz"); down=$(val swdown "swim dz"); jump=$(val swjump "swim dz"); vr=$(val swvr "swim dz")
lv=$(grep -ah "^flatmove: swim" "$S/fm_swup.log" "$S/fm_swdown.log" "$S/fm_swjump.log" | grep -c "waterlevel 3")
check $(awk -v u="$up" -v n="$lv" 'BEGIN{exit !(u>50 && n==3)}' && echo 1 || echo 0) "swim: looking up, forward rises $up units in half a second (under water all the way: $lv of 3)"
check $(awk -v d="$down" 'BEGIN{exit !(d<-50)}' && echo 1 || echo 0) "swim: looking down, forward sinks $down units"
check $(awk -v j="$jump" 'BEGIN{exit !(j>25)}' && echo 1 || echo 0) "swim: jump held, looking level, rises $jump units"
check $(awk -v v="$vr" -v u="$up" 'BEGIN{exit !(v!="" && v>0 && v<0.75*u)}' && echo 1 || echo 0) \
    "swim in the headset: as it was, the slow stick looking up rises $vr units (under 3/4 of the flat-screen's $up)"

# 3. out of the water onto the start map's pool's low edge
run ledge flat "map start; wait60; god; notarget; setpos 1344 1728 86 0 180 0; noclip; wait5; +forward; vr_flatmove_test 3"
out=$(grep -a "^flatmove: \(out\|timed\)" "$S/fm_ledge.log" | head -1)
check $(grep -q "^flatmove: out .* waterjumped 1" <<<"$out" && echo 1 || echo 0) "ledge: swimming at the pool's edge lifts him out onto dry ground (${out:-no line})"

verdict; exit $fail
