#!/bin/bash
# stats_levelchange_test.sh <agent> -- a stat whose value carries across a map change reaches the client on the new map
# (the author's note e1m2_2026-10-07_22-43-20: after a slipgate, Immersive reloading drew no ammo pouch nor magazines;
# the server never resent STAT_QVR_RELOADMODE, unchanged at 3, after the client had cleared its stats; SV_SendServerinfo
# now forgets what it sent). Headless, by vr_mock_hand_to main ammopouch (which needs the client's reloading mode 3):
#   1. e1m1: the pouch is there. 2. changelevel e1m2: still there. 3. a save and a load: still there.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
POUCH="vr_mock_hand_to main ammopouch;wait3"
log=$(bash $KIT/run.sh $AGENT -Script "vr_holster_mode 0;vr_reload_mode 3;map e1m1;wait60;$POUCH;changelevel e1m2;wait90;$POUCH;save qvr_stats_test;wait5;load qvr_stats_test;wait90;$POUCH;toggleconsole;quit" -Filter "vr_mock_hand_to: (the|no) ammo|rror" 2>&1)
lines=$(echo "$log" | grep -E "vr_mock_hand_to: (the|no) ammo")
check $(echo "$lines" | sed -n 1p | grep -q "the ammo pouch" && echo 1 || echo 0) "e1m1: the ammo pouch"
check $(echo "$lines" | sed -n 2p | grep -q "the ammo pouch" && echo 1 || echo 0) "changelevel e1m2: the ammo pouch still there"
check $(echo "$lines" | sed -n 3p | grep -q "the ammo pouch" && echo 1 || echo 0) "save and load: the ammo pouch still there"
check $(echo "$log" | grep -q "rror" && echo 0 || echo 1) "no error"
exit $fail
