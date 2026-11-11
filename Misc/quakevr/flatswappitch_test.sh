#!/bin/bash
# flatswappitch_test.sh <agent> [shots-prefix] -- flat-screen play's hand swap animation follows the view's pitch (his
# words 2026-11-11: "The off-hand's position is always put at the same height/angle during the animation, as if the player
# is looking straight ahead -- the current camera angle should be taken into account"; Quake/vr/vr_flatswap.cpp, Frame;
# docs/vr-port/FLATPLAY.md, "The hand swap"). Headless, fast mode, -flatscreenmode; each run ends on its own last line
# (-QuitOn). In each run the swap is played at pitch -60, 0, +60, +85 and -85 (the look's limit) (and swapped back each time), and
# vr_flat_swap_debug's "flatswap: in view" line (each hand's place and turn in the view's frame as they meet) must be the
# same at every pitch: flatswappitch_check.py prints the checks and "flatswappitch: N passed, M failed".
#   rl2h    the rocket launcher held in both hands: release, pass, attach
#   ads2h   the same, sighted (vr_flat_aim 1)
#   saw2h   the chainsaw, a two-handed melee weapon (vr_flat_melee_two_hand)
#   sg1h    a shotgun in the main hand, the off hand beside it: two-handed too since 2026-11-11 (vr_flat_swap_2h_all)
# With a shots prefix, the rl2h run also takes a screenshot mid-pass at pitch -60, 0 and +60 (composed into
# scratch/<prefix>_rl2h.png: the before and after strips).
AGENT=${1:?agent}
PREFIX=$2
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatswappitch
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(flatswap|flatswappitch_test|flat ?swap)"
BASE="map vrtesthall; god; notarget; developer 1; vr_flat_swap_debug 1; wait5; setpos 0 300 25 0 90 0; wait5"
SW="vr_mock_key 3 down; wait2; vr_mock_key 3 up"
RL2H="vr_test_give_wid 10; impulse 149; wait90" # (the rocket launcher, its grip taken by the free hand: flathands_test.sh)
# One pitch: the swap there (a screenshot `shotAt` frames after the press, if asked), then back, each to its end
# (release 0.2 + pass 0.25 + attach 0.25 s and the answer: 60 frames; the one-hand trip 0.15 + 0.2 s: 40).
pitchStep() { local p=$1 shotAt=$2 rest=$3 shot=$4
    local s="setpos 0 300 25 $p 90 0; wait10; $SW"
    if [ -n "$shot" ]; then s="$s; wait$shotAt; screenshot; wait$rest"; else s="$s; wait$((shotAt + rest))"; fi
    echo "$s; $SW; wait$((shotAt + rest))"; }
steps() { local shotAt=$1 rest=$2 shots=$3 out=""
    for p in -60 0 60; do out="$out; $(pitchStep $p $shotAt $rest $shots)"; done
    for p in 85 -85; do out="$out; $(pitchStep $p $shotAt $rest)"; done
    echo "$out"; }
run() { local tag=$1 script=$2 extra=$3
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode $extra -Script "$script; echo flatswappitch_test: $tag done; toggleconsole; quit" \
            -QuitOn "^flatswappitch_test: $tag done" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }

SHOTS=""; OUT=""
if [ -n "$PREFIX" ]; then SHOTS=1; OUT="-Out ${PREFIX}_rl2h.png -Clean"; fi
# (2h: mid-pass is 0.2 + 0.125 s after the press, 24 frames; 1h: halfway to the meeting point, 6 frames.)
run rl2h "$BASE; $RL2H; vr_flatplay_status$(steps 22 38 $SHOTS)" "$OUT"
[ -n "$PREFIX" ] && grep -a "composed" "$S/rl2h.log"
run ads2h "$BASE; $RL2H; vr_flat_aim 1; wait40$(steps 22 38)"
run saw2h "$BASE; impulse 164; wait90; vr_flatplay_status$(steps 22 38)"
run sg1h "$BASE; impulse 154; wait30$(steps 4 36)"

python "$HERE/flatswappitch_check.py" "$S"
exit $?
