#!/bin/bash
# deathdoll_flat_test.sh <agent> -- a flat-screen player's corpse (his avatar doll, QC vr_deathdoll.qc) is his flat body
# height's size, as his living avatar is drawn (mpavatar::serverBody; ROUND25 "backlog1122b"), headless, single player
# e1m1, killed by vr_hurt: in flat play with vr_flat_body_height 1.4 and 1.8 the doll's scale goes as the height (1.8 /
# 1.4), and in VR it is the headset's height (unchanged by vr_flat_body_height). Prints PASS/FAIL lines and a verdict.
AGENT=${1:-dagger}
source "$(dirname "$0")/qvrtest.sh"
run() { # $1 extra args, $2 flat height: prints the doll's scale
    bash $KIT/run.sh $AGENT $1 -Timeout 120 -QuitOn "^deathdoll_flat: dead" -Filter "mp_doll_rig|^deathdoll_flat" \
        -Script "map e1m1;wait60;vr_flat_body_height $2;vr_hurt 110 0;wait120;vr_mp_avatars;echo deathdoll_flat: dead;toggleconsole;quit" \
        2>&1 | grep -a -o "dollscale=[0-9.]*" | head -1 | cut -d= -f2
}
a=$(run "-ExtraArgs -flatscreenmode" 1.4)
b=$(run "-ExtraArgs -flatscreenmode" 1.8)
v1=$(run "" 1.4)
v2=$(run "" 1.8)
check "$(awk -v a="$a" -v b="$b" 'BEGIN { print (a > 0 && b / a > 1.27 && b / a < 1.30) ? 1 : 0 }')" "flat: the corpse goes as his flat height ($a at 1.4 m, $b at 1.8 m)"
check "$(awk -v a="$v1" -v b="$v2" 'BEGIN { print (a > 0 && a == b) ? 1 : 0 }')" "VR: the headset's height, not the flat one ($v1, $v2)"
verdict; exit $fail
