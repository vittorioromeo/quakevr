#!/bin/bash
# prop_sides_test.sh <agent> [orientation ...] (ROUND22.md, "Prop climbing from any side"): a large crate (40 x 40 x 48)
# climbed from each of its four sides, in each orientation, on vrclimb's open floor (its middle at -300 -60), one game a
# climb: the player set square to the side, 20 units back from it (the gap prop_climb_test.sh's climbs have), both hands
# onto the lip and pulled over (climb_plays.py pullup, as prop_climb_test.sh's heavy and side). A side passes when the
# hands hold and the player mantles onto the top (standing over it: z over the top + 20).
#   up      upright (48 high), square to the floor's axes (vr_test_spawn 120)
#   side    lying on its long side (40 high, 48 x 40 top), its long axis along x (vr_test_spawn 122)
#   sidex   the same, its long axis along y
#   up45    upright, turned 45 degrees
#   side45  lying, turned 45 degrees
# Sides: 0 the face towards the spawning view (square to it), 1..3 on round it anticlockwise from above.
# Prints a line per climb (orientation, side, held, mantled, the end's z) and a table; exit 1 if any fails.
AGENT=${1:?agent}; shift
ORIENTS=${*:-up side sidex up45 side45}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
export TB_NODRAW=${TB_NODRAW-1}
tb_body() {
HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
W=C:/OHWorkspace/qvr-agents/$AGENT
D=$W/scratch/climb
mkdir -p "$D"
python -c "
import sys; sys.path.insert(0, sys.argv[1]); import climb_plays as c
for name, args in (('ps_up', (1.52, 0.72, -0.69, -0.49)), ('ps_side', (1.27, 0.47, -0.69, -0.49))):
    L, t = c.pullup(*args); c.write(sys.argv[2] + '/' + name + '.txt', L)
" "$HERE" "$D"
fail=0
table=""
for o in $ORIENTS; do
  case $o in
    up)     kind=120; yaw=90;  hb=20; play=ps_up;   top=72;;
    side)   kind=122; yaw=90;  hb=24; play=ps_side; top=64;;
    sidex)  kind=122; yaw=0;   hb=24; play=ps_side; top=64;;
    up45)   kind=120; yaw=135; hb=20; play=ps_up;   top=72;;
    side45) kind=122; yaw=135; hb=24; play=ps_side; top=64;;
    *) echo "unknown orientation $o"; continue;;
  esac
  row="$o:"
  for s in 0 1 2 3; do
    # the spawn spot (40 back from the middle along the spawn's view), the side's spot (its half-width + 20 out) and yaw
    read -r sx sy ax ay ayaw <<< "$(awk -v y="$yaw" -v s="$s" -v hb="$hb" 'BEGIN { r = atan2(0, -1) / 180;
      cx = -300; cy = -60; ny = y + 180 + 90 * s; h = (s % 2 == 0) ? 20 : hb;
      printf "%.2f %.2f %.2f %.2f %.1f\n", cx - 40 * cos(y * r), cy - 40 * sin(y * r), cx + (h + 20) * cos(ny * r), cy + (h + 20) * sin(ny * r), ny + 180 }')"
    log=$(tb_run_sh -Script "map vrclimb;wait30;god;notarget;developer 1;vr_climb 1;vr_climb_debug 1;setpos $sx $sy 24 0 $yaw 0;noclip;wait20;vr_test_spawn_dist 40;vr_test_spawn $kind;impulse 241;wait90;setpos $ax $ay 24 0 $ayaw 0;noclip;wait30;vr_mock_play $D/$play.txt;wait300;viewpos;vr_climb_debug 0;developer 0;toggleconsole;quit" \
      -Timeout 200 -Filter "climb:|ENGINE|Player pos" 2>&1 | grep "climb:\|ENGINE\|Player pos" | grep -v "hold search\|eases onto" | tr -d '\r')
    echo "$log" > "$D/ps_${o}_$s.log"
    z=$(echo "$log" | grep "Player pos" | tail -1 | sed 's/.*(\([-0-9.]*\) \([-0-9.]*\) \([-0-9.]*\)).*/\3/')
    held=$(echo "$log" | grep -c "hand holds")
    mantled=$(echo "$log" | grep -c "mantled onto")
    ok=0
    [ "$mantled" -gt 0 ] && awk -v z="$z" -v t="$top" 'BEGIN{exit !(z > t - 3 && z < t + 4)}' && ok=1
    echo "$o side $s: at $ax $ay yaw $ayaw, holds $held, mantled $mantled, z $z: $([ $ok = 1 ] && echo PASS || echo FAIL)"
    row="$row $([ $ok = 1 ] && echo ok || echo FAIL)"
    [ $ok = 1 ] || fail=1
  done
  table="$table$row
"
done
echo "== sides 0 1 2 3"
printf "%s" "$table"
exit $fail
}
source "$(dirname "$0")/../testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
