#!/bin/bash
# fist_alert_test.sh <agent> -- a hand merely closing doesn't wake monsters up (the author's note e1m2_2026-10-07_22-44-33:
# the trigger on a fist ran W_AttackImpl's "wake monsters up", show_hostile). Read from the player's show_hostile in
# saves (scratch saves in quakevr/: qvr_alert_*.sav), on e1m1, headless:
#   1. the fist closed by the grip and by the trigger, held: show_hostile unset
#   2. a punch in the air with the closed fist (vr_mock_play, 5 m/s; it whooshes): set
#   3. the shotgun fired: set
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
W=C:/OHWorkspace/qvr-agents/$AGENT
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
mkdir -p "$W/scratch"
printf '0.000 main 0.00 1.15 -0.20 0 0 0\n0.300 main 0.00 1.15 -0.20 0 0 0\n0.450 main 0.00 1.15 -1.00 0 0 0\n1.200 main 0.00 1.15 -1.00 0 0 0\n' > "$W/scratch/qvr_alert_punch.txt"
P=$(cd "$W/scratch" && pwd -W 2>/dev/null || pwd)/qvr_alert_punch.txt
hostile() { awk 'BEGIN{RS="}"} /"classname" "player"/' "$W/quakevr/$1.sav" | grep -E '"show_hostile"' | sed -E 's/.*"([0-9.]+)"/\1/'; }
S="vr_fixed_frames 1;vr_fixed_frames_rate 90;map e1m1;wait60;notarget;vr_mock_hand main 0.00 1.15 -0.20 0 0 0;wait5"
S="$S;+grabmain;vr_mock_button main grip 1;vr_mock_button main trigger 1;+attack;wait30;save qvr_alert_close"
S="$S;vr_mock_play $P;wait150;save qvr_alert_punch;-attack;vr_mock_button main trigger 0;vr_mock_button main grip 0;-grabmain;wait10"
S="$S;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait10;+attack;vr_mock_button main trigger 1;wait3;save qvr_alert_shot;-attack;vr_mock_button main trigger 0;wait5;toggleconsole;quit"
log=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "rror" 2>&1)
check $([ -z "$(hostile qvr_alert_close)" ] && echo 1 || echo 0) "the fist closed, held: no wake ($(hostile qvr_alert_close))"
check $([ -n "$(hostile qvr_alert_punch)" ] && echo 1 || echo 0) "a punch: wakes ($(hostile qvr_alert_punch))"
check $([ -n "$(hostile qvr_alert_shot)" ] && echo 1 || echo 0) "the shotgun fired: wakes ($(hostile qvr_alert_shot))"
check $(echo "$log" | grep -q "rror" && echo 0 || echo 1) "no error"
exit $fail
