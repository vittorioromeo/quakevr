#!/bin/bash
# forcegrab_block_test.sh <agent> [pitch] -- a force grab past a body (ROUND22.md, "Force grabs: bodies block the view"):
# on the firing range a health box 150 units ahead, the empty main hand pointing down at it, the trigger pulled and
# the hand flicked up 0.3 m in 0.1 s, then the grip (the mock's force grab); with nothing between, with a grunt 80
# units ahead (alive, notarget) and with a corpse there (vr_test_spawn_dead 1: a ragdoll, vr_ragdoll 1); the grunt with vr_forcegrab_bodies_block
# 0 too. Each case: "force grab: caught" or not; developer 2 also "forcegrab: ... behind monster_army, skipped".
NAME="$1"; P="${2:-60}"; KIT=C:/OHWorkspace/qvr-kit; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; mkdir -p "$ROOT/scratch"
PLAY="$ROOT/scratch/fgblock_play.txt"
printf '%s\n' "0.000 main 0.2 1.2 -0.3 $P 0 0" "0.000 off -0.25 1.1 -0.2 70 0 0" "0.300 cmd +attack" \
    "0.400 main 0.2 1.2 -0.3 $P 0 0" "0.500 main 0.2 1.5 -0.25 $P 0 0" "0.550 cmd +grabright" "0.550 grip main 1" \
    "0.600 cmd -attack" "2.000 main 0.2 1.5 -0.25 $P 0 0" > "$PLAY"
BASE="map vrfiringrange;wait60;god;notarget;developer 2;vr_test_spawn_facing 1;vr_mock_hand main 0.2 1.2 -0.3 $P 0 0;vr_mock_hand off -0.25 1.1 -0.2 70 0 0;vr_test_spawn 100;vr_test_spawn_dist 150;impulse 241;wait20"
one() { # <label> <cvars before the grab>
    echo "== $1"
    bash $KIT/run.sh "$NAME" -Script "$BASE;$2;wait30;vr_mock_play $PLAY;wait300;toggleconsole;quit" -Filter "force grab: caught|behind .*skipped" 2>&1 | grep -v "^$\|^exit" | sort | uniq -c | head -4
}
one "nothing between" ""
one "a grunt between" "vr_test_spawn 0;vr_test_spawn_dist 80;impulse 241"
one "a grunt between, bodies_block 0" "vr_forcegrab_bodies_block 0;vr_test_spawn 0;vr_test_spawn_dist 80;impulse 241"
one "a corpse between" "vr_test_spawn 0;vr_test_spawn_dist 80;vr_test_spawn_dead 1;impulse 241;wait60"
