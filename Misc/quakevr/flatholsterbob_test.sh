#!/bin/bash
# flatholsterbob_test.sh <agent> -- flat-screen play: the hip holsters' and the ammo pouch's sway with the walking legs,
# scaled across and up and down apart by vr_flat_holster_bob_h / _v (his words 2026-11-28, item 2; ROUND26.md
# "flatmisc3"). Headless flat run on vrtesthall, fast mode: four walks forward (0.8 s each), one per pair of values
# (1 1, 0 0, 1 0, 0 1), with vr_flat_holster_bob_report every 3 frames while walking (how far the legs carry each, across
# and up and down, world units). 1 1: they sway both ways; 0 0: still; 1 0: across only; 0 1: up and down only.
# Ends on its own last line (-QuitOn); prints "flatholsterbob: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=flatholsterbob
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatholsterbob
mkdir -p "$S"
L="$S/run.log"
reports() { local r=""; for i in 1 2 3 4 5 6 7 8 9 10 11 12; do r="$r; wait3; vr_flat_holster_bob_report"; done; echo "$r"; }
walk() { echo "tune vr_flat_holster_bob_h $1; tune vr_flat_holster_bob_v $2; setpos 0 100 25 0 90 0; noclip 0; wait10; echo hb: case $1 $2; +forward; wait20 $(reports); -forward; wait10"; }
SC="map vrtesthall; god; notarget; wait10"
SC="$SC; $(walk 1 1); $(walk 0 0); $(walk 1 0); $(walk 0 1); tune_reset vr_flat_holster_bob_h; tune_reset vr_flat_holster_bob_v"
SC="$SC; echo hb: done; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SC" -QuitOn "^hb: done" -Timeout 180 \
    -Filter '^hb:|^holsterbob:|ERROR|CRASH' > "$L" 2>&1
block() { awk -v m="hb: case $1" '$0==m || $0==m" " {on=1;next} /^hb:/{on=0} on' "$L"; }
# The most of one measure (lhip/rhip/pouch, h or v) over a case's reports.
most() { block "$1" | grep -a "^holsterbob:" | awk -v k="$2" -v c="$3" '{ for(i = 1; i < NF; i++) if($i == k) { v = (c == "h" ? $(i+2) : $(i+4)) + 0; if(v > m) m = v } } END { printf "%.3f", m + 0 }'; }
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
check $(grep -aq "^hb: done" "$L" && echo 1 || echo 0) "the run reached its end"
n=$(block "1 1" | grep -ac "^holsterbob: lhip")
spd=$(block "1 1" | grep -a "^holsterbob:" | tail -1 | grep -ao "speed [0-9]*")
check $(ok "$n >= 10") "1 1: $n reports while walking (${spd:-no speed})"
for c in "1 1" "0 0" "1 0" "0 1"; do
  set -- $c
  for part in lhip rhip pouch; do
    h=$(most "$c" $part h); v=$(most "$c" $part v)
    if [ "$1" = 1 ]; then hc="$h > 0.05"; else hc="$h <= 0.001"; fi
    if [ "$2" = 1 ]; then vc="$v > 0.02"; else vc="$v <= 0.001"; fi
    check $(ok "$hc && $vc") "bob_h $1 bob_v $2: $part sways across at most $h, up and down $v (want across $([ "$1" = 1 ] && echo '> 0.05' || echo 0), up and down $([ "$2" = 1 ] && echo '> 0.02' || echo 0))"
  done
done
verdict
exit $fail
