#!/bin/bash
# teleporters_test.sh <agent> [walk|throw|chase|views|all]: headless checks of vrteleporters (ROUND21.md, "Teleporter test map")
# with the agent kit (C:/OHWorkspace/qvr-kit). One game run per line printed (chase and throw) or per section (walk, views).
#   walk   the player walks into each gate from 64 units out (both sides), the high sill also with a jump: the
#          engine's crossings (developer 1: "VR portal: carried edict 1 through side N") and where he ends up
#   throw  a shells box thrown at the flush player gate and the framed crate hatch; a small crate at the flush
#          player gate (sheet 8 deep) and the flush large one (48 deep): where each lands
#   chase  a monster spawned 200 units from the player, who is then put 150 units past the gate in the north room: the
#          monster's place every 15 frames; prints when it reaches the north room and where it came out. Quake's sight
#          (vr_stealth_meter 0): the stealth meter takes seconds to fill on a still player, this test needs it hostile
#          before he moves (the stealth AI's own tests: vr_stealth_test)
#          Expected: the dogs and fiends through (the sill-32 dog bumps the sill, slides along the wall and takes the
#          sill-16 gate: 165-270 frames, now and then not within the 480); the grunt never: it stands and shoots through
#          the gate (PORTAL_AI.md)
#   views  a screenshot of both eyes from 64 units in front of a gate of each kind (scratch/teleporter_views.png)
AGENT=${1:?agent}; WHAT=${2:-all}; ONLY=$3  # ONLY: a chase's name (framed_large) to run only it
KIT=C:/OHWorkspace/qvr-kit
HERE=$(cd "$(dirname "$0")" && pwd)
TREE=$(cd "$HERE/../../.." && pwd -W)
LOG=$KIT/bases/$AGENT/qbase/qconsole.log
mkdir -p "$TREE/scratch"
TESTS=$(python "$HERE/make_vrteleporters_map.py" --tests)
run() { bash $KIT/run.sh $AGENT "$@" > /dev/null; }
pos() { echo "$TESTS" | awk -F'|' -v n="$1" -v c="$2" '$1 == n { print $c }'; }

if [ "$WHAT" = walk ] || [ "$WHAT" = all ]; then
    S="developer 1;map vrteleporters;wait60;god;notarget"
    while IFS='|' read -r name a b fa fb; do
        for side in A B; do
            p=$a; [ $side = B ] && p=$b
            S="$S;echo WALK ${name}_$side;setpos $p;wait5;noclip 0;wait5;vr_mock_stick off 0 1;wait60;vr_mock_stick off 0 0;wait5;viewpos"
        done
    done <<< "$TESTS"
    S="$S;echo WALK framed_highsill_A_jump;setpos $(pos framed_highsill 2);wait5;noclip 0;wait5;vr_mock_stick off 0 1;wait12;+jump;wait5;-jump;wait45;vr_mock_stick off 0 0;wait5;viewpos"
    run -Script "$S;toggleconsole;quit"
    awk '/^WALK /{if (n) print n, (c ? c : "not carried"), p; n=$2; c=""; p=""}
         /VR portal: carried edict 1 through side/{c = c "side " $8 " "}
         /^Player pos:/{p=$3 " " $4 " " $5 " yaw " $7}
         END{print n, (c ? c : "not carried"), p}' "$LOG"
fi

if [ "$WHAT" = throw ] || [ "$WHAT" = all ]; then
    MOCK="$TREE/scratch/teleporters_throw.mock"
    python - "$MOCK" <<'PY'
import sys
L = ["0.000 main 0.15 1.25 -0.10 70 0 0"]
for i in range(21):  # 0.6 m forward (rising 9 cm) in 0.16 s, minimum jerk; let go at the fastest
    s = i / 20; d = 0.6 * (10 * s ** 3 - 15 * s ** 4 + 6 * s ** 5)
    L.append(f"{0.3 + 0.16 * s:.4f} main 0.15 {1.25 + 0.15 * d:.4f} {-0.10 - d:.4f} 70 0 0")
L += ["0.3880 cmd -grabright", "0.3880 button main grip 0"]
open(sys.argv[1], "w", newline="\n").write("\n".join(L) + "\n")
PY
    for t in "101 -256 560 flush_player" "101 1000 560 framed_crate" "107 -256 600 flush_player_8deep" "107 0 600 flush_large_48deep"; do
        set -- $t
        run -Script "developer 1;map vrteleporters;wait60;god;notarget;setpos $2 $3 24 0 90 0;wait5;noclip 0;vr_mock_hand main 0.15 1.25 -0.10 70 0 0;wait20;+grabright;vr_mock_button main grip 1;wait5;vr_test_spawn $1;vr_test_spawn_hold 1;impulse 241;wait30;echo THROWN;vr_mock_play $MOCK;wait150;echo LANDED;entities;toggleconsole;quit"
        speed=$(grep -o "carry: thrown at .*(\s*[0-9.]* u/s)" "$LOG" | grep -o "( *[0-9.]* u/s)")
        # the thing in the hand was spawned last: the highest-numbered box or crate after LANDED
        at=$(awk '/^LANDED/{f=1} f && /:(maps\/b_shell|progs\/vr_crate)/{l=$0} END{print l}' "$LOG" | grep -o "(.*)" | head -1)
        echo "throw $( [ $1 = 101 ] && echo shells || echo crate ) at $4 $speed: lands $at (north room: y > 928)"
    done
fi

if [ "$WHAT" = chase ] || [ "$WHAT" = all ]; then
    for t in "7 dog -256 flush_player" "7 dog 1180 framed_sill16" "7 dog 1360 framed_sill32" "9 demon 0 flush_large" "9 demon 1580 framed_large" "0 soldier -256 flush_player"; do
        [ -n "$ONLY" ] && [[ "$t" != *"$ONLY"* ]] && continue
        set -- $t
        S="developer 1;map vrteleporters;wait60;god;vr_stealth_meter 0;setpos $3 600 24 0 270 0;wait5;noclip 0;wait5;vr_test_spawn $1;vr_test_spawn_dist 200;impulse 241;wait40;echo MOVE;setpos $3 1150 24 0 270 0;wait5;noclip 0"
        for i in $(seq 1 32); do S="$S;wait15;echo SNAP $i;entities"; done
        run -Script "$S;toggleconsole;quit"
        awk -v m="progs/$2.mdl:" -v what="$2 at $4" '/^SNAP/{s=$2} index($0, m) && s {split($0, a, "("); split(a[2], b, ","); y=b[2]+0
             if (y > 928 && !done) {printf "chase %s: in the north room after %d frames, at (%s,%s\n", what, s * 15, b[1], b[2]; done=1}
             last=a[2]} END{if (!done) printf "chase %s: never reached the north room; last at (%s\n", what, last}' "$LOG" | cut -d')' -f1 | sed 's/$/)/'
    done
fi

if [ "$WHAT" = views ] || [ "$WHAT" = all ]; then
    S="map vrteleporters;wait60;vr_mirror 2;vr_window_view 0"
    for n in flush_crate flush_player flush_large flush_wide framed_player framed_wide loop turn90 turn45 heights; do
        p=$(pos $n 2); set -- $p
        # 96 units further back than the walk's place, along the way he faces
        S="$S;setpos $(python -c "import math; x,y,z,a,b,c=map(float,'$p'.split()); r=math.radians(b); print(f'{x-96*math.cos(r):.0f} {y-96*math.sin(r):.0f} {z:.0f} 0 {b:.0f} 0')");wait30;screenshot"
    done
    bash $KIT/run.sh $AGENT -Clean -Out teleporter_views.png -Script "$S;toggleconsole;quit" -Filter composed | tail -1
fi
