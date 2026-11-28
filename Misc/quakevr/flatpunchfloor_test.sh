#!/bin/bash
# flatpunchfloor_test.sh <agent> -- flat-screen play, both hands empty, crouched and aiming down: the jabs stop on the
# floor and strike it, as a punch at a wall does (his words 2026-11-28, item 7: "the player's hands go through the floor
# instead of hitting it"; ROUND26.md "flatmisc3"; vr_flatplay.cpp handOffWall, handpose::pointSurfaceDepth). Headless
# flat run on vrtesthall's open floor, fast mode, the probe on (vr_flatplay_debug 2: "flatwall:" lines, each frame a
# hand met a surface: how deep its pose had it, how far back it went, how deep its point is now):
#   down  crouched, looking 70 down: two jabs (main, off). Each pose had its fist in the floor (in > 1), each fist stops
#         on it (now at most 0.51: vr_flat_punch_wall_margin -0.5) and the world blow lands (a melee event on a wall);
#   level standing, looking ahead: two jabs in the air meet nothing (no flatwall line, no wall blow).
# Ends on its own last line (-QuitOn); prints "flatpunchfloor: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=flatpunchfloor
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatpunchfloor
mkdir -p "$S"
L="$S/run.log"
press="+attack; wait2; -attack; wait50"
SC="map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; impulse 150; impulse 170; wait5"
SC="$SC; setpos 0 300 25 70 90 0; noclip 0; +flatcrouch; wait40; vr_flatplay_debug 2; echo pf: down; $press; $press; vr_flatplay_debug 1; -flatcrouch; wait40"
SC="$SC; setpos 0 300 25 0 90 0; noclip 0; wait10; vr_flatplay_debug 2; echo pf: level; $press; $press; vr_flatplay_debug 1"
SC="$SC; echo pf: done; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Script "$SC" -QuitOn "^pf: done" -Timeout 180 \
    -Filter '^pf:|^flatwall:|^melee event|^flatplay: (main|off) punch|ERROR|CRASH' > "$L" 2>&1
block() { awk -v m="pf: $1" '$0==m || $0==m" " {on=1;next} /^pf:/{on=0} on' "$L"; }
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
field() { block "$1" | grep -a "^flatwall: $2 hand .* punch " | grep -ao " $3 [-0-9.]*" | awk '{ print $2 + 0 }' | sort -g | tail -1; }
check $(grep -aq "^pf: done" "$L" && echo 1 || echo 0) "the run reached its end"
jabs=$(block down | grep -a "^flatplay: .* punch (the left button's jab)" | awk '{ print $2 }' | tr '\n' ' ')
check $([ "$jabs" = "main off " ] && echo 1 || echo 0) "down: two jabs, main and off (${jabs:-none})"
for h in main off; do
  in=$(field down $h in); now=$(field down $h now)
  check $(ok "${in:-0} > 1 &&${now:-99} <= 0.51") "down: the $h jab's pose had its fist ${in:-?} units into the floor; it stops on it (deepest after ${now:-?}, at most 0.51)"
done
wb=$(block down | grep -aE "^melee event: punch.*(wall|world)" | head -1)
check $([ -n "$wb" ] && echo 1 || echo 0) "down: the jab's blow on the floor lands (${wb:-no world blow})"
lw=$(block level | grep -ac "^flatwall: .* punch ")
lb=$(block level | grep -acE "^melee event: punch.*(wall|world)")
check $(ok "$lw == 0 && $lb == 0") "level: jabs in the air meet nothing ($lw flatwall lines, $lb world blows)"
verdict
exit $fail
