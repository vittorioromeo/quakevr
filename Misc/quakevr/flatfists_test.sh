#!/bin/bash
# flatfists_test.sh <agent> -- flat-screen play with both hands empty: the left button's jabs, each hand in turn, and the
# right button's crossed-arms guard (flatfists, his words 2026-11-27; ROUND26.md "flatfists"); V with one weapon held, in
# either hand, is that weapon's quick melee by its own hand (his item 9). Headless flat runs on vrtesthall, fast mode:
#   1. both fists (impulse 150 / 170): three presses of the left button jab main, off, main; held 100 frames, more jabs,
#      still alternating; two presses at a grunt 16 ahead land a melee blow each; the right button held 20 frames: the
#      guard up (share 1, the main fist across on the left of the off fist, the wrists close), let go: down (share 0);
#      V with a shotgun in the off hand alone: the off hand's melee with it, a hit on a grunt; a shotgun in the main hand
#      alone: the main hand's; a shotgun in each: the main hand's;
#   2. a knight 60 ahead, the guard up: its first blow parried by the crossed arms, the damage cut by
#      vr_flat_fist_guard_reduction (0.45);
#   3. the knight behind him, the guard up: not in front (the cone), its blow lands;
#   4. the knight ahead, no guard: its blow lands.
# Each run ends on its own last line (-QuitOn); prints "flatfists: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=flatfists
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatfists
mkdir -p "$S"
L="$S/run.log"
grunt="vr_test_spawn 0; vr_test_spawn_dist 16; impulse 241; wait5"
press="+attack; wait2; -attack; wait40"
SC="map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; vr_debug_damage_numbers 1; vr_flat_melee_probe 1"
SC="$SC; impulse 150; impulse 170; wait5; setpos 0 260 25 0 90 0; wait5"
SC="$SC; echo ff: presses; $press; $press; $press; wait20"
SC="$SC; echo ff: held; +attack; wait100; -attack; wait50"
SC="$SC; echo ff: jabhit; $grunt; $press; vr_test_kill 1; wait5; $grunt; $press; wait30; vr_test_kill 1; wait5"
SC="$SC; echo ff: guard; +offhandattack; wait20; vr_flatplay_status; -offhandattack; wait20; echo ff: letgo; vr_flatplay_status"
SC="$SC; impulse 174; wait30; $grunt; echo ff: offgun; vr_flat_punch v; wait60; vr_test_kill 1; wait5"
SC="$SC; impulse 170; impulse 154; wait30; $grunt; echo ff: maingun; vr_flat_punch v; wait60; vr_test_kill 1; wait5"
SC="$SC; impulse 174; wait30; $grunt; echo ff: dual; vr_flat_punch v; wait60; vr_test_kill 1; wait5"
SC="$SC; echo ff: done; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Script "$SC" -QuitOn "^ff: done" -Timeout 240 \
    -Filter '^ff:|^flatplay: (main|off) punch|^flatfists|^  fist|^Damage:|ERROR|CRASH' > "$L" 2>&1
# The lines of one block: from its marker to the next.
block() { awk -v m="ff: $1" '$0==m || $0==m" "{on=1;next} /^ff:/{on=0} on' "$L"; }
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
hands() { block "$1" | grep -a "^flatplay: .* punch (the left button" | awk '{ print $2 }' | tr '\n' ' '; }
alternates() { awk -v s="$1" 'BEGIN { n = split(s, a, " "); if(n < 2) exit 1; for(i = 2; i <= n; i++) if(a[i] == a[i-1]) exit 1; exit 0 }' && echo 1 || echo 0; }
check $(grep -aq "^ff: done" "$L" && echo 1 || echo 0) "run 1 reached its end"
p=$(hands presses)
check $([ "$p" = "main off main " ] && echo 1 || echo 0) "three presses: main, off, main (${p:-none})"
hl=$(hands held); hn=$(wc -w <<<"$hl")
check $(ok "$hn >= 3") "held 100 frames: $hn jabs (vr_flat_jab_repeat 0.4: 3-4)"
check $(alternates "$hl") "held: the jabs alternate ($hl)"
hits=$(block jabhit | grep -ac "^Damage:.* melee:")
jh=$(hands jabhit)
check $(ok "$hits >= 2") "two jabs at a grunt: $hits melee hits ($jh)"
gl=$(block guard | grep -a "^  fist" | tr '\n' ' ')
share=$(grep -ao "share [0-9.]*" <<<"$gl" | awk '{ print $2 + 0 }')
right=$(grep -ao "right [-0-9.]*" <<<"$gl" | awk '{ print $2 + 0 }')
gap=$(grep -ao "gap [0-9.]*" <<<"$gl" | awk '{ print $2 + 0 }')
check $(ok "${share:-0} >= 0.99") "the right button held: the crossed-arms guard up (share ${share:-?})"
check $(ok "${right:-9} < -1 && ${gap:-99} < 16") "the guard's arms crossed: the main fist ${right:-?} right of the off fist, ${gap:-?} apart (QC VR_Parry_ArmsCrossed: < -1, within 16)"
check $(block guard | grep -aq "^flatfists: crossed-arms guard up" && echo 1 || echo 0) "the guard reached ($(block guard | grep -a -m1 '^flatfists: crossed-arms guard up' | sed 's/^flatfists: //'))"
ls=$(block letgo | grep -a "^  fists:" | grep -ao "share [0-9.]*" | awk '{ print $2 + 0 }')
check $(ok "${ls:-1} <= 0.01") "let go: the guard down (share ${ls:-?})"
check $(block offgun | grep -aq "^flatplay: off punch (.*V.*):.*overhead swing" && echo 1 || echo 0) "V, a shotgun in the off hand alone: the off hand's swing with it ($(block offgun | grep -a -m1 '^flatplay: .* punch' || echo none))"
check $(ok "$(block offgun | grep -ac '^Damage:.* melee:') >= 1") "V, the off hand's shotgun: a melee hit on the grunt"
check $(block maingun | grep -aq "^flatplay: main punch (.*V.*):.*overhead swing" && echo 1 || echo 0) "V, a shotgun in the main hand alone: the main hand's swing with it ($(block maingun | grep -a -m1 '^flatplay: .* punch' || echo none))"
check $(block dual | grep -aq "^flatplay: main punch (.*V.*):.*overhead swing" && echo 1 || echo 0) "V, a shotgun in each hand: the main hand's swing ($(block dual | grep -a -m1 '^flatplay: .* punch' || echo none))"
# 2..4: a knight's blow on the crossed-arms guard (QC VR_Parry_FlatFists): in front parried, its damage cut; behind not.
for c in front behind noguard; do
  L2="$S/$c.log"
  K="map vrtesthall; wait60; god; developer 1; vr_parry_stamina 0; vr_stealth_meter 0; setpos 0 300 25 0 90 0"
  K="$K; impulse 150; impulse 170; wait5; vr_test_spawn 5; vr_test_spawn_dist 60; impulse 241; wait5"
  [ "$c" = behind ] && K="$K; setpos 0 300 25 0 270 0; wait2"
  [ "$c" != noguard ] && K="$K; +offhandattack"
  K="$K; wait3000; echo flatfists gave up; toggleconsole; quit"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$K" -QuitOn "^flatfists: parry|^parry: none|^flatfists gave up" \
      -Timeout 200 -Filter '^parry|^flatfists|ERROR|CRASH' > "$L2" 2>&1
  first=$(grep -aE "^flatfists: parry|^parry: none|^flatfists gave up" "$L2" | head -1)
  if [ "$c" = front ]; then
    a=$(grep -ao "damage [0-9.]* -> [0-9.]*" <<<"$first" | awk '{ print $2 + 0 }')
    b=$(grep -ao "damage [0-9.]* -> [0-9.]*" <<<"$first" | awk '{ print $4 + 0 }')
    check $(grep -aq "^flatfists: parry 1 " <<<"$first" && echo 1 || echo 0) "knight in front, the guard up: its first blow parried by the crossed arms (${first:-no blow})"
    check $(ok "${a:-0} > 0 && ${b:--1} > 0.5 * ${a:-0} && ${b:-99} < 0.6 * ${a:-0}") "its damage cut by vr_flat_fist_guard_reduction 0.45 (${a:-?} -> ${b:-?})"
  elif [ "$c" = behind ]; then
    check $(grep -aq "^parry: none" <<<"$first" && grep -aq "flat fists: .* not in front" "$L2" && echo 1 || echo 0) "knight behind, the guard up: not in front, its blow lands (${first:-no blow})"
  else
    check $(grep -aq "^parry: none" <<<"$first" && echo 1 || echo 0) "knight in front, no guard: its blow lands (${first:-no blow})"
  fi
done
verdict
exit $fail
