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
#   movelight  the off hand on a light large crate (vr_climb_prop_mass 70), the main on a heavy explosive box (80.5 kg)
#          beside it, flung away at 150 units/s: while both hold, the body follows the box's hold all the way (the main
#          hand's tracked hand within 1.5 units of it, the arms with room to bend: vr_body_shoulder_reach 0.5) and ends
#          carried 20+ units; a light hold no longer halves the carry (before: drifted 11 units)
#   liftlight  the off hand on a light large crate beside vrclimb's lift, the main on the lift's lip as it rises: the body
#          rides the lift at its speed while both hold (the push carries it: VR_ClimbCarried), sync within 1.5
#   lip    the small crates' lips by vr_climb_try (two games, no climb): a small one on a large one (spawn 123; its lip
#          80 over the feet, 4 behind the large one's) is a hold; a small one alone (spawn 124; 32 over the feet) is none
#          from the ground (under vr_climb_prop_min_height 36: the grip carries it), a hold at 30
# light, close: the let-go is the slip ("pulled away"), not any let-go.
# The plays (climb_plays.py pullup) are written to the agent's scratch/climb. Prints each case's lines and the verdicts.
AGENT=${1:?agent}; shift
CASES=${*:-heavy stack side light close knock hang off lift movelight liftlight lip}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
export TB_NODRAW=${TB_NODRAW-1} # (batched frames not drawn: no check here reads the drawing; testbatch.sh)
# (The body: tb_main plays its runs as batches, testbatch.sh; TB_SLOW=1: each alone by run.sh, as before.)
tb_body() {
HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
W=C:/OHWorkspace/qvr-agents/$AGENT
D=$W/scratch/climb
mkdir -p "$D"
OFFX=${OFFX:--0.83}; MAINX=${MAINX:-0.35}; MAINY=${MAINY:-2.0}; MAINZ=${MAINZ:--0.55}; FLING=${FLING:-150}
python -c "
import sys; sys.path.insert(0, sys.argv[1]); import climb_plays as c
for name, args in (('pc_mantle', (1.52, 0.72, -0.69, -0.49)), ('pc_stack', (3.04, 0.72)), ('pc_side', (1.27, 0.47, -0.69, -0.49)),
                   ('pc_hang', (1.52, 1.37, -0.69, -0.69)), ('pc_hangfar', (1.52, 1.37, -1.0, -1.0))):
    L, t = c.pullup(*args); c.write(sys.argv[2] + '/' + name + '.txt', L)
L, t = c.mover('liftride'); c.write(sys.argv[2] + '/pc_liftride.txt', L)
# liftlight, movelight: the off hand far left onto a light crate's lip; the main hand onto the lift's lip as it rises
# (as liftride's), or onto a heavy explosive box's
for name, offx, main in (('pc_liftlight', -0.7, (0.12, 1.638, -0.69)), ('pc_movelight', $OFFX, ($MAINX, $MAINY, $MAINZ))):
    L = []
    for h, (x, y, z) in (('off', (offx, 1.52, -0.69)), ('main', main)):
        L += [f'{t:.3f} {h} {p[0]} {p[1]} {p[2]} 0 0 0' for t, p in ((0, (-0.12 if h == 'off' else 0.12, 1.2, -0.3)), (0.5, (x, y, z)), (12.0, (x, y, z)))]
    L += ['0.700 cmd +graboff', '0.750 cmd +grabmain', '11.000 cmd -graboff', '11.000 cmd -grabmain']
    c.write(sys.argv[2] + '/' + name + '.txt', L)
" "$HERE" "$D"
# after the light case: the hands let go, the player onto the lift's trigger (it starts rising: its lip reaches the hands
# as they grip), holding its lip
LIFT="wait500;viewpos;-graboff;-grabmain;wait10;setpos -318 88 24 0 0 0;noclip;wait50;vr_mock_play $D/pc_liftride.txt"
fail=0
run() { # name kind play extra-before after-play frames dist
  tb_run_sh -Script "map vrclimb;wait30;god;notarget;developer 1;vr_climb 1;vr_climb_debug 1;setpos -300 -100 24 0 90 0;noclip;wait20;vr_test_spawn_dist $7;vr_test_spawn $2;impulse 241;wait90;$4;vr_mock_play $D/$3.txt;$5;wait$6;viewpos;vr_climb_debug 0;developer 0;toggleconsole;quit" \
    -Timeout 200 -Filter "climb:|ENGINE|Player pos" 2>&1 | grep "climb:\|ENGINE\|Player pos" | grep -v "hold search\|eases onto" | tr -d '\r'
}
climb2() { # setup play after: a light crate (vr_climb_prop_mass 70) in the off hand; vr_climb_debug 2's climbreach lines
  # (each hold's sync: how far its tracked hand drifted from the hold), the arms with room to bend (shoulder reach 0.5)
  tb_run_sh -Script "map vrclimb;wait30;god;notarget;developer 1;vr_climb 1;vr_climb_debug 2;vr_climb_prop_mass 70;vr_body_shoulder_reach 0.5;$1;vr_mock_play $D/$2.txt;$3;viewpos;vr_climb_debug 0;developer 0;vr_body_shoulder_reach 0.08;vr_climb_prop_mass 60;toggleconsole;quit" \
    -Timeout 200 -Filter "climb:|climbreach |ENGINE|Player pos| kg[)]" 2>&1 | grep -E "climb:|climbreach |ENGINE|Player pos| kg[)]" | grep -v "hold search\|eases onto" | tr -d '\r'
}
tries() { # spawn, then vr_climb_try commands: the test crate ahead, the tries from the ground
  tb_run_sh -Script "map vrclimb;wait30;god;notarget;developer 1;vr_climb 1;setpos -300 -100 24 0 90 0;noclip;wait20;vr_test_spawn_dist 40;vr_test_spawn $1;impulse 241;wait90;$2;vr_climb_prop_min_height 36;developer 0;toggleconsole;quit" \
    -Timeout 200 -Filter "climbtry|ENGINE" 2>&1 | grep "climbtry\|ENGINE" | tr -d '\r'
}
both() { # frames both hands held, and the main hand's largest sync meanwhile
  echo "$1" | awk '/^climbreach [0-9]/ { split($4, o, "/"); for(i = 1; i <= NF; i++) if($i == "main") split($(i + 1), m, "/");
    for(i = 1; i <= NF; i++) if($i == "sync") s = $(i + 2); if(o[1] > 0 && m[1] > 0) { n++; if(s > mx) mx = s } }
    END { printf "%d %.2f\n", n, mx }'
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
    # the light crate (vr_test_spawn 120) left of the lift (y 40..136), spawned from a spot beside it; the player onto
    # the lift's trigger
    liftlight) log=$(climb2 "setpos -316 161 24 0 0 0;noclip;wait20;vr_test_spawn_dist 40;vr_test_spawn 120;impulse 241;wait90;setpos -318 126 24 0 0 0;noclip;wait50" pc_liftlight "wait240");;
    # on the open floor facing +y: the light crate ahead-left, the explosive box (80.5 kg: heavy) ahead-right, its face
    # 16 ahead as the crate's; flung sideways (+x, along its lip) once both hands hold
    movelight) log=$(climb2 "setpos -342 -100 24 0 90 0;noclip;wait20;vr_test_spawn_dist 40;vr_test_spawn 120;impulse 241;wait30;vr_physics_spawn misc_explobox 16 -44;wait60;setpos -300 -100 24 0 90 0;noclip;wait30" pc_movelight "wait90;vr_physics_fling nearest $FLING 90;wait120");;
    # the small crates' lips (their front 16 ahead of the middle: y -76; a hold's hand 2 behind)
    lip)   log=$(tries 123 "vr_climb_try -300 -74 80 main"; tries 124 "vr_climb_try -300 -74 32 main;vr_climb_prop_min_height 30;vr_climb_try -300 -74 32.5 main");;
    *) echo "unknown case $c"; continue;;
  esac
  echo "$log" > "$D/pc_$c.log" # (the whole log, the climbreach lines too)
  echo "$log" | grep -v "^climbreach "
  read -r x y z <<< "$(pos "$log")"
  read -r nboth msync <<< "$(both "$log")"
  [ "$c" = liftlight ] || [ "$c" = movelight ] && echo "both hands held $nboth frames, the main hand's sync at most $msync"
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
    # (the holds' lines wrap: their props' "(light, 65 kg)" / "(heavy, 80 kg)" on lines of their own)
    liftlight) [ "$(echo "$log" | grep -c "(light, ")" = 1 ] && ! echo "$log" | grep -q "(heavy, " &&
             echo "$log" | grep -q "main hand holds" &&
             awk -v n="$nboth" -v s="$msync" -v z="$z" 'BEGIN{exit !(n >= 10 && s < 1.5 && z > 40)}' && ok=1;;
    movelight) [ "$(echo "$log" | grep -c "(light, ")" = 1 ] && [ "$(echo "$log" | grep -c "(heavy, ")" = 1 ] &&
             awk -v n="$nboth" -v s="$msync" -v y="$y" 'BEGIN{exit !(n >= 10 && s < 1.5 && y > -80)}' && ok=1;;
    lip)   echo "$log" | grep -q "climbtry -300.00 -74.00 80.00 main: exact hold .* top 80.1" &&
             echo "$log" | grep -q "climbtry -300.00 -74.00 32.00 main: none.* 1 low" &&
             echo "$log" | grep -q "climbtry -300.00 -74.00 32.50 main: exact hold .* top 32.1" && ok=1;;
  esac
  verdict $c $ok
done
exit $fail
}
source "$(dirname "$0")/../testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
