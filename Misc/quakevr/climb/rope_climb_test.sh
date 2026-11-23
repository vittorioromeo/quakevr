#!/bin/bash
# rope_climb_test.sh <agent> [play ...] (docs/vr-port/ROPES.md, "Tests"): vr_rope_climb_test's mock-hand plays in vrropes,
# one game each (rope_plays.py writes them to the agent's scratch/climb):
#   rope_climb  hand over hand up (at least 30 units), down (at least 15), a nudged swing carries the body (15), let go:
#               the fall keeps the rope's speed at the hands
#   rope_jump   up, the nudge, a jump: up by vr_rope_jump over the rope's own, its level speed kept
#   rope_pump   up, the forward key held 6 s: the swing pumped to at least 250 units/s at the hands, no more than the cap
#   rope_calm   (ropes6) four pulls up hand over hand, the body close to it: up at least 50, the swing at the hands at
#               most 25 units/s and its free end at most 30 (each regrab barely moves it), each pull grunts (3 or 4)
#   rope_short  (ropes6) a short rope (425 long: its end near his legs) taken overhead with both hands and pulled up off
#               the floor: the feet hang at least 10 units under the pelvis, not reaching up for its end
#   pole_wood, pole_metal  (ropes7) a pole standing ahead: up hand over hand (30), the feet round it, its material's grip
#               sound, down (15); it never moved
#   monkey_drop, monkey_jump  (ropes7) a rope strung level overhead across him: taken with both hands and pulled down
#               (a monkey traverse, the feet up 10 and hanging), hand over hand along it (15), then let go (dropped 5)
#               or jump off (as off a rope)
#   pole_monkey  (ropes8a) monkey_drop on a level pole strung across him (no sag)
#   pole_jump  (ropes10) up a wooden pole, jump with the grips held: a normal jump up and pushed off it (vr_pole_jump,
#               _push), a free fall to the floor (no frame held back), the held grips not taking it again
#   pole_through  (ropes14) pole_jump, then straight back through the pole at 320 units/s: let through a moment after
#               letting go (vr_rope_letgo_pass), on its far side
#   monkey_keep  (ropes14) a level pole taken with the controllers turned; the head turned round and across under it: each
#               hand keeps the grip it took; the main hand taken again turned the other way: the other grip; pulled up
#   monkey_support  (ropes14) under a level pole, the hands pushed down (the body over it): the wrists turn over it (the
#               palms on top, the knuckles down); back under it: turned back
#   (ropes14) rope_climb and pole_wood/metal also check the feet eased (legjerk: vr_rope_leg_smooth)
# Each ends on its verdict line ("vr_rope_climb_test: N passed, M failed").
AGENT=${1:?agent}; shift
PLAYS=${*:-rope_climb rope_jump rope_pump rope_calm rope_short pole_wood pole_metal monkey_drop monkey_jump pole_monkey pole_jump pole_through monkey_keep monkey_support}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
D=C:/OHWorkspace/qvr-agents/$AGENT/scratch/climb
python "$HERE/rope_plays.py" "$D" > /dev/null || exit 1
for play in $PLAYS; do
    echo "== $play"
    length=""
    [ "$play" = rope_short ] && length=425
    [ "$play" = pole_wood ] && length="0 wood"
    [ "$play" = pole_metal ] && length="0 metal"
    [ "$play" = pole_jump ] && length="0 wood"
    case $play in monkey_*) length="0 strung" ;; esac
    [ "$play" = pole_monkey ] && length="0 strungpole"
    [ "$play" = pole_through ] && length="0 wood"
    [ "$play" = monkey_keep ] && length="0 strungpole"
    [ "$play" = monkey_support ] && length="0 strungpole"
    bash "$KIT/run.sh" "$AGENT" -Script "map vrropes; vr_rope_climb_test \"$D/$play.txt\" $length" -Filter "vr_rope_climb_test" \
        -QuitOn "vr_rope_climb_test: [0-9]+ passed" -Timeout 120 | grep "vr_rope_climb_test: \(FAIL\|[0-9]* passed\)"
done
