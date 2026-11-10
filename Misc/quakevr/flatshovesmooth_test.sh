#!/bin/bash
# flatshovesmooth_test.sh <agent> -- flat-screen play: the shove's fast smoothing (vr_flat_shove_smooth; his note 2026-11-10,
# "Yes, please add some fast smoothing": the push moved the free hand up to 25 units in a frame). Headless, -flatscreenmode,
# fixed frames at 60 and at 144 fps (vr_fixed_frames_rate), the smoothing off (0) and on (its default): one shove of the off
# hand from the rocket launcher's two-handed grip and one from the axe's (its grip up the handle taken with
# vr_2h_angle_threshold -1, the threshold reset once held), each followed by vr_flat_fg_report's "game off hand: most N units a
# frame" (the hand the game gets, off the head) and "drawn off hand" (as drawn, off the main hand). Checks per rate and weapon:
# smoothed, the most a frame is at most 80% of the unsmoothed one; the shove still lands (QC's "melee event: flat shove") the
# same number of times. Then two strips at 72 fps of the launcher's push, a frame apart from its start (off, then on):
# scratch/flatshove_0.png, scratch/flatshove_1.png. Prints "flatshovesmooth_test: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
QT_NAME=flatshovesmooth_test
S=$TREE/scratch; mkdir -p "$S"
F="flatfgrab: (game|drawn) off hand|^fss |melee event: flat shove"
REP="vr_flat_fg_report; wait2"
ON=${2:-0.08} # (the smoothing tried: its default)
run() # run <tag> <script> [extra run.sh args...]
{
    local tag=$1 script=$2; shift 2
    bash "$KIT/run.sh" "$AGENT" -ExtraArgs "-flatscreenmode" -Timeout 200 -QuitOn "^fss done" -Script "$script; echo fss done; toggleconsole; quit" -Filter "$F" "$@" > "$S/fss_$tag.log" 2>&1
}
for rate in 60 144; do
    for sm in 0 $ON; do
        SC="map vrtesthall; wait60; developer 1; god; notarget; setpos 0 300 25 0 90 0; vr_fixed_frames_rate $rate; vr_flat_shove_smooth $sm; wait5"
        SC="$SC; impulse 160; wait150; echo fss launcher; $REP; vr_flat_shove_now; wait150; $REP"
        SC="$SC; impulse 150; impulse 170; wait5; vr_2h_angle_threshold -1; impulse 152; wait100; reset vr_2h_angle_threshold; wait3"
        SC="$SC; echo fss axe; $REP; vr_flat_shove_now; wait150; $REP"
        run "${rate}_$sm" "$SC"
    done
done
# The second report after a weapon's marker: its shove's most a frame (the first one resets the probe).
most() # most <log> <weapon> <game|drawn>
{
    awk -v m="^fss $2" -v k="flatfgrab: $3 off hand" '$0 ~ m {on=1; n=0; next} on && $0 ~ k {n++; if (n == 2) {match($0, /most [0-9.]+/); print substr($0, RSTART + 5, RLENGTH - 5); exit}}' "$1"
}
for rate in 60 144; do
    for w in launcher axe; do
        g0=$(most "$S/fss_${rate}_0.log" $w game); g1=$(most "$S/fss_${rate}_$ON.log" $w game)
        d0=$(most "$S/fss_${rate}_0.log" $w drawn); d1=$(most "$S/fss_${rate}_$ON.log" $w drawn)
        echo "flatshovesmooth_test: $rate fps, $w: game hand most ${g0:-?} -> ${g1:-?} units a frame, drawn ${d0:-?} -> ${d1:-?}"
        check $(awk -v a="$g0" -v b="$g1" 'BEGIN{print (a != "" && b != "" && b + 0 <= 0.8 * a) ? 1 : 0}') "$rate fps, $w: the shove's hand smoothed (game, most a frame ${g0:-?} -> ${g1:-?})"
    done
    l0=$(grep -ac "melee event: flat shove" "$S/fss_${rate}_0.log"); l1=$(grep -ac "melee event: flat shove" "$S/fss_${rate}_$ON.log")
    check $([ "$l0" = "$l1" ] && [ "$l0" -ge 2 ] && echo 1 || echo 0) "$rate fps: the shoves land as before ($l0 -> $l1)"
done
# The strips: the launcher's shove at 72 fps, a picture a frame from its start (the windup is 9 frames, the push 4 or 5).
for sm in 0 1; do
    v=0; [ $sm = 1 ] && v=$ON
    SC="map vrtesthall; wait60; developer 1; god; notarget; setpos 0 300 25 0 90 0; vr_flat_shove_smooth $v; wait5; impulse 160; wait150; vr_flat_shove_now; wait6"
    for i in 1 2 3 4 5 6 7 8 9 10; do SC="$SC; screenshot; wait1"; done
    run "strip$sm" "$SC; wait60" -Clean -Out "flatshove_$sm.png"
done
verdict
exit $fail
