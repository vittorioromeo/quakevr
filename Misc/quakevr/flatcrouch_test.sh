#!/bin/bash
# flatcrouch_test.sh <agent> -- flatscreen play's crouch eases (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Crouching"):
#   ease    vr_flat_crouch_trace prints the drawn eye's and the main hand's height every frame (the shotgun held: impulse
#           154) while +flatcrouch is held 40 frames and let go (the ease takes 18 frames each way at the default 0.25 s),
#           and its verdict: neither stepped more in a frame than the ease allows (the eye: the drop over the crouch's time
#           at the ease's steepest; the hand: twice that). Before the fix the eye fell 7 units in a frame, twice: the
#           headroom cap pulled the view under each crouched box its own lowered eyes asked for.
#   depth   the eye goes the whole vr_flat_crouch_drop (18) down and comes all the way back.
#   offsets Gun Down Crouched -11 (vr_flat_gun_down_crouch: 6 higher than its default -5) puts the crouched hand at least 5 higher.
# Prints PASS/FAIL, exits 1 on a failure.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
trace() { bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^flatcrouch: trace done" -Timeout 120 -Filter "^flatcrouch" \
    -Script "map start; wait60; god; notarget; impulse 154; wait30; $2 vr_flat_crouch_trace 90; +flatcrouch; wait40; -flatcrouch; wait200; toggleconsole; quit" > "$S/fc_$1.log" 2>&1; }
trace base ""
trace up "vr_flat_gun_down_crouch -11;"
L="$S/fc_base.log"
done_line=$(grep -a "^flatcrouch: trace done" "$L" | head -1)
check $(grep -q "PASS$" <<<"$done_line" && echo 1 || echo 0) "ease: ${done_line:-no verdict line}"
range=$(grep -a "^flatcrouch: frame" "$L" | awk '{e=$9; if(NR==1){a=e; lo=e} if(e<lo) lo=e; z=e} END{printf "%.2f %.2f %.2f", a, lo, z}')
read first low last <<<"$range"
check $(awk -v a="$first" -v l="$low" -v z="$last" 'BEGIN{exit !(a!="" && a-l>17.5 && a-z<0.25 && z-a<0.25)}' && echo 1 || echo 0) \
    "depth: the eye went from $first down to $low and back to $last (the full 18 down, all the way back)"
h0=$(grep -a "^flatcrouch: frame 30 " "$S/fc_base.log" | awk '{print $12}'); h1=$(grep -a "^flatcrouch: frame 30 " "$S/fc_up.log" | awk '{print $12}')
up_done=$(grep -a "^flatcrouch: trace done" "$S/fc_up.log" | head -1)
check $(awk -v a="$h0" -v b="$h1" 'BEGIN{exit !(a!="" && b!="" && b-a>=5)}' && grep -q "PASS$" <<<"$up_done" && echo 1 || echo 0) \
    "offsets: crouched, the hand at $h0, with Gun Down Crouched -11 at $h1 (its trace: ${up_done##*: })"
verdict; exit $fail
