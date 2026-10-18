#!/bin/bash
# zonedrag_test.sh <agent> -- ROUND22.md, "Holster zones: dragged by hand, on the clone". e1m1, the mock hands, one run;
# prints PASS/FAIL, exits 1 on a failure:
#   move    vr_holster_zone_hip_drag 1: the main hand gripping in the right hip zone's middle, moved by a world offset:
#           vr_holster_zone_hip_x/y/z change by the hand's move in the body's frame (forward, right, up: the report's
#           hand point), to 0.1
#   resize  the off hand gripping the same zone too, 2 units below it, then 2 more: the spread twice, the hip zone's
#           depth, width and height twice theirs (the off hand across the body turns the body's yaw: the offsets
#           follow the main hand in the body's frame, not checked here)
#   left    the left hip zone (zone 2) dragged by the off hand: moved left (outwards there), y grows by as much
#   off     vr_holster_zone_hip_drag 0: a grip in the zone and a move leave the offsets as they were
AGENT=$1
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
RES=${RES:-C:/OHWorkspace/qvr-agents/$AGENT/scratch/zonedrag_result.txt}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
P="map e1m1;wait60;god;notarget;vr_holster_zones_show 1"
CV="vr_holster_zone_hip_x;vr_holster_zone_hip_y;vr_holster_zone_hip_z;vr_holster_zone_hip_depth;vr_holster_zone_hip_width;vr_holster_zone_hip_height"
MARK="echo ---"
S="$P;vr_holster_zone_hip_drag 1;vr_mock_hand_to main zonebox 3;wait5;$MARK;$CV;vr_holster_zones_report"
S="$S;vr_mock_button main grip 1;wait3;vr_mock_hand_to main by 3 2 4;wait5;$MARK;$CV;vr_holster_zones_report"
S="$S;vr_mock_hand_to off zonebox 3 0 0 -2;wait5;vr_mock_button off grip 1;wait3;vr_mock_hand_to off by 0 0 -2;wait5;$MARK;$CV;vr_holster_zones_report"
S="$S;vr_mock_button off grip 0;vr_mock_button main grip 0;wait5"
S="$S;vr_mock_hand_to off zonebox 2;wait5;$MARK;$CV;vr_holster_zones_report;vr_mock_button off grip 1;wait3"
S="$S;vr_mock_hand_to off by 2 3 1;wait5;$MARK;$CV;vr_holster_zones_report;vr_mock_button off grip 0;wait5"
S="$S;vr_holster_zone_hip_drag 0;vr_mock_hand_to main zonebox 3;wait5;$MARK;$CV;vr_mock_button main grip 1;wait3"
S="$S;vr_mock_hand_to main by 3 2 4;wait5;$MARK;$CV;vr_mock_button main grip 0;wait3"
log=$(bash $KIT/run.sh $AGENT -Script "$S;toggleconsole;quit" -Filter '^---|"vr_holster_zone_hip_|^zone hand (main|off):|rror')
echo "$log" > ${RES%.txt}_log.txt
# Per block (between the ---): the six cvars, and each hand's first point (forward right up from the eyes).
echo "$log" | awk '
/^--- *$/ { b++; next }
/"vr_holster_zone_hip_/ { split($0, q, "\""); v[b, q[2]] = q[4] }
/^zone hand main:/ { if (match($0, /points \(([-0-9. ]+)\)/)) { split(substr($0, RSTART + 8, RLENGTH - 9), p, " "); mf[b] = p[1]; mr[b] = p[2]; mu[b] = p[3] } }
/^zone hand off:/ { if (match($0, /points \(([-0-9. ]+)\)/)) { split(substr($0, RSTART + 8, RLENGTH - 9), p, " "); of[b] = p[1]; orr[b] = p[2]; ou[b] = p[3] } }
function ab(x) { return x < 0 ? -x : x }
function c(n) { return "vr_holster_zone_hip_" n }
END {
    dx = v[2, c("x")] - v[1, c("x")]; dy = v[2, c("y")] - v[1, c("y")]; dz = v[2, c("z")] - v[1, c("z")]
    hf = mf[2] - mf[1]; hr = mr[2] - mr[1]; hu = mu[2] - mu[1]
    printf "move: offsets moved %.2f %.2f %.2f, the hand %.2f %.2f %.2f\n", dx, dy, dz, hf, hr, hu
    ok = (ab(dx - hf) < 0.15 && ab(dy - hr) < 0.15 && ab(dz - hu) < 0.15 && ab(hu) > 1) ? 1 : 0
    print "RESULT " ok " move"
    s0 = sqrt((mf[3]-of[3])^2 + (mr[3]-orr[3])^2 + (mu[3]-ou[3])^2)
    fd = v[3, c("depth")] / v[2, c("depth")]; fw = v[3, c("width")] / v[2, c("width")]; fh = v[3, c("height")] / v[2, c("height")]
    printf "resize: spread now %.2f; depth %s -> %s, width %s -> %s, height %s -> %s (x%.2f %.2f %.2f); offsets %s %s %s\n", s0, v[2, c("depth")], v[3, c("depth")], v[2, c("width")], v[3, c("width")], v[2, c("height")], v[3, c("height")], fd, fw, fh, v[3, c("x")], v[3, c("y")], v[3, c("z")]
    ok = (ab(fd - 2) < 0.12 && ab(fw - 2) < 0.06 && ab(fh - 2) < 0.06) ? 1 : 0
    print "RESULT " ok " resize"
    lx = v[5, c("x")] - v[4, c("x")]; ly = v[5, c("y")] - v[4, c("y")]; lf = of[5] - of[4]; lr = orr[5] - orr[4]
    printf "left: x y moved %.2f %.2f, the off hand forward %.2f right %.2f (outwards there: %.2f)\n", lx, ly, lf, lr, -lr
    ok = (ab(lx - lf) < 0.15 && ab(ly + lr) < 0.15 && ab(lr) + ab(lf) > 1) ? 1 : 0
    print "RESULT " ok " left"
    printf "off: %s %s %s -> %s %s %s\n", v[6, c("x")], v[6, c("y")], v[6, c("z")], v[7, c("x")], v[7, c("y")], v[7, c("z")]
    ok = (v[6, c("x")] == v[7, c("x")] && v[6, c("y")] == v[7, c("y")] && v[6, c("z")] == v[7, c("z")] && v[7, c("x")] != "") ? 1 : 0
    print "RESULT " ok " off"
}' > $RES
grep -v "^RESULT" $RES
while read -r tag ok name; do check "$ok" "$name"; done < <(grep "^RESULT" $RES)
echo "$log" | grep -i "rror" | head -3
exit $fail
