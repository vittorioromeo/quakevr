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
# Each ends on its verdict line ("vr_rope_climb_test: N passed, M failed").
AGENT=${1:?agent}; shift
PLAYS=${*:-rope_climb rope_jump rope_pump rope_calm rope_short pole_wood pole_metal}
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
    bash "$KIT/run.sh" "$AGENT" -Script "map vrropes; vr_rope_climb_test \"$D/$play.txt\" $length" -Filter "vr_rope_climb_test" \
        -QuitOn "vr_rope_climb_test: [0-9]+ passed" -Timeout 120 | grep "vr_rope_climb_test: \(FAIL\|[0-9]* passed\)"
done
