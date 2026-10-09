#!/bin/bash
# worldparts_test.sh <agent> -- the parts of the guns lying about (their magazine, ammo screen and button: vr_view.cpp
# setupWorldWeapons, setupMagazines; the author's note vrfiringrange_2026-10-09_12-35-45: they popped in and out a few
# metres off), headless, in vrfiringrange (its tables' 30-odd guns, six of them magazine guns): from the spawn and from
# 1500 units over the tables (noclip, setpos), how many show their screen and their magazine and the farthest that does
# (vr_reload_debug's once-a-second prints): at the defaults (vr_weapon_world_attach_range 2500, _max 32) all of them;
# at the old 320 units and 6 guns none from either place; with the range at 1000, none past it.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
# "<screens> <magazines> <farthest>" in the last second, from `$1` (a setpos, or none), with the settings `$2`.
count() {
    local at=""
    [ -n "$1" ] && at="noclip;setpos $1;"
    local log=$(bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait90;developer 1;vr_reload_debug 1;$2;${at}wait90;echo MARK;wait80;toggleconsole;quit" -Filter "^MARK|^world gun|^reload: a lying" 2>&1 | sed -n '/^MARK/,$p' | sort -u)
    local far=$(echo "$log" | grep "^world gun" | sed -n 's/.* \([0-9][0-9]*\) units off$/\1/p' | sort -n | tail -1)
    echo "$(echo "$log" | grep -c "^world gun") $(echo "$log" | grep -c "^reload: a lying") ${far:-0}"
}
for row in "spawn|" "above|0 -940 1500"; do
    name=${row%%|*}; at=${row#*|}
    set -- $(count "$at" "echo defaults"); s=$1; m=$2; f=$3
    set -- $(count "$at" "vr_weapon_world_attach_range 320;vr_weapon_world_attach_max 6"); os=$1; om=$2
    check $([ $s -ge 30 ] && [ $m -ge 6 ] && echo 1 || echo 0) "from the $name: $s screens and $m magazines (all the guns: 30 and 6 or more), the farthest $f units off"
    check $([ $os = 0 ] && [ $om = 0 ] && echo 1 || echo 0) "from the $name at the old 320 units and 6 guns: $os screens, $om magazines"
done
set -- $(count "0 -940 1500" "vr_weapon_world_attach_range 1000"); s=$1; f=$3
check $([ $s = 0 ] && echo 1 || echo 0) "the range at 1000, 1500 units over the tables: $s screens"
set -- $(count "" "vr_weapon_world_attach_range 400"); s=$1; f=$3
check $([ $f -le 400 ] && echo 1 || echo 0) "the range at 400, from the spawn: $s screens, the farthest $f units off"
exit $fail
