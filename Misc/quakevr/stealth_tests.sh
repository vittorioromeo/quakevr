#!/bin/bash
# stealth_tests.sh <agent> [gun|blast|kinds|infight|saveload|liquid|horde|all] -- the stealth AI's further scenes
# (QC vr_stealth_test2.qc; docs/vr-port/STEALTH_PLAN.md, "Tests"), headless on e1m1 (kinds: id1's, hipnotic's and rogue's
# monsters: the kit's games mount both). Prints the `stealthtest:` lines; exits 1 on a FAIL. Coop: Misc/quakevr/multiplayer/stealth_mp_test.sh.
#   gun      each weapon's real shot (the trigger pulled): heard at 0.8 of its reach, not at 1.2, not behind a wall
#   blast    explosions of 50 and 200 damage: heard by their size, walls absorbing
#   kinds    every monster kind: the rules apply (or it is left to Quake's AI), dark idle, a knock alerts, lit spots
#   infight  a monster hit by another fights it, then stands Idle again (the meter, not Quake's sight)
#   saveload an investigation saved mid-walk, loaded, carried on to Idle at its post
#   liquid   an investigation across lava or slime stops at the edge (e1m7's lava by default: LIQMAP)
#   horde    60 idle grunts round you (vr_profile's "stealth" scope: its share of the frame)
AGENT=${1:?agent}; WHICH=${2:-all}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
W=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$W/scratch"
fail=0
PRE="vr_fixed_frames 1;vr_fixed_frames_rate 90;map e1m1;wait60;god;vr_weapon_grip_mode 1;impulse 9;wait5"
run() { # <tag> <script> [extra run.sh args]
    local tag=$1 s=$2; shift 2
    bash $KIT/run.sh $AGENT -Script "$s" -Filter "stealthtest:|rror|ENGINE|stealth:.*(hostile|alert)|vr_profile|stealth " -Timeout 600 "$@" > "$W/scratch/stealth_$tag.log" 2>&1
    grep -E "stealthtest:|ENGINE|rror" "$W/scratch/stealth_$tag.log"
    grep -qE "FAIL|ENGINE (ERROR|CRASH)|TIMEOUT" "$W/scratch/stealth_$tag.log" && fail=1
}
if [ "$WHICH" = gun ] || [ "$WHICH" = all ]; then
    S="$PRE"
    for w in 154 155 156 157 158 160 161; do
        S="$S;impulse $w;wait10;vr_stealth_test 100;wait90;+attack;vr_mock_button main trigger 1;wait3;-attack;vr_mock_button main trigger 0;wait120"
    done
    run gun "$S;toggleconsole;quit"
fi
if [ "$WHICH" = blast ] || [ "$WHICH" = all ]; then
    run blast "$PRE;vr_stealth_test 101;wait300;toggleconsole;quit"
fi
if [ "$WHICH" = infight ] || [ "$WHICH" = all ]; then
    run infight "$PRE;vr_stealth_test 107;wait900;toggleconsole;quit"
fi
if [ "$WHICH" = saveload ] || [ "$WHICH" = all ]; then
    run saveload "$PRE;vr_stealth_test 104;wait300;save qvr_stealth_mid;wait90;load qvr_stealth_mid;wait30;vr_stealth_test 105;wait4500;toggleconsole;quit"
fi
if [ "$WHICH" = liquid ] || [ "$WHICH" = all ]; then
    run liquid "vr_fixed_frames 1;vr_fixed_frames_rate 90;map ${LIQMAP:-e1m7};wait60;god;notarget;${LIQPOS:+setpos $LIQPOS;wait5;}vr_stealth_test 108;wait1500;toggleconsole;quit"
fi
if [ "$WHICH" = kinds ] || [ "$WHICH" = all ]; then
    run kinds "$PRE;vr_stealth_test 106;wait30000;toggleconsole;quit" # (the kit's games mount hipnotic and rogue too)
fi
if [ "$WHICH" = horde ]; then
    run horde "$PRE;vr_stealth_test 103;wait90;vr_profile 1;wait900;vr_profile_report 8;vr_profile 0;wait1000;toggleconsole;quit" -RealTime
fi
exit $fail
