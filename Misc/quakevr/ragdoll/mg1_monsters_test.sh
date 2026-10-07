#!/bin/bash
# MG1 monsters in the native MG1 context (mge1m1's start): each attacks (health from 999), is beheaded by a sword's
# slash (ragdoll where rigged), loses a limb to a killing slash, and has its head popped by a shot.
# N=<agent> bash mg1_monsters_test.sh [indices]   (vr_test_spawn: 0 grunt 1 ogre 2 zombie 3 shambler 4 scrag 5 knight 6 hknight
#                                   7 dog 8 enforcer 9 fiend 10 vore 11 spawn; the marksman ogre: vr_mg_hub_test 38)
KIT=C:/OHWorkspace/qvr-kit
N=${N:?N=<agent>}; W=C:/OHWorkspace/qvr-agents/$N/scratch; mkdir -p $W
MONS=${*:-0 1 2 3 4 5 6 7 8 9 10 11}
PRE="developer 1;${STOCKPRE:-vr_campaign_native mg1;skill 1;map mge1m1};wait60;god 1;notarget 1;vr_debug_ragdoll 1;vr_ragdoll 1;vr_test_spawn_dist ${DIST:-120}"
for m in $MONS; do
    S="$PRE;vr_test_spawn $m;impulse 241;wait10"
    # attack: notarget off, god off, health 999; it must hurt us
    S="$S;notarget 0;god 0;give h 999;wait${ATTACKWAIT:-600};vr_mg_hub_test 3;wait3;god 1;notarget 1"
    S="$S;vr_decap_test 1;wait90;vr_ragdoll_list 1;wait3"
    S="$S;vr_test_spawn $m;impulse 241;wait20;vr_limb_test 4;wait60;vr_ragdoll_list 1;wait3"
    S="$S;vr_test_spawn $m;impulse 241;wait20;vr_decap_test 12;wait60;vr_ragdoll_list 1"
    echo "== monster $m"
    bash $KIT/run.sh $N -ExtraArgs "-nomapindex" -Timeout 300 -Script "$S;wait5;toggleconsole;quit" \
        -Filter "test spawn|mghubtest: player|^decap|beheaded|^ragdoll [0-9]|cut off|gibbed|^limbtest|popped|Host_Error|ENGINE|TIMEOUT|exit=[^0]" \
        > $W/mon${TAG}_$m.log 2>&1
    grep -a -v "^$" $W/mon${TAG}_$m.log | cut -c1-170 | head -${LINES_PER:-14}
done
