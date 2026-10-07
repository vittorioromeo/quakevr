#!/bin/bash
# held_over_props_test.sh <agent> -- a prop held low in one hand while walking over boxes lying on the floor
# (ROUND21.md, "Held props jittered over props on the floor"): a box of shells in the off hand (vr_test_held_pick 11)
# or a wall torch in the main hand, the hand at 0.75 m on the right, 1, 3 and 5 boxes of shells spawned ahead
# (vr_test_spawn 101), 150 frames walking over them. Per case: the held prop's drawn place in the hand, peak to peak
# (cm; 0 = steady), the physical prop off its hand's place (cm), and the held-prop wall pushes (vr_debug_carry). Before
# the fix the client's wall test met the boxes' unscaled brush hulls: 12-15 cm up and down for the box, 42-46 for the
# torch. PASS: every case 0 pushes and under 0.5 cm.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
for spec in "1 11" "3 11" "5 11" "3 torch" "5 torch"; do
    set -- $spec; n=$1; pick=$2
    s="map vrfiringrange;wait60;god;notarget;developer 1;vr_debug_carry 1"
    if [ "$pick" = torch ]; then
        s="$s;vr_physics_spawn light_torch_small_walltorch 64 0;vr_mock_hand main 0.1 1.25 -0.25 70 0 0;vr_mock_hand off -0.25 0.85 -0.15 0 0 0;wait20;+grabright;vr_mock_button main grip 1;vr_test_walltorch_shot 24;wait30;vr_mock_hand main 0.15 0.75 -0.15 0 0 0;wait20"
    else
        s="$s;vr_mock_hand off -0.2 0.85 -0.15 0 0 0;vr_mock_hand main 0.25 0.85 -0.15 0 0 0;wait10;+graboff;vr_mock_button off grip 1;vr_test_held_pick 11;impulse 252;wait20;vr_mock_hand off 0.15 0.75 -0.15 0 0 0;wait10"
    fi
    s="$s;vr_test_spawn 101"
    for i in $(seq 1 $n); do s="$s;vr_test_spawn_dist $((40 + 22 * i));impulse 241;wait3"; done
    s="$s;wait90;vr_mock_stick off 0 0.6"
    for i in $(seq 1 150); do s="$s;vr_carry_check;wait1"; done
    s="$s;vr_mock_stick off 0 0;wait5;toggleconsole;quit"
    log=$(bash $KIT/run.sh $AGENT -Script "$s" -Filter "^carry check: (off|main) hand: drawn|^carry check: the physical|^held: .*wall" 2>&1)
    line=$(echo "$log" | python -c "
import sys, re
t = sys.stdin.read()
d = [tuple(map(float, m)) for m in re.findall(r'drawn in the hand \(forward, left, up\) at (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) cm', t)]
ph = [float(m) for m in re.findall(r'the physical (-?[\d.]+) cm off', t)]
w = len(re.findall(r'against a wall', t))
pp = max((max(c) - min(c) for c in zip(*d)), default=-1)
print(f'{len(d)} {pp:.2f} {max(ph, default=-1):.2f} {w}')")
    set -- $line
    verdict=PASS
    if [ "$(python -c "print(1 if $1 >= 100 and $4 == 0 and 0 <= $2 < 0.5 else 0)")" != 1 ]; then
        verdict=FAIL; fail=1
    fi
    echo "$n boxes, held $pick: $1 frames, drawn in the hand p2p $2 cm, physical off $3 cm, wall pushes $4: $verdict"
done
[ $fail -eq 0 ] && echo "PASS" || echo "FAIL"
