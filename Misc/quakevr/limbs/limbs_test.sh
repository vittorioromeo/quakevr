#!/bin/bash
# limbs_test.sh <agent> [cases] -- limb gore (ROUND21.md, "Limb gore"; QC vr_limbs.qc, vr_limbs_test.qc): each case in
# the firing range, `limbtest:` lines (vr_limb_test) and the engine's:
#   models   vr_limb_models for every rigged monster's model: each limb's triangles, cap and size
#   corpse   a dead grunt (MON=n: that Thing) cut apart, the ends first (vr_limb_test 2), and another whole limbs first
#            (3): the parts left after each cut, down to the torso (2: pelvis and chest), its head last
#   live     for each monster in KINDS (the firing range's Thing numbers; default every rigged one), a fresh one per
#            blow, the view turned between them: a sword's killing slash at a limb (4), a fist's (5), a shotgun blast's
#            (6), a bolt's (7), an explosion beside it (9), gibbed (10); vr_decap_pop_roll 0 (every chance > 0 wins)
#   chance   2000 rolls at chance 0.5, head and limb, at Limb Chance / Head Chance 1, 0.5 and 0 (vr_limb_test 8)
#   cap      twice Most Limbs (vr_limbs_max 6) thrown: 6 stay
#   grab     a forearm cut off, left to land; the mock main hand put on it and gripping: held
#   save     a corpse missing an arm saved and loaded: its ragdoll made again without it (cut bones the same)
#   zombie   a zombie slashed at a limb at full health (12): dies for good, a ragdoll; vr_limbs_zombies 0: it doesn't
#   blast    vr_limbs_blast 0 and 1: an explosion's kill gibs, or pops the limbs near it (the body a ragdoll)
#   perf     64 limbs lying about and 8 dismembered ragdolls: the physics step's time (vr_physics_steptime)
#   eyes     eyeshots (vr_eyeshot: both eyes) into the kit's scratch (limbs_eyes.png): the grunt's limbs hung before you, a corpse missing
#            limbs (its stumps), a forearm cut off lying on the floor
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-models corpse live chance cap grab save zombie blast perf}
KINDS=${KINDS:-0 1 2 3 4 5 6 7 8 9 10 12 13 14}
PRE="map vrfiringrange;wait20;god;notarget;developer 1;vr_test_spawn ${MON:-0}"
FILTER="^limbtest|^vr_limb_models|^  *[0-9]+ [a-z_0-9]+ +bones|^vr_physics_steptime|rror|CRASH"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "${3:-$FILTER}" -Timeout 300 ${2:+-Out $2} 2>&1 | grep -v "^$" | grep -v "^exit=0"; }
for c in $CASES; do
    echo "== $c"
    case $c in
    models)
        S="map vrfiringrange;wait10"
        for m in soldier knight ogre enforcer hknight dog wizard zombie demon shambler grem mummy shalrath scor; do S="$S;vr_limb_models progs/$m.mdl"; done
        run "$S;toggleconsole;quit" ;;
    corpse)
        run "$PRE;vr_test_spawn_dist 90;vr_test_spawn_dead 1;impulse 241;wait60;vr_limb_test 2;wait20;vr_mock_look 0 60;impulse 241;wait60;vr_limb_test 3;wait20;toggleconsole;quit" ;;
    live)
        for k in $KINDS; do
            S="map vrfiringrange;wait20;god;notarget;developer 1;vr_decap_pop_roll 0;vr_test_spawn $k;vr_test_spawn_dist 90"
            yaw=0
            for t in 4 5 6 7 9 10; do
                S="$S;vr_mock_look 0 $yaw;wait3;impulse 241;wait30;vr_limb_test $t;wait40"
                yaw=$((yaw + 50))
            done
            echo "-- Thing $k"
            run "$S;toggleconsole;quit" "" "^limbtest|rror|CRASH" | grep -E "on monster|no monster|rror|CRASH" | sed -E 's/limbs [0-9]+ -> [0-9]+, //'
        done ;;
    chance)
        run "$PRE;wait5;vr_limb_test 8;wait5;vr_limbs_chance_scale 0.5;vr_decap_chance_scale 0.5;wait2;vr_limb_test 8;wait5;vr_limbs_chance_scale 0;vr_decap_chance_scale 2;wait2;vr_limb_test 8;wait5;vr_limbs_chance_scale 1;vr_decap_chance_scale 1;toggleconsole;quit" ;;
    cap)
        run "$PRE;vr_limbs_max 6;vr_test_spawn_dist 90;impulse 241;wait30;vr_limb_test 11;wait5;vr_limbs_max 24;toggleconsole;quit" ;;
    grab)
        # (The off hand: the main one holds a gun. The hand jumps there in a frame: Gibs Can Be Destroyed off, or its
        # leap would strike the limb as a blow.)
        run "$PRE;vr_gib_destroy 0;vr_test_spawn_dist 60;impulse 241;wait30;vr_limb_test 4;wait300;vr_limb_test 14;wait30;vr_mock_hand_to off nearest vr_limb 12;wait2;vr_mock_hand_to off nearest vr_limb 12;wait5;vr_limb_test 15;+graboff;vr_mock_button off grip 1;wait1;vr_mock_hand_to off nearest vr_limb 1;wait2;vr_mock_hand_to off nearest vr_limb 1;wait30;vr_limb_test 15;wait5;vr_mock_hand_to off by 0 0 20;wait30;vr_limb_test 15;wait5;-graboff;vr_mock_button off grip 0;wait60;vr_limb_test 15;vr_gib_destroy 1;toggleconsole;quit" ;;
    save)
        run "$PRE;vr_test_spawn_dist 90;vr_test_spawn_dead 1;impulse 241;wait60;vr_debug_ragdoll 1;vr_limb_test 13;vr_limb_test 3;wait5;vr_ragdoll_list;save limbtest;wait10;load limbtest;wait60;vr_ragdoll_list 1;wait5;vr_limb_test 15;wait5;vr_limb_test 13;toggleconsole;quit" "" "^limbtest|^vr_ragdoll_list|cut off|rror|CRASH" ;;
    zombie)
        run "$PRE;vr_test_spawn 2;vr_test_spawn_dist 90;impulse 241;wait30;vr_limb_test 12;wait60;vr_mock_look 0 60;vr_limbs_zombies 0;impulse 241;wait30;vr_limb_test 12;wait30;vr_limbs_zombies 1;toggleconsole;quit" ;;
    blast)
        run "$PRE;vr_decap_pop_roll 0;vr_limbs_blast 0;vr_test_spawn_dist 90;impulse 241;wait30;vr_limb_test 9;wait30;vr_mock_look 0 60;vr_limbs_blast 1;impulse 241;wait30;vr_limb_test 9;wait30;toggleconsole;quit" ;;
    perf)
        S="$PRE;vr_limbs_max 64;vr_test_spawn_dead 1;vr_test_spawn_dist 90"
        for y in 0 45 90 135 180 225 270 315; do S="$S;vr_mock_look 0 $y;impulse 241;wait30;vr_limb_test 3;wait5"; done
        run "$S;vr_limb_test 11;wait300;vr_physics_steptime;wait90;vr_physics_steptime;toggleconsole;quit" "" "^limbtest: 11|^vr_physics_steptime|steptime|rror|CRASH" ;;
    eyes)
        run "map vrfiringrange;wait20;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 250;impulse 241;wait30;vr_limb_test 1;wait200;vr_eyeshot 1;vr_test_spawn_dist 70;vr_mock_look 0 90;vr_test_spawn_dead 1;impulse 241;wait60;vr_limb_test 2;wait10;vr_limb_test 2;wait300;vr_mock_look 45 90;wait5;vr_eyeshot 1;toggleconsole;quit" limbs_eyes.png "composed|rror" ;;
    *) echo "unknown case $c" ;;
    esac
done
