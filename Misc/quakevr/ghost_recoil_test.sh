#!/bin/bash
# ghost_recoil_test.sh <agent>: a shot's programmatic recoil brings no ghost hand out (vr_ghost_hands_ignore_recoil 1),
# and the same shot does with it 0 (ROUND23.md, "Kick cancel in the poses; two-handed recoil; no ghost hands from recoil"). The shotgun at Recoil Amount 3 (about
# 17 cm, 17 degrees: past Ghost Hidden Within's 6 cm, pinned), fired once in each phase; each phase waits out its kick (0.35 s:
# 26 frames, 40 waited) and the shotgun's refire. Ends on "vr_ghost_recoil_test: N passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-arenamap}
S="map e1m1;wait60;god;notarget;vr_weapon_grip_mode 1;impulse 9;wait2;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait5"
S="$S;vr_test_give_wid 4;impulse 149;wait5;vr_ghost_hands 1;vr_ghost_hands_fade_from 6;vr_recoil_amount 3"
S="$S;vr_ghost_hands_ignore_recoil 1;vr_ghost_recoil_test start;+attack;wait2;-attack;wait40;vr_ghost_recoil_test ignored"
S="$S;vr_ghost_hands_ignore_recoil 0;wait10;+attack;wait2;-attack;wait40;vr_ghost_recoil_test end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_ghost_recoil_test" -QuitOn "vr_ghost_recoil_test: \d+ passed"
