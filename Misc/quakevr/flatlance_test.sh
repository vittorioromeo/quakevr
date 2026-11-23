#!/bin/bash
# flatlance_test.sh <agent> -- flat-screen play with Dwell's Crystal Lance (flatmelee2; his notes 2026-11-23; ROUND25.md,
# "flatmelee2"). Needs Dwell v2.2 in the base dir. One headless run in fast mode on e1m1, the Lance in the main
# hand (vr_ad_test 207):
#   1. B (Next Ammo Type, impulse 43): refused ("ammo switch: refused, the Crystal Lance"), no ammo change, still the Lance
#      (vr_flatplay_status's "spear:" line: a pole weapon; the lightning gun is none);
#   2. the spear pose's tunables, as drawn (vr_flatplay_status "spear:"): vr_flat_pole_along 6 brings the tip 6 nearer the
#      fist; vr_flat_pole_pitch 20 lifts the pole 20 degrees and _yaw 15 turns it 15 left; vr_flat_pole_off_up 3 moves the
#      other hand 3 off the pole; the other hand stays on the pole at vr_flat_pole_off_along from the fist;
#      vr_flat_pole_off_pitch 30 turns the drawn other hand 30 degrees and vr_flat_pole_hand_roll 30 rolls the fist 30 about
#      its forward (flatmelee3: "spear hands:", each hand's drawn forward and up);
#   3. V: the spear thrust (vr_flat_thrust: "spear thrust"), drawn back, thrust out along the pole and back (the probe's
#      "flatmelee: main thrust" line), a stab by the head (QC "dwlance: a thrust, the head") on an ogre 52 units ahead,
#      which is hurt.
# Every wait is the frames a step needs at 72 a second (the thrust 0.64 s: 46); ends on "flatlance: done" (-QuitOn).
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch
mkdir -p "$S"
L="$S/flatlance.log"
ST="vr_flatplay_status"
KEY() { echo "vr_mock_key $1 down; wait3; vr_mock_key $1 up; wait10"; }
SC="map e1m1; wait60; god; notarget; developer 1; vr_flatplay_debug 1; vr_flat_reload_debug 1; vr_flat_melee_probe 1; vr_debug_2h_grip 1${FLT_EXTRA:+; $FLT_EXTRA}"
SC="$SC; vr_ad_test 207; wait60; echo flt: base; $ST"
SC="$SC; echo flt: b; $(KEY b); $ST"
SC="$SC; tune vr_flat_pole_along 6; wait10; echo flt: along; $ST; tune_reset vr_flat_pole_along"
SC="$SC; tune vr_flat_pole_pitch 20; tune vr_flat_pole_yaw 15; wait10; echo flt: turn; $ST; tune_reset vr_flat_pole_pitch; tune_reset vr_flat_pole_yaw"
SC="$SC; tune vr_flat_pole_off_up 3; wait10; echo flt: offup; $ST; tune_reset vr_flat_pole_off_up; wait10"
SC="$SC; tune vr_flat_pole_off_pitch 30; tune vr_flat_pole_hand_roll 30; wait10; echo flt: rot; $ST; tune_reset vr_flat_pole_off_pitch; tune_reset vr_flat_pole_hand_roll; wait10"
SC="$SC; vr_test_spawn 1; vr_test_spawn_dist 52; impulse 241; wait20; echo flt: thrust; vr_flat_punch main v; wait50; vr_test_hurt -1; wait5"
SC="$SC; vr_flat_melee_probe done; wait2; echo flatlance: done; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SC" -QuitOn "^flatlance: done" -Timeout 240 \
  -Filter '^flt:|^flatlance|^ammo|flatreload: b|spear:|spear hands:|spear thrust|^flatmelee: main|dwlance: a thrust|2h grip|test hurt:|ERROR|CRASH' > "$L" 2>&1
block() { awk -v m="flt: $1" '$0==m || $0==m" "{on=1;next} /^flt:|^flatlance: done/{on=0} on' "$L"; }
# A number after a word on the block's "spear: main" line.
num() { block "$1" | grep -a "spear: main" | head -1 | grep -ao " $2 -\?[0-9.]*" | head -1 | grep -ao -- '-\?[0-9.]*$'; }
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
check $(grep -aq "^flatlance: done" "$L" && echo 1 || echo 0) "the run reached its end"

# 1. B with the Lance.
b=$(block b)
check $(grep -aq "flatreload: b: next ammo type" <<<"$b" && grep -aq "ammo switch: refused, the Crystal Lance" <<<"$b" && ! grep -aq "^ammo: hand" <<<"$b" && echo 1 || echo 0) "B with the Lance: sent, refused, no ammo change"
check $(grep -aq "spear: main" <<<"$b" && echo 1 || echo 0) "B with the Lance: still the Lance (a pole weapon), not the lightning gun"

# 2. The spear pose's tunables.
t0=$(num base tip); t1=$(num along tip); u0=$(num base up); u1=$(num turn up); l0=$(num base left); l1=$(num turn left)
a0=$(num base along); o0=$(num base off); o1=$(num offup off)
echo "  base: tip $t0, up $u0, left $l0, other hand along $a0 off $o0; along 6: tip $t1; pitch 20 yaw 15: up $u1 left $l1; off_up 3: off $o1"
check $(ok "\"$t0\" != \"\" && \"$t1\" != \"\" && ($t0 - $t1) > 5 && ($t0 - $t1) < 7") "vr_flat_pole_along 6: the tip 6 nearer the fist (${t0:-?} -> ${t1:-?})"
check $(ok "\"$u0\" != \"\" && \"$u1\" != \"\" && ($u1 - $u0) > 17 && ($u1 - $u0) < 23") "vr_flat_pole_pitch 20: the pole 20 degrees up (${u0:-?} -> ${u1:-?})"
check $(ok "\"$l0\" != \"\" && \"$l1\" != \"\" && ($l1 - $l0) > 12 && ($l1 - $l0) < 18") "vr_flat_pole_yaw 15: the pole 15 degrees left (${l0:-?} -> ${l1:-?})"
check $(ok "\"$a0\" != \"\" && \"$o0\" != \"\" && $a0 > 12 && $a0 < 20 && $o0 < 4") "the other hand on the pole about vr_flat_pole_off_along 16 from the fist (along ${a0:-?}, off ${o0:-?})"
check $(ok "\"$o1\" != \"\" && ($o1 - $o0) > 2 && ($o1 - $o0) < 4") "vr_flat_pole_off_up 3: the other hand 3 off the pole (${o0:-?} -> ${o1:-?})"
# The hands' own turns (flatmelee3): a block's "spear hands:" vector N (1 the fist's forward, 2 its up, 3 the other hand's
# forward, 4 its up), and the degrees between two vectors.
vec() { block "$1" | grep -a "spear hands:" | head -1 | tr -c '0-9.\n-' ' ' | awk -v i="$2" '{ print $(3*i-2), $(3*i-1), $(3*i) }'; }
deg() { awk -v a="$1" -v b="$2" 'BEGIN { if (a == "" || b == "") { print -1; exit } split(a, x, " "); split(b, y, " "); d = x[1]*y[1] + x[2]*y[2] + x[3]*y[3]; d = d > 1 ? 1 : (d < -1 ? -1 : d); printf "%.1f", atan2(sqrt(1 - d*d), d) * 57.29578 }'; }
of=$(deg "$(vec base 3)" "$(vec rot 3)"); fu=$(deg "$(vec base 2)" "$(vec rot 2)"); ff=$(deg "$(vec base 1)" "$(vec rot 1)")
check $(ok "$of > 27 && $of < 33") "vr_flat_pole_off_pitch 30: the drawn other hand turned 30 degrees ($of)"
check $(ok "$fu > 27 && $fu < 33 && $ff >= 0 && $ff < 2") "vr_flat_pole_hand_roll 30: the drawn fist rolled 30 about its forward (up $fu, forward $ff)"

# 3. The thrust.
th=$(block thrust)
check $(grep -aq "main punch (.*spear thrust" <<<"$th" && echo 1 || echo 0) "V with the Lance: the spear thrust, not the overhead swing"
check $(grep -aq "^flatmelee: main thrust" <<<"$th" && echo 1 || echo 0) "the probe saw the thrust ($(grep -a '^flatmelee: main thrust' <<<"$th" | head -1 | cut -c1-90))"
d=$(grep -a "dwlance: a thrust" <<<"$th" | head -1)
check $(grep -aq "the head" <<<"$d" && echo 1 || echo 0) "the thrust is the head's stab (${d:-no thrust})"
h=$(grep -a "test hurt:" <<<"$th" | head -1)
hp=$(grep -ao "health [0-9.]*" <<<"$h" | head -1 | awk '{ print $2 }')
check $(grep -aq "monster_ogre" <<<"$h" && ok "\"$hp\" != \"\" && $hp < 200") "the ogre ahead hurt by it (${h:-none})"
verdict
exit $fail
