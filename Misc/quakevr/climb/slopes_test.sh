#!/bin/bash
# slopes_test.sh <agent> ["<cvars>"] (ROUND21.md, "Climbing: mantling onto sloping tops"): the mantle play on each of
# vrslopes' tops; prints the run's lines and, with no cvars (the defaults: vr_climb_mantle_lenient 1, slope 40), a PASS or
# FAIL a top against ROUND21.md's table (mantled: level, up10, up20, up30, down20, across15; no room: up45, up20slab).
# The mantle play: climb_plays.py's mantle with the hands at 1.49 m (its default 1.638 no longer reaches the lip with the
# current default body), written to the agent's scratch (P=<file> to use another).
AGENT=${1:?agent}; CVARS=${2:-}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
W=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$W/scratch/climb"
P=${P:-$W/scratch/climb/mantle_1.49.txt}
[ -f "$P" ] || python -c "import sys; sys.path.insert(0, sys.argv[1]); import climb_plays as c; L, t = c.mantle(1.49); c.write(sys.argv[2], L)" "$HERE" "$P"
S="vr_climb 1;map vrslopes;wait30;god;$CVARS;vr_climb_debug 1"
i=0
for name in level up10 up20 up30 up45 down20 across15 up20slab; do
  x=$((i*160+48)); i=$((i+1))
  S="$S;echo == $name;setpos $x -18 24 0 90 0;noclip;wait40;vr_mock_play $P;wait300;vr_mock_play"
done
log=$(bash $KIT/run.sh $AGENT -Script "$S;toggleconsole;quit" -Timeout 300 -Filter "ENGINE|rror|^==|mantle|no room|holds at" 2>&1 | grep -v grunt | cut -c1-150)
echo "$log"
[ -n "$CVARS" ] && exit 0
fail=0
for want in "level mantled" "up10 mantled" "up20 mantled" "up30 mantled" "up45 noroom" "down20 mantled" "across15 mantled" "up20slab noroom"; do
  set -- $want
  # the top's lines: from its "== name" to the next "=="
  part=$(echo "$log" | awk -v n="$1" '$1 == "==" {f = ($2 == n); next} f')
  got=noroom
  echo "$part" | grep -q "mantled onto" && got=mantled
  if [ $got = $2 ]; then echo "PASS $1: $2"; else echo "FAIL $1: want $2, got $got"; fail=1; fi
done
exit $fail
