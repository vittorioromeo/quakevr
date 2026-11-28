#!/bin/bash
# flatmelee6_test.sh <agent> -- flat-screen V with both hands empty: the two-handed overhead blow (vr_flat_overhand_both;
# flatmelee6, his words 2026-11-26; ROUND26.md "flatmelee6"). One headless flat run on vrtesthall, fast mode, the probe on
# (vr_flat_melee_probe 1: a "flatmelee:" line per hand as its blow ends) and the damage numbers (vr_debug_damage_numbers 1),
# a fresh grunt 16 ahead for each blow:
#   1. both hands empty, V: both fists' blow (the engine's "overhand blow, both fists"), both hands raised over the eye
#      together (a probe line for each, overlapping), the off fist's motion synthesized; one hit on the grunt;
#   2. vr_flat_overhand_both 0: today's one-fist overhand, the off hand still; one hit, about half the two-fisted one's
#      damage (vr_flat_overhand_2h_damage 2);
#   3. an axe in the off hand (impulse 172), the main hand empty: V swings the axe (flatfists, his item 9 2026-11-27).
# The run ends on "flatmelee: done" (-QuitOn); each blow is given 60 frames (the overhand's 0.64 s is 46).
AGENT=${1:?agent}
EXTRA=${2:-wait1} # (optional: console commands said before the blows, e.g. "tune vr_flat_overhand_2h_end_out 0")
fail=0
QT_NAME=flatmelee6
source "$(dirname "$0")/qvrtest.sh"
log=$TREE/scratch/flatmelee6_test.log
grunt="vr_test_spawn 0; vr_test_spawn_dist 16; impulse 241; wait5"
S="map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; vr_handsource_debug 1; vr_flat_melee_probe 1"
S="$S; vr_debug_damage_numbers 1; vr_flat_overhand_both 1; $EXTRA; wait5; setpos 0 260 25 0 90 0; wait5"
S="$S; $grunt; echo flatmelee6: case 1; vr_flat_punch v; wait60; vr_test_kill 1; wait5"
S="$S; vr_flat_overhand_both 0; $grunt; echo flatmelee6: case 2; vr_flat_punch v; wait60; vr_test_kill 1; wait5; vr_flat_overhand_both 1"
S="$S; impulse 172; wait30; $grunt; echo flatmelee6: case 3; vr_flat_punch v; wait60; vr_test_kill 1; wait5"
S="$S; vr_flat_melee_probe done; wait2; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Script "$S" -QuitOn "flatmelee: done" -Timeout 200 \
    -Filter "^flatmelee|^flatplay: (main|off) punch|^Damage:|^handsource: (main|off) hand|^melee event:" > "$log" 2>&1
# `part <n>`: the log's lines of case n (from its echo to the next one's).
part() { awk -v n="$1" '/^flatmelee6: case /{ c = $NF + 0; next } c == n' "$log"; }
for n in 1 2 3; do echo "  case $n:"; part $n | grep -a "^flatmelee: \|^flatplay: \|^Damage:" | sed 's/^/    /'; done
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
has() { part "$1" | grep -aq "$2" && echo 1 || echo 0; }
count() { part "$1" | grep -ac "$2"; }
dmg() { part "$1" | grep -a "^Damage:.* melee:" | head -1 | awk '{ print $2 + 0 }'; }
eye() { part "$1" | grep -a "^flatmelee: $2 " | head -1 | grep -ao " eye [-0-9.]*" | awk '{ print $2 + 0 }'; }
d1=$(dmg 1); d2=$(dmg 2); e1m=$(eye 1 main); e1o=$(eye 1 off)
check $(has 1 "^flatplay: main punch .*overhand blow, both fists") "both empty: V is the two-fisted overhand blow"
check $(part 1 | grep -aq "^flatmelee: main overhand both.* both 1" && part 1 | grep -aq "^flatmelee: off overhand both.* both 1" && echo 1 || echo 0) "both empty: both hands animate, together"
check $(ok "${e1m:--99} > 0 && ${e1o:--99} > 0") "both empty: both fists go over the eye (main ${e1m:-?}, off ${e1o:-?} units)"
check $(has 1 "^handsource: off hand synthesized (two-fisted blow's other fist)") "both empty: the off fist's motion strikes nothing (synthesized)"
check $(ok "$(count 1 '^Damage:.* melee:') == 1") "both empty: one combined hit ($(count 1 '^Damage:.* melee:') damage lines, ${d1:-?})"
check $(ok "$(count 2 'overhand blow, both fists') == 0 && $(count 2 '^flatplay: main punch .*overhand blow') == 1 && $(count 2 '^flatmelee: off ') == 0") "option off: today's one-fist overhand, the off hand still"
check $(ok "$(count 2 '^Damage:.* melee:') == 1") "option off: one hit (${d2:-?})"
check $(ok "${d1:-0} > 1.4 * ${d2:-99} && ${d1:-0} < 2.8 * ${d2:-0}") "the two-fisted hit about twice one fist's (${d1:-?} vs ${d2:-?}; vr_flat_overhand_2h_damage 2)"
check $(ok "$(count 3 'overhand blow') == 0 && $(count 3 '^flatplay: off punch .*overhead swing') == 1 && $(count 3 '^flatplay: main punch') == 0") "an axe in the off hand alone: V swings it (flatfists, his item 9 2026-11-27), no fist's overhand"
verdict
exit $fail
