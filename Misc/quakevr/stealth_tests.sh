#!/bin/bash
# stealth_tests.sh <agent> [gun|blast|kinds|infight|saveload|liquid|gates|seethrough|hunt|dogs|props|horde|all] -- the stealth AI's further scenes
# (QC vr_stealth_test2.qc; docs/vr-port/STEALTH_PLAN.md, "Tests"), headless on e1m1 (kinds: id1's, hipnotic's and rogue's
# monsters: the kit's games mount both). Prints the `stealthtest:` lines; exits 1 on a FAIL. Coop: Misc/quakevr/multiplayer/stealth_mp_test.sh.
#   gun      each weapon's real shot (the trigger pulled): heard at 0.8 of its reach, not at 1.2, not behind a wall
#   blast    explosions of 50 and 200 damage: heard by their size, walls absorbing
#   kinds    every monster kind: the rules apply (or it is left to Quake's AI), dark idle, a knock alerts, lit spots
#   infight  a monster hit by another fights it, then stands Idle again (the meter, not Quake's sight)
#   saveload an investigation saved mid-walk, loaded, carried on to Idle at its post
#   liquid   an investigation across lava or slime stops at the edge (e1m7's lava by default: LIQMAP)
#   horde    60 idle grunts round you (vr_profile's "stealth" scope: its share of the frame)
#   gates    through seamless teleporters (vrteleporters, QC vr_stealth_test3.qc): a grunt sees you through a gate, walks
#            through to where you stood and back (two crossings); a knock heard through it, the same (and not with
#            vr_stealth_gates 0); a dog sees you through it and comes through (not with vr_stealth_gates 0); Quake's AI
#            (vr_ai_enhanced 0): only the grunt sees you through
#   seethrough a wall between you and a grunt (QC vr_stealth_test4.qc): opaque, it neither sees you nor hears a knock;
#            alpha 0.5 or its textures fences ('{'): both; vr_stealth_seethrough 0: neither
#   hunt     a Hostile grunt loses you as you dart off: it follows your trail (your last spot, then ahead along your
#            way), gives up at vr_stealth_lose_time (8 s here); with you far (vr_stealth_lose_far), after a quarter
#   dogs     a dog chasing you as you run in circles, its drawn moves logged (vr_debug_drawn_moves): with
#            vr_monster_lerp_continue 1 no jump between frames; 0 (Quake's drawing) for comparison
#   props    a row of crates between a Hostile dog and you (vrteleporters, room T): it gets round them (vr_ai_props 1);
#            with 0, Quake's way, its time for comparison
AGENT=${1:?agent}; WHICH=${2:-all}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
W=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$W/scratch"
fail=0
PRE="vr_fixed_frames 1;vr_fixed_frames_rate 90;map e1m1;wait60;god;vr_weapon_grip_mode 1;impulse 9;wait5"
run() { # <tag> <script> [extra run.sh args]
    local tag=$1 s=$2; shift 2
    bash $KIT/run.sh $AGENT -Script "$s" -Filter "stealthtest:|rror|ENGINE|stealth:.*(hostile|alert)|vr_profile|stealth |drawnmove" -Timeout 600 "$@" > "$W/scratch/stealth_$tag.log" 2>&1
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
if [ "$WHICH" = gates ] || [ "$WHICH" = all ]; then
    run gates "developer 1;vr_fixed_frames 1;vr_fixed_frames_rate 90;map vrteleporters;wait60;god;setpos -1100 1408 24 0 180 0;wait5;noclip 0;vr_stealth_test 110;wait16000;toggleconsole;quit"
    grep -q "gates done" "$W/scratch/stealth_gates.log" || { echo "stealthtest: gates FAIL (never finished)"; fail=1; }
fi
if [ "$WHICH" = seethrough ] || [ "$WHICH" = all ]; then
    run seethrough "$PRE;vr_stealth_test 120;wait2000;toggleconsole;quit"
    grep -q "seethrough done" "$W/scratch/stealth_seethrough.log" || { echo "stealthtest: seethrough FAIL (never finished)"; fail=1; }
fi
if [ "$WHICH" = hunt ] || [ "$WHICH" = all ]; then
    run hunt "$PRE;vr_stealth_test 122;wait3000;toggleconsole;quit"
    grep -q "hunt done" "$W/scratch/stealth_hunt.log" || { echo "stealthtest: hunt FAIL (never finished)"; fail=1; }
fi
if [ "$WHICH" = dogs ] || [ "$WHICH" = all ]; then
    for on in 0 1; do
        run dogs$on "$PRE;vr_monster_lerp_continue $on;vr_debug_drawn_moves progs/dog;vr_stealth_test 123;wait3000;toggleconsole;quit"
        J=$(grep -c "drawnmove: [0-9.]* ent" "$W/scratch/stealth_dogs$on.log")
        echo "stealthtest: dog_steps vr_monster_lerp_continue $on: $J jumps between frames ($(grep -o '[0-9]* frames drawn.*' "$W/scratch/stealth_dogs$on.log" | tail -1))"
        grep -q "dogs done" "$W/scratch/stealth_dogs$on.log" || { echo "stealthtest: dogs FAIL (never finished)"; fail=1; }
        [ "$on" = 1 ] && [ "$J" != 0 ] && { echo "stealthtest: dog_steps FAIL (jumps with the fix on)"; fail=1; }
    done
fi
if [ "$WHICH" = props ] || [ "$WHICH" = all ]; then
    run props "vr_fixed_frames 1;vr_fixed_frames_rate 90;map vrteleporters;wait60;god;vr_stealth_test 124;wait3500;toggleconsole;quit"
    grep -q "props done" "$W/scratch/stealth_props.log" || { echo "stealthtest: props FAIL (never finished)"; fail=1; }
fi
if [ "$WHICH" = horde ]; then
    run horde "$PRE;vr_stealth_test 103;wait90;vr_profile 1;wait900;vr_profile_report 8;vr_profile 0;wait1000;toggleconsole;quit" -RealTime
fi
exit $fail
