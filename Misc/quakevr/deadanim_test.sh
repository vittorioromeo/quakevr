#!/bin/bash
# deadanim_test.sh <agent> [choice ...] -- dead monsters stay dead (QC vr_deadanim.qc; ROUND22.md, "A beheaded zombie
# knight ran again"): each monster (the debug spawner's choice: 0 grunt, 2 zombie, 5 knight; AD's 200 death knight,
# 201 zombie knight, 211 rocket soldier, 238 mummy) beheaded (vr_decap_test 1) or killed by a blow of its health
# (vr_deadanim_test 6) in each state, then its use fired (a trigger reaching it: vr_deadanim_test 5) and watched 15 s
# with vr_debug_deadanim 1. Every "deadanim: dead ..." line is a dead one animating again (a failure); each case ends
# with the watch's report ("deadanim: report: N dead watched, 0 warnings").
#   stand   standing          down    knocked down (vr_knockdown_test 0: its ragdoll)
#   getup   getting up (1)    stagger its ragdoll's stagger (vr_deadanim_test 2)
#   parry   a parry's stagger (3)   fall    a zombie's own fall without its ragdoll (4: AD's, Quake's)
#   floor   an AD zombie lying dormant as a map's (8: up when its trigger fires: the use fired after its death)
# CASES picks some (default all); KILL=1 kills instead of beheading. MAP: an AD map for AD's (default start; others e1m1).
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CHOICES=${*:-201}
CASES=${CASES:-stand down getup stagger parry fall}
BLOW="vr_decap_test 1"
if [ "${KILL:-0}" = 1 ]; then BLOW="vr_deadanim_test 6"; fi
FILTER="${FILTER:-^deadanim|^decaptest: [0-9]|^knockdown test|rror|CRASH}"
for c in $CHOICES; do
    if [ "$c" -ge 200 ]; then
        PRE="skill 1;vr_campaign_native ad;wait30;map ${MAP:-start}"
    else
        PRE="skill 1;map ${MAP:-e1m1}"
    fi
    PRE="$PRE;wait60;god;notarget;developer 1;vr_decap_pop_roll 0;vr_test_spawn_dist 90;vr_knockdown_test 20;wait2;vr_debug_deadanim 1;vr_test_spawn $c;impulse 241;wait60"
    for k in $CASES; do
        case $k in
        stand)   S="" ;;
        down)    S="vr_knockdown_test 0;wait30;" ;;
        getup)   S="vr_knockdown_test 0;wait30;vr_knockdown_test 1;wait${GETUP_WAIT:-40};" ;;
        stagger) S="vr_deadanim_test 2;wait4;" ;;
        parry)   S="vr_deadanim_test 3;wait4;" ;;
        fall)    S="vr_deadanim_test 4;wait25;" ;;
        floor)   S="vr_deadanim_test 8;wait10;" ;;
        esac
        echo "== $c $k"
        bash "$KIT/run.sh" "$AGENT" -Script "$PRE;${S}vr_deadanim_test 9;wait1;$BLOW;wait400;vr_deadanim_test 5;wait700;vr_deadanim_test 1;wait2;toggleconsole;quit" \
            -Filter "$FILTER" -Timeout 300 2>&1 | grep -v "^$\|^exit=0"
    done
done
