#!/bin/bash
# pouchcatch_test.sh <agent>: a magazine out of a gun falling into the ammo pouch is collected, drawn going in
# (vr_reload_pouch_catch; ROUND23.md, "Falling magazines into the pouch"; vr_reload_test 44): with rounds it goes in; empty,
# with the option off, with the reserve full, to one side or taken by a hand it doesn't. Ends on "pouchcatch: N passed, M
# failed". The put-away transition's line: "collect fx: progs/vr_mag_nail.mdl ... into hotspot 12".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
S="sv_random_seed 7;map e1m1;wait30;god;notarget;developer 1;vr_weapon_grip_mode 1;vr_debug_collect_fx 1;wait20;vr_reload_test 44;impulse 125"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "pouchcatch|reload: (PASS|FAIL)|collect fx: progs|host_error|PROG" -QuitOn "pouchcatch: \d+ passed" -Timeout 60
