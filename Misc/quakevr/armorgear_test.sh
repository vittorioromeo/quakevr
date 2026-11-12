#!/bin/bash
# armorgear_test.sh <agent> -- the ammo pouch and the belt torch move forward by vr_ammo_pouch_armor_x and
# vr_flashlight_armor_forward while the armour's plates show on the body, and not otherwise (vr_armorgear_check).
# Ends `armorgear: N passed, M failed`.
NAME=${1:?agent name}
S="vr_player_plates 1;vr_player_plates_morph_time 0;vr_ammo_pouch_armor_x 5;vr_flashlight_armor_forward 0.04;map e1m1;wait30;give a 100;wait10"
S="$S;vr_armorgear_check;vr_player_plates 0;wait5;vr_armorgear_check;vr_armorgear_check end;resetcvar vr_ammo_pouch_armor_x;resetcvar vr_flashlight_armor_forward;resetcvar vr_player_plates_morph_time;toggleconsole;quit"
bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "$S" -Filter "armorgear|rror" -QuitOn "armorgear: [0-9]+ passed" -Timeout 300
