#!/bin/bash
# flatdebuglines_test.sh <agent> -- Show Melee Arcs and Show Aim Cones in flatscreen play (his report 2026-11-25: "I cannot
# see any debug visualizations when fighting armed humanoids"; ROUND26.md "flatmisc1125"). The view queues their lines in
# flatscreen play as in a headset's; the flat view's overlay (vr_ragtool.cpp VR_RagtoolOverlay) now draws them.
#   a. the scene agrunt_melee_arcs (a sword and an axe grunt at you, god), flat, both options on: after 450 frames (their walk
#      in and a few swings) the overlay drew the lines on most frames and an armed humanoid's swing on some.
#   b. the melee arcs tests in flatscreen play: vr_agrunt_test 216 (a sword grunt's swing sent and drawn) and 212 (each kind's rule).
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
log=$HERE/scratch/flatdebuglines_a.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "vr_scenario agrunt_melee_arcs; vr_enemy_aim_show 1; wait450; vr_flat_debug_lines_probe; toggleconsole; quit" -Filter "^flatdebuglines" -QuitOn "^flatdebuglines" -Timeout 120 > "$log" 2>&1
line=$(grep -a "^flatdebuglines" "$log" | tail -1 | tr -d '\r')
frames=$(awk '{ print $3 }' <<<"$line"); swing=$(awk '{ print $5 }' <<<"$line")
echo "  $line"
check $([ "${frames:-0}" -gt 300 ] 2>/dev/null && echo 1 || echo 0) "flat: the debug lines drawn over the view ($frames of 450 frames)"
check $([ "${swing:-0}" -gt 5 ] 2>/dev/null && echo 1 || echo 0) "flat: an armed humanoid's swing drawn ($swing frames)"
for t in 216 212; do
  out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "vr_agrunt_test $t" -Filter "passed, [0-9]+ failed|FAIL" -QuitOn "test $t over" -Timeout 300 2>&1)
  grep -a "FAIL" <<<"$out" | head -5 | sed 's/^/  /'
  expect_verdict "$out" "test $t over" "flat: vr_agrunt_test $t"
done
verdict; exit $fail
