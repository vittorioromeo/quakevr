#!/bin/bash
# flatparry_test.sh <agent> -- flat-screen play: a melee weapon's two-handed grip and the right mouse button's parry guard
# (his notes 2026-11-10; FLATPLAY.md, "Parry guard"). Four headless runs in fast mode:
#   1. each melee weapon swung by a handle (axe, Mjolnir, the knight's sword, the crowbar, the super axe) in the main hand,
#      swung twice (the free hand may take it two-handed after a swing): the grip the free hand takes is hs1, the one lowest
#      on the handle (vr_flat_melee_grip), and never the blade; then QC's parry probe (impulse 249, a blow from straight
#      ahead) before the guard ("no parry": the weapon stands up the hand), the right mouse button held (+offhandattack):
#      the guard reached within vr_flat_parry_in_time (one frame's slack), the probe "PARRIES"; let go: the share back to
#      0 after vr_flat_parry_out_time. The chainsaw and an axe in the off hand only the guard.
#   2. the scene flat_parry (a sword grunt attacking, a knight's sword held) with the guard held: its first blow parried
#      ("parry outcome");
#   3, 4. a knight's overhead chop on the same sword: parried in the guard, landing without it ("parry: none").
# Every run ends on its own last line (-QuitOn); prints "flatparry: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatparry
mkdir -p "$S"
L="$S/run.log"
ST="vr_flatplay_status"
SCRIPT="map vrtesthall;wait60;god;notarget;developer 1;vr_flatplay_debug 1;vr_debug_2h_grip 1;setpos 0 300 25 0 90 0"
for w in "axe 152 main" "mjolnir 153 main" "sword 163 main" "crowbar 167 main" "superaxe 168 main" "saw 164 -" "offaxe 172 off"; do
  set -- $w
  [ "$3" = off ] && SCRIPT="$SCRIPT;impulse 150;wait5"
  SCRIPT="$SCRIPT;echo fpt: $1;impulse $2;wait40"
  if [ "$3" = main ]; then
    SCRIPT="$SCRIPT;vr_flat_punch main;wait70;vr_flat_punch main;wait70;$ST"
  fi
  # (The guard: 0.15 s is 11 frames; 30 is plenty. Let go: 0.2 s is 15 frames; 25.)
  SCRIPT="$SCRIPT;echo fpt: $1 rest;impulse 249;wait3;+offhandattack;wait30;echo fpt: $1 guard;impulse 249;wait3;-offhandattack;wait25;echo fpt: $1 let go;$ST;wait2"
done
SCRIPT="$SCRIPT;echo flatparry done;toggleconsole;quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SCRIPT" -QuitOn "^flatparry done" -Timeout 240 \
  -Filter '^fpt:|^flatparry|^flatparry:|parry probe|melee grip:|2h grip: .* took|parry guard:|ERROR|CRASH' > "$L" 2>&1
# The lines of one block: from its marker to the next.
block() { awk -v m="fpt: $1" '$0==m || $0==m" "{on=1;next} /^fpt:|^flatparry done/{on=0} on' "$L"; }
check $(grep -aq "^flatparry done" "$L" && echo 1 || echo 0) "run 1 reached its end"
for w in axe mjolnir sword crowbar superaxe; do
  g=$(block $w | grep -a "melee grip:")
  check $([ -n "$g" ] && ! grep -aqv "melee grip: hs1 " <<<"$g" && echo 1 || echo 0) "$w: the two-handed grip is hs1, the lowest on the handle ($(tail -1 <<<"$g" | sed 's/^ *//'))"
  check $(block $w | grep -aq "took the blade\|on the blade" && echo 0 || echo 1) "$w: the free hand never on the blade"
done
for w in axe mjolnir sword crowbar superaxe saw offaxe; do
  check $(block "$w rest" | grep -aq "parry probe.*no parry" && echo 1 || echo 0) "$w: no parry at rest"
  check $(block "$w guard" | grep -aq "parry probe.*PARRIES" && echo 1 || echo 0) "$w: the guard parries ($(block "$w guard" | grep -ao 'off level *[0-9.]*, off the attacker.s line *[0-9.]*' | head -1))"
  t=$(block "$w rest" | grep -ao 'in the guard, [0-9.]* s' | grep -ao '[0-9.]* s' | head -1)
  check $([ -n "$t" ] && awk -v t="${t% s}" 'BEGIN{exit !(t <= 0.15 + 1/72 + 0.001)}' && echo 1 || echo 0) "$w: the guard reached in ${t:-never} (vr_flat_parry_in_time 0.15)"
  check $(block "$w let go" | grep -aq "parry guard: .* not asked for, share 0.00" && echo 1 || echo 0) "$w: let go, back to the hold"
done

# 2..4: the first blow of the sword grunt (the scene flat_parry) in the guard, and of a knight (its overhead chop) in the
# guard and not. (The grunt's blow is not tried unguarded: a sideways swing meets the sword stood up in the hold too, and
# is parried by it, as a headset's would be.)
for c in grunt1 knight1 knight0; do
  L2="$S/$c.log"
  if [ "${c%?}" = grunt ]; then
    SC="developer 1;vr_parry_stamina 0;vr_stealth_meter 0;vr_scenario flat_parry;wait120"
  else
    SC="map vrtesthall;wait60;god;developer 1;vr_parry_stamina 0;vr_stealth_meter 0;setpos 0 300 25 0 90 0;impulse 163;wait30"
    SC="$SC;vr_test_spawn 5;vr_test_spawn_dist 60;impulse 241;wait5"
  fi
  [ "${c: -1}" = 1 ] && SC="$SC;+offhandattack"
  SC="$SC;wait2000;echo flatparry gave up;toggleconsole;quit"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SC" -QuitOn "^parry outcome|^parry: none|^flatparry gave up" -Timeout 180 \
    -Filter '^parry|^dirparry|^flatparry|ERROR|CRASH' > "$L2" 2>&1
  first=$(grep -aE "^parry outcome|^parry: none" "$L2" | head -1)
  if [ "${c: -1}" = 1 ]; then
    check $(grep -aq "^parry outcome" <<<"$first" && echo 1 || echo 0) "${c%?}: its first blow parried in the guard (${first:-no blow})"
  else
    check $(grep -aq "^parry: none" <<<"$first" && echo 1 || echo 0) "${c%?}: its first blow lands without it (${first:-no blow})"
  fi
done
verdict
