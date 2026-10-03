#!/bin/bash
# decap_test.sh <agent> [case ...] -- decapitation's mock tests (ROUND21.md, "Decapitation"; QC vr_decap.qc vr_decap_test).
# Cases: live (a sword's slash kills a grunt), axe (an axe's slash), refuse (a stab, a pommel strike, a slow slash: none
# behead), corpse (a slash at a ragdoll's head), saw (the chainsaw's bar), zombie (a slash at a zombie at full health),
# thrown (an axe thrown edge first at a grunt's head), gib (a headless corpse gibbed: no head thrown), save (beheaded,
# saved, loaded: headless again, then gibbed), monsters (a slash on each rigged monster); head pops (shots at the head:
# "Head pops"): pop (vr_decap_test 12-18), popoff (each weapon's option off: 12-15 not popped), popzombie (a zombie's
# head shot at its 60: popped, dead for good; Zombies off: not). MON (0): the monster.
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-live axe refuse corpse saw zombie thrown gib save monsters pop popoff popzombie}
MON=${MON:-0}
PRE="wait30;god;notarget;developer 1;vr_debug_ragdoll 1;vr_test_spawn_dist 80"
SPAWN() { echo "vr_test_spawn ${1:-$MON};impulse 241;wait20"; }
FILTER="${FILTER:-^decap|beheaded|rror|^ragdoll [0-9]|cut off|gibbed|axestick: |^loaded}"
run() { bash $KIT/run.sh $AGENT -Script "$1;wait5;toggleconsole;quit" -Filter "$FILTER" -Timeout 240 2>&1 | grep -v "^$\|^exit=0"; }
for c in $CASES; do
    echo "== $c"
    case $c in
    live) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 1;wait90;vr_ragdoll_list 1" ;;
    axe) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 9" ;;
    refuse) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 2;wait5;$(SPAWN);vr_decap_test 3;wait5;$(SPAWN);vr_decap_test 4" ;;
    corpse) run "map e1m1;$PRE;$(SPAWN);vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0;wait200;vr_decap_test 5;wait30;vr_ragdoll_list 1" ;;
    saw) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 6;wait5;$(SPAWN);vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0;wait200;vr_decap_test 10" ;;
    zombie) run "map vrfiringrange;$PRE;$(SPAWN 2);vr_decap_test 7;wait120;vr_ragdoll_list 1;wait200;vr_decap_test 8" ;;
    thrown) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 11;wait2;vr_debug_axestick 1;vr_test_axe_up 15;vr_test_axe_at 1;vr_test_axe_dist 70;vr_test_axe_speed 9;vr_test_axe 0;impulse 209;wait120;vr_ragdoll_list 1" ;;
    gib) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 1;wait200;vr_decap_test 8;wait5;vr_ragdoll_list" ;;
    save) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 1;wait200;save decaptest;wait5;load decaptest;wait60;echo loaded;vr_ragdoll_list 1;vr_decap_test 8" ;;
    pop) run "map e1m1;$PRE;$(SPAWN);vr_decap_test 12;wait5;$(SPAWN);vr_decap_test 13;wait5;$(SPAWN);vr_decap_test 14;wait5;$(SPAWN);vr_decap_test 15;wait5;$(SPAWN);vr_decap_test 16;wait5;$(SPAWN);vr_decap_test 17;wait5;$(SPAWN);vr_decap_test 18;wait60;vr_ragdoll_list 1" ;;
    popoff) run "map e1m1;$PRE;vr_decap_shotgun 0;vr_decap_super_shotgun 0;vr_decap_lightning 0;$(SPAWN);vr_decap_test 12;wait5;$(SPAWN);vr_decap_test 13;wait5;$(SPAWN);vr_decap_test 14;wait5;$(SPAWN);vr_decap_test 15;wait5;vr_decap_shotgun 1;vr_decap_super_shotgun 1;vr_decap_lightning 1" ;;
    popzombie) run "map vrfiringrange;$PRE;$(SPAWN 2);vr_decap_test 17;wait5;$(SPAWN 2);vr_decap_test 15;wait5;vr_decap_zombies 0;$(SPAWN 2);vr_decap_test 17;wait5;vr_decap_zombies 1" ;;
    monsters) for m in 0 5 1 8 6 7 4; do run "map vrfiringrange;$PRE;$(SPAWN $m);vr_decap_test 1;wait60;vr_ragdoll_list 1"; done ;;
    esac
done
