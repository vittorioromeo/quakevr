#!/bin/bash
# ragdoll_test.sh <agent> [cases] -- ragdolls (ROUND21.md, "Ragdolls" and "Ragdolls 2"; vr_ragdoll 1): a grunt killed
# ahead goes limp and is followed to rest; each scenario prints its ragdolls (vr_ragdoll_list: parts awake, where the
# pelvis lies, how high the parts' lowest and highest points are) and what happened to them:
#   flat     e1m1's start: killed 90 units ahead (a side eyeshot from the firing range: EYES=1)
#   stairs   vrclimb's stairs (12 steps of 16): killed at their top, he falls down them and rests across steps
#   blast    e1m1: a live grunt killed by a 60-point blast beside him: the blast throws his ragdoll as it is made
#   shot     e1m1: his ragdoll shot with the shotgun (each pellet's push, his precise hit's corpse damage)
#   gib      e1m1: his ragdoll blown up (200): gibbed, the ragdoll gone
#   cap      e1m1: 10 grunts killed, at most 8 ragdolls (vr_ragdoll_max): 8 ragdolls and 2 corpses; the step's cost
#   save     e1m1: saved and loaded with a ragdoll lying there: made again from his frame where he lay
#   slowmo   e1m1 in slow motion (vr_timescale 0.1): six grunts killed (both death animations); each one's first frame
#            drawn against the animated mesh's ("first drawn": units rms and at most). EYES=1: vrfiringrange, a frame
#            every 2 around the switch (ragdoll_slowmo.png: 8 crops round the largest change; MON=5: the knight)
#   grab     e1m1: the off hand grips the limb nearest it (vr_mock_hand_to ... ragdoll near), lifts it 30 units, flings
#            it and lets go: taken, held (how far from the hand), let go of at the throw's speed, where it came to rest
#   twohand  both hands grip, lift 25 units together and let go
#   pull     a force grab from 140 units (a vr_mock_play take: the main hand points, the trigger locks, a flick up would
#            pull, the grip would catch): never pulled (force grab leaves ragdolls out; its ragdoll pull was removed)
#   pile     four grunts killed on one spot, ragdolls meeting each other (vr_ragdoll_collide_each 1) and not (0)
#   burn     a ragdoll set on fire (vr_burn_test 1), then lifted and flung by hand: its flames' distance to its limbs
#   wounds   a ragdoll shot twice: the wound masks (vr_wounds_info: painted on progs/soldier.mdl#rag)
#   walk     you walking at a ragdoll (and a corpse), You and Corpses 0 and 2: your height as you walk (on it: higher)
#   blows    the small gibs tests' chainsaw second and blows (vr_smallgibs_test 6 and 4) on a ragdoll
#   retro    retro textures on (vr_retro 1): the living grunt's and his ragdoll's skin size in Quake texels (vr_retro_list:
#            the ragdoll's skinned copy keeps the .mdl's, not a quarter of its texture's: the same blocks as alive)
# EYES=1 also takes eyeshots (flat, stairs, blast, slowmo) into the kit's scratch.
# MON=5: the knight instead of the grunt (vr_test_spawn: the Thing ahead).
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-flat stairs blast shot gib cap save slowmo grab twohand pull pile burn wounds walk blows retro}
PRE="wait30;god;notarget;vr_ragdoll 1;$XPRE;vr_debug_ragdoll 1;vr_test_spawn ${MON:-0}"
DEAD="vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0"
FILTER="^ragdoll|^vr_ragdoll_list|^vr_physics_steptime|corpses in the physics|gibbed|corpse: .* hit by|rror|CRASH|pushed at|  (held|pulled) by|flames on it|^wounds|soldier.mdl#rag|force grab: (a rag|caught a)"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "${3:-$FILTER}" -Timeout 300 ${2:+-Out $2} 2>&1 | grep -v "^$" | grep -v "part [0-9]* blasted"; }
# The hand on the limb nearest it, 3 units over its middle (twice: the mock hand reaches from where it was).
TAKE() { echo "vr_mock_hand_to $1 ragdoll near 3;wait2;vr_mock_hand_to $1 ragdoll near 3;+grab$2;vr_mock_button $1 grip 1"; }
for c in $CASES; do
    echo "== $c"
    case $c in
    flat)
        run "map e1m1;$PRE;vr_test_spawn_dist 90;$DEAD;wait20;vr_ragdoll_list;wait300;vr_ragdoll_list;toggleconsole;quit"
        if [ -n "$EYES" ]; then
            run "map vrfiringrange;$PRE;vr_test_spawn_dist 100;$DEAD;vr_mock_camera 1.6 0.4 -3.2 15 80;wait6;screenshot;wait12;screenshot;wait12;screenshot;wait24;screenshot;wait300;screenshot;toggleconsole;quit" ragdoll_flat.png
        fi ;;
    stairs)
        S="map vrclimb;$PRE;setpos -430 704 24 0 0 0;wait5;vr_test_spawn_dist 90;$DEAD;wait20;vr_ragdoll_list;wait400;vr_ragdoll_list 1"
        [ -n "$EYES" ] && S="$S;r_fullbright 1;vr_mock_camera 0.3 -1.8 -5.6 -17 180;wait3;screenshot;vr_debug_physics_shapes 1;wait2;screenshot"
        run "$S;toggleconsole;quit" ${EYES:+ragdoll_stairs.png} ;;
    blast)
        run "map e1m1;$PRE;vr_test_spawn_dist 90;impulse 241;wait30;vr_physics_blast 500 -255 50 ${BLAST:-60};wait20;vr_ragdoll_list 1;wait300;vr_ragdoll_list;toggleconsole;quit"
        if [ -n "$EYES" ]; then
            run "map vrfiringrange;$PRE;vr_test_spawn_dist 100;impulse 241;wait30;vr_mock_camera 1.6 0.4 -3.2 15 80;wait3;screenshot;vr_physics_blast 222 -585 30 65;wait12;screenshot;wait12;screenshot;wait24;screenshot;wait300;vr_mock_camera 0.3 1.7 0 18 12;wait2;screenshot;vr_ragdoll_list;toggleconsole;quit" ragdoll_blast.png
        fi ;;
    shot)
        run "map e1m1;$PRE;vr_weapon_grip_mode 1;impulse 9;wait5;impulse 154;wait5;vr_mock_hand main -0.05 1.3 -0.3 40 0 0;vr_test_spawn_dist 70;$DEAD;wait300;vr_ragdoll_list;+attack;wait3;-attack;wait100;vr_ragdoll_list;toggleconsole;quit" ;;
    gib)
        run "map e1m1;$PRE;vr_test_spawn_dist 70;$DEAD;wait300;vr_ragdoll_list;vr_physics_blast 475 -281 50 200;wait10;vr_ragdoll_list;toggleconsole;quit" ;;
    cap)
        SP="vr_test_spawn_dead 1;impulse 241;wait3"
        run "map e1m1;$PRE;vr_debug_ragdoll 0;vr_ragdoll_max 8;vr_test_spawn_dist 90;wait60;vr_physics_steptime;wait120;vr_physics_steptime;$SP;$SP;$SP;$SP;$SP;$SP;$SP;$SP;$SP;$SP;vr_test_spawn_dead 0;wait20;vr_physics_steptime;wait120;vr_physics_steptime;vr_ragdoll_list;wait400;vr_physics_steptime;wait120;vr_physics_steptime;vr_ragdoll_list;vr_corpse_list;toggleconsole;quit" | sed 's/^vr_physics_steptime/  steptime/' ;;
    save)
        run "map e1m1;$PRE;vr_test_spawn_dist 90;$DEAD;wait300;vr_ragdoll_list;save ragdolltest;wait10;load ragdolltest;wait60;vr_ragdoll_list;wait300;vr_ragdoll_list;toggleconsole;quit" ;;
    slowmo)
        S="map e1m1;$PRE;vr_timescale 0.1;wait5"
        for d in 60 90 120 150 180 210; do S="$S;vr_test_spawn_dist $d;$DEAD;wait7"; done
        run "$S;wait400;toggleconsole;quit" "" "first drawn|limp at" | tr -d '\n' | sed 's/ragdoll: /\nragdoll: /g' | grep -o "limp at frame [0-9]*\|poses.*"; echo
        if [ -n "$EYES" ]; then
            S="map vrfiringrange;$PRE;vr_bullettime_fx 0;vr_test_spawn_dist 100;vr_timescale 0.1;wait5;$DEAD;wait130"
            for i in $(seq 30); do S="$S;wait2;screenshot"; done
            bash $KIT/run.sh $AGENT -Clean -Script "$S;toggleconsole;quit" -Filter "first drawn" -Timeout 300 2>&1 | tr -d '\n' | grep -o "poses.*"; echo
            ${PY:-py -3.13} - "$KIT/bases/$AGENT/qbase/quakevr/screenshots" "$KIT/scratch/ragdoll_slowmo.png" <<'PY'
import glob, os, sys
from PIL import Image, ImageChops, ImageStat
files = sorted(glob.glob(sys.argv[1] + '/*.png'), key=os.path.getmtime)
crop = [Image.open(f).convert('RGB').crop((330, 250, 630, 540)) for f in files]
d = [sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3 for a, b in zip(crop, crop[1:])]
k = max(range(len(d)), key=lambda i: d[i]) # (the switch: the largest change, or none stands out)
print('crop diffs frame to frame (a frame every 2):', ' '.join('%.1f' % x for x in d), '; largest between', k, 'and', k + 1)
crop = crop[max(0, k - 3):k + 5]
strip = Image.new('RGB', (300 * len(crop), 290))
for n, c in enumerate(crop):
    strip.paste(c, (300 * n, 0))
strip.save(sys.argv[2])
PY
        fi ;;
    grab)
        run "map e1m1;$PRE;developer 1;vr_test_spawn_dist 28;$DEAD;wait300;$(TAKE off left);wait10;vr_ragdoll_list 1;vr_mock_hand_to off by 0 0 30;wait40;vr_ragdoll_list 1;vr_mock_hand_to off by 0 0 8;wait1;vr_mock_hand_to off by 0 10 8;wait1;vr_mock_hand_to off by 0 12 6;wait1;vr_mock_hand_to off by 0 12 4;wait1;-grableft;vr_mock_button off grip 0;wait200;vr_ragdoll_list;toggleconsole;quit" | grep -v -E "^  [ 0-9][0-9] [a-z]" ;;
    twohand)
        run "map e1m1;$PRE;developer 1;vr_test_spawn_dist 28;$DEAD;wait300;$(TAKE off left);wait5;$(TAKE main right);wait10;vr_ragdoll_list 1;vr_mock_hand_to off by 0 0 25;vr_mock_hand_to main by 0 0 25;wait30;vr_ragdoll_list 1;-grableft;vr_mock_button off grip 0;-grabright;vr_mock_button main grip 0;wait100;vr_ragdoll_list 1;toggleconsole;quit" | grep -v -E "^  [ 0-9][0-9] [a-z]" ;;
    pull)
        G=$KIT/bases/$AGENT/qbase/id1/ragdoll_pull.txt
        printf "%s\n" "0.000 off -0.350 1.100 -0.200 70 0 0" "0.000 main 0.100 1.300 -0.450 56 0 0" "0.500 cmd +attack" \
            "1.000 main 0.100 1.300 -0.450 56 0 0" "1.100 main 0.100 1.600 -0.450 56 0 0" "1.150 cmd +grabright" \
            "1.150 button main grip 1" "1.200 cmd -attack" "1.300 cmd vr_ragdoll_list 1" "2.000 cmd vr_ragdoll_list 1" \
            "3.000 cmd vr_ragdoll_list 1" "3.200 cmd -grabright" "3.200 button main grip 0" "5.000 cmd vr_ragdoll_list" > $G
        run "map e1m1;$PRE;developer 1;vr_test_spawn_dist 140;$DEAD;wait300;vr_mock_play $G;wait500;toggleconsole;quit" | grep -v -E "^  [ 0-9][0-9] [a-z]" ;;
    pile)
        for e in 1 0; do
            S="map e1m1;$PRE;vr_debug_ragdoll 0;vr_ragdoll_collide_each $e;vr_test_spawn_dist 70"
            for i in 1 2 3 4; do S="$S;$DEAD;wait40"; done
            echo "vr_ragdoll_collide_each $e:"; run "$S;wait400;vr_ragdoll_list;toggleconsole;quit" | grep -o "pelvis.*"
        done ;;
    burn)
        run "map e1m1;$PRE;developer 1;vr_test_spawn_dist 28;$DEAD;wait300;vr_burn_test 1;wait40;vr_ragdoll_list 2;$(TAKE off left);wait10;vr_mock_hand_to off by 0 0 30;wait20;vr_ragdoll_list 2;vr_mock_hand_to off by 0 10 8;wait1;vr_mock_hand_to off by 0 12 6;wait1;vr_mock_hand_to off by 0 12 4;wait1;-grableft;vr_mock_button off grip 0;wait8;vr_ragdoll_list 2;wait30;vr_ragdoll_list 2;toggleconsole;quit" | grep -v -E "^  [ 0-9][0-9] [a-z]" ;;
    wounds)
        run "map e1m1;$PRE;vr_weapon_grip_mode 1;impulse 9;wait5;impulse 154;wait5;vr_mock_hand main -0.05 1.3 -0.3 40 0 0;vr_test_spawn_dist 70;$DEAD;wait300;vr_wounds_info;+attack;wait3;-attack;wait30;vr_wounds_info;wait60;+attack;wait3;-attack;wait30;vr_wounds_info;toggleconsole;quit" ;;
    walk)
        for r in 1 0; do for p in 0 2; do
            S="map e1m1;$PRE;vr_ragdoll $r;vr_corpse_collide_player $p;vr_test_spawn_dist 60;$DEAD;wait300;cl_forwardspeed 100;+forward"
            for i in 1 2 3 4 5 6 7 8; do S="$S;wait8;edict 1"; done
            echo -n "vr_ragdoll $r, You and Corpses $p: your y/z as you walk: "
            run "$S;-forward;toggleconsole;quit" "" "^origin" | grep "^origin" | awk -F"'" '{print $2}' | awk '{printf "%.0f/%.0f  ", $2, $3}'; echo
        done; done ;;
    blows)
        run "map e1m1;$PRE;developer 1;vr_test_spawn_dist 70;$DEAD;wait300;vr_smallgibs_test_n 40;vr_smallgibs_test 6;wait100;vr_ragdoll_list;vr_smallgibs_test 4;wait100;vr_ragdoll_list;toggleconsole;quit" "" "smallgib: monster|^vr_ragdoll_list|rror" | sed 's/: chance.*//' | sort | uniq -c ;;
    retro)
        run "map e1m1;$PRE;vr_retro 1;vr_test_spawn_dist 90;impulse 241;wait20;vr_retro_list;$DEAD;wait100;vr_retro_list;toggleconsole;quit" "" "soldier.mdl" | grep -o "progs/soldier.mdl.*skin.*texture [0-9x]*" | sed 's/  */ /g' ;;
    esac
done
