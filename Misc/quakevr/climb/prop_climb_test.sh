#!/bin/bash
# prop_climb_test.sh <agent> [case ...] (ROUND22.md, "Climbing props"): the headless climbs of physics props on vrclimb's
# open floor (setpos -300 -100 24 facing +y; the test crate 40 units ahead: vr_test_spawn 120..122), one game each, PASS or
# FAIL per case:
#   heavy  a large crate (65 kg: holds like the level): grabbed, mantled onto, standing on it (z 72)
#   stack  two large crates stacked (96 high, the hands raised to 3.04 m): mantled onto the top one (z 120)
#   side   a large crate lying on its side (40 high): mantled onto (z 64)
#   light  a large crate made light (vr_climb_prop_mass 70), the player 10 units back: it tips towards him, the hands let
#          go, and he hasn't moved (within 2 units: not pulled along)
#   close  the same, the player against it: it can't tip (his body is in the way), the hands let go as it leans, he hasn't
#          moved
#   knock  a large crate held, flung away (vr_physics_fling, 300 units/s): the hands let go ("knocked away")
#   hang   a large crate held 6 s: no let-go, the body still (within 6 units of the pull's rise)
#   off    vr_climb_props 0: no hold on it
#   lift   the light case, then in the same game vrclimb's lift (climb_plays.py liftride): its lip held as it rises, the
#          holds not marked light (a light prop's flag was left on the hands: no carry from a hold the QC moves) and the
#          body carried up (over z 60)
# light, close: the let-go is the slip ("pulled away"), not any let-go. Not covered: the small crates (spawns 123, 124;
# vr_climb_try finds no hold at a small crate's lip, ROUND22.md "Review fixes").
# The plays (climb_plays.py pullup) are written to the agent's scratch/climb. Prints each case's lines and the verdicts.
AGENT=${1:?agent}; shift
CASES=${*:-heavy stack side light close knock hang off lift}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
W=C:/OHWorkspace/qvr-agents/$AGENT
D=$W/scratch/climb
mkdir -p "$D"
python -c "
import sys; sys.path.insert(0, sys.argv[1]); import climb_plays as c
for name, args in (('pc_mantle', (1.52, 0.72, -0.69, -0.49)), ('pc_stack', (3.04, 1.0)), ('pc_side', (1.27, 0.47, -0.69, -0.49)),
                   ('pc_hang', (1.52, 1.37, -0.69, -0.69)), ('pc_hangfar', (1.52, 1.37, -1.0, -1.0))):
    L, t = c.pullup(*args); c.write(sys.argv[2] + '/' + name + '.txt', L)
L, t = c.mover('liftride'); c.write(sys.argv[2] + '/pc_liftride.txt', L)
" "$HERE" "$D"
# after the light case: the hands let go, the player onto the lift's trigger (it starts rising: its lip reaches the hands
# as they grip), holding its lip
LIFT="wait500;viewpos;-graboff;-grabmain;wait10;setpos -318 88 24 0 0 0;noclip;wait50;vr_mock_play $D/pc_liftride.txt"
fail=0
run() { # name kind play extra-before after-play frames dist
  bash $KIT/run.sh $AGENT -Script "map vrclimb;wait30;god;notarget;developer 1;vr_climb 1;vr_climb_debug 1;setpos -300 -100 24 0 90 0;noclip;wait20;vr_test_spawn_dist $7;vr_test_spawn $2;impulse 241;wait90;$4;vr_mock_play $D/$3.txt;$5;wait$6;viewpos;vr_climb_debug 0;developer 0;toggleconsole;quit" \
    -Timeout 200 -Filter "climb:|ENGINE|Player pos" 2>&1 | grep "climb:\|ENGINE\|Player pos" | grep -v "hold search\|eases onto" | tr -d '\r'
}
verdict() { # name ok
  if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1"; fail=1; fi
}
pos() { # the log's last Player pos: x y z
  echo "$1" | grep "Player pos" | tail -1 | sed 's/.*(\([-0-9.]*\) \([-0-9.]*\) \([-0-9.]*\)).*/\1 \2 \3/'
}
for c in $CASES; do
  echo "== $c"
  case $c in
    heavy) log=$(run heavy 120 pc_mantle echo echo 300 40);;
    stack) log=$(run stack 121 pc_stack echo echo 360 40);;
    side)  log=$(run side 122 pc_side echo echo 300 40);;
    light) log=$(run light 120 pc_hangfar "vr_climb_prop_mass 70" echo 500 50);;
    close) log=$(run close 120 pc_hang "vr_climb_prop_mass 70" echo 500 40);;
    knock) log=$(run knock 120 pc_hang echo "wait150;vr_physics_fling nearest 300 90" 300 40);;
    hang)  log=$(run hang 120 pc_hang echo echo 450 40);;
    off)   log=$(run off 120 pc_hang "vr_climb_props 0" echo 200 40);;
    lift)  log=$(run lift 120 pc_hangfar "vr_climb_prop_mass 70" "$LIFT" 500 50);;
    *) echo "unknown case $c"; continue;;
  esac
  echo "$log"
  read -r x y z <<< "$(pos "$log")"
  ok=0
  case $c in
    heavy) echo "$log" | grep -q "mantled onto" && awk -v z="$z" 'BEGIN{exit !(z > 70 && z < 75)}' && ok=1;;
    stack) echo "$log" | grep -q "mantled onto" && awk -v z="$z" 'BEGIN{exit !(z > 118 && z < 123)}' && ok=1;;
    side)  echo "$log" | grep -q "mantled onto" && awk -v z="$z" 'BEGIN{exit !(z > 60 && z < 68)}' && ok=1;;
    light|close) echo "$log" | grep -q "lets go of prop.*pulled away" && awk -v x="$x" -v y="$y" 'BEGIN{exit !((x+300)^2 + (y+100)^2 < 4)}' && ok=1;;
    knock) echo "$log" | grep -q "knocked away" && ok=1;;
    hang)  echo "$log" | grep -q "hand holds" && ! echo "$log" | grep -q "lets go" &&
             awk -v x="$x" -v y="$y" -v z="$z" 'BEGIN{exit !((x+300)^2 + (y+100)^2 < 4 && z < 31)}' && ok=1;;
    off)   ! echo "$log" | grep -q "hand holds" && ok=1;;
    lift)  [ "$(echo "$log" | grep -c "hand holds")" = 4 ] && ! echo "$log" | grep -q "hand holds at [0-9.]*, light" &&
             awk -v z="$z" 'BEGIN{exit !(z > 60)}' && ok=1;;
  esac
  verdict $c $ok
done
exit $fail
