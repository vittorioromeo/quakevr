#!/bin/bash
# flat_gate_aim_test.sh <agent>: flat-screen hip fire through a see-through teleporter (vrteleporters), headless with the
# agent kit (C:/OHWorkspace/qvr-kit), fast mode; each run quits itself. ROUND25.md, "Flat-screen hip fire through
# teleporters". vr_flat_xhair_probe's "gates" line: where the main gun's shot as sent lands in the world (its line on
# through the gates as QC's MOVE_PORTALS pellets go) against what the middle of the screen is on (its ray on through the
# gates), "apart" in units. Before the fix the guns aimed at the gate's face (the crosshair's trace stopped there) and the
# gate turned the hip shot off: 11.4 units apart 160 back from the gate, 35.1 from 24 back, ADS 2.0, the loop 223.
#   hip, hip near, ads, turn90 (the 90-degree gate): through a gate, apart under 1 unit
#   plain: no gate, apart under 0.5 (non-portal aiming as before)
#   loop: the real shotgun hip-fired at your own back seen through T's loop gate (god off): your health drops
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
S="developer 1;map vrteleporters;wait60;notarget;god"
run() { bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Filter "main gates shot|^health" -Timeout 90 -Script "$1" | grep -a "main gates shot\|^health"; }
PASS=0; FAIL=0
gate() { # gate <name> <setpos> <extra> <want gates (1: through a gate, 0: none)> <max apart>
    line=$(run "$S;setpos $2;impulse 154;wait40;$3;wait60;vr_flat_xhair_probe;toggleconsole;quit" | tail -1)
    if echo "$line" | awk -v g="$4" -v m="$5" '{ for (i = 1; i <= NF; i++) { if ($i == "shot") s = $(i+1); if ($i == "centre") c = $(i+1); if ($i == "apart") a = $(i+1) }
        exit !(((g && s >= 1 && c >= 1) || (!g && s == 0 && c == 0)) && a != "" && a + 0 < m) }'; then
        PASS=$((PASS+1)); echo "PASS $1: $line"
    else
        FAIL=$((FAIL+1)); echo "FAIL $1: $line"
    fi
}
gate hip "-256 440 24 0 90 0" "wait" 1 1
gate "hip near" "-256 576 24 0 90 0" "wait" 1 1
gate ads "-256 440 24 0 90 0" "vr_flat_aim 1" 1 1
gate turn90 "-1280 568 24 0 90 0" "wait" 1 1
gate plain "-256 440 24 0 270 0" "wait" 0 0.5
h=$(run "developer 1;map vrteleporters;wait60;notarget;setpos -1400 560 24 0 180 0;impulse 154;wait60;edict 1;+attack;wait3;-attack;wait60;edict 1;toggleconsole;quit" | grep -a "^health" | awk '{print $2}' | tr '\n' ' ')
if echo "$h" | awk '{ exit !($1 == 100 && $2 + 0 < 100) }'; then PASS=$((PASS+1)); echo "PASS loop: health $h"; else FAIL=$((FAIL+1)); echo "FAIL loop: health $h"; fi
echo "flat_gate_aim: $PASS passed, $FAIL failed"
