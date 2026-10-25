#!/bin/bash
# gun_wall_test.sh <agent> [scenario...] -- a weapon held each way walked into vrtesthall's plain north wall (y 640):
# by its handle (main, off), with the other hand on it anywhere, carried off its handle after the hand-off, taken off
# the floor anywhere on it, by a hotspot (a crowbar's blade, hotspot 1), and carried with both hands on it. After the
# walk, `vr_gun_wall_depth` prints each hand's drawn weapon: its points in the level ("in", deepest "deep") and how far
# the walls hold the hand out ("out"). PASS when no weapon goes more than 6 cm in (a weapon held by its handle has kept
# up to 5 cm at a sample point since round 21; carried ones went 13-31 cm in before 2026-10-20: ROUND22.md, "Weapons
# held off their handle collide with the level"). Carried weapons point back from the hand as taken here: their hand is
# turned round (yaw 180) before the walk, so that they point into the wall.
AGENT=$1; shift; KIT=${KIT:-C:/OHWorkspace/qvr-kit}; D=C:/OHWorkspace/qvr-agents/$AGENT/scratch/gunwall
mkdir -p $D
P="sv_random_seed 1;map vrtesthall;wait60;developer 1;vr_debug_2h_grip 1;vr_weapon_grip_mode 0;setpos 0 500 24 0 90 0;noclip;wait5;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;vr_mock_hand off -0.1 1.0 -0.3;wait10"
GRAB_MAIN="+grabright;vr_mock_button main grip 1;impulse 154;wait30"
OFF_ANY="vr_mock_hand_to off held 0.4;wait5;vr_mock_hand_to off held 0.4;wait5;+grableft;vr_mock_button off grip 1;wait30"
WALK="setpos 0 560 24 0 90 0;noclip;wait10;vr_gun_wall_depth;+forward;wait150;-forward;wait30;echo === after;vr_gun_wall_depth;toggleconsole;quit"
DROP="+grabright;vr_mock_button main grip 1;impulse 154;wait30;-grabright;vr_mock_button main grip 0;wait120"
TAKE_ANY="vr_mock_hand_to main weapon 0.5;wait5;vr_mock_hand_to main weapon 0.5;wait5;+grabright;vr_mock_button main grip 1;wait30"
TURN_MAIN="vr_mock_hand main 0.15 1.2 -0.45 70 180 0;wait30"
TURN_OFF="vr_mock_hand off -0.15 1.2 -0.45 70 180 0;wait30"
OFF_GUN="vr_mock_hand off -0.15 1.2 -0.45 70 0 0;wait5;+grableft;vr_mock_button off grip 1;impulse 174;wait30"
declare -A S
S[held]="$P;$GRAB_MAIN;$WALK"
S[held_anywhere]="$P;$GRAB_MAIN;$OFF_ANY;$WALK"
S[handoff_carried]="$P;$GRAB_MAIN;$OFF_ANY;-grabright;vr_mock_button main grip 0;wait30;$TURN_OFF;$WALK"
S[ground_anywhere]="$P;$DROP;$TAKE_ANY;$TURN_MAIN;$WALK"
S[ground_spot1]="$P;impulse 217;wait120;setpos 0 520 24 0 90 0;noclip;wait10;vr_mock_hand_to main spot 1 3;wait5;vr_mock_hand_to main spot 1 3;wait5;+grabright;vr_mock_button main grip 1;wait30;$TURN_MAIN;$WALK"
S[carried_both]="$P;$DROP;$TAKE_ANY;$TURN_MAIN;vr_mock_hand_to off held 0.6;wait5;vr_mock_hand_to off held 0.6;wait5;+grableft;vr_mock_button off grip 1;wait30;$WALK"
S[off_held]="$P;$OFF_GUN;$WALK"
S[off_held_main_anywhere]="$P;vr_mock_hand main 0.1 1.0 -0.3;$OFF_GUN;vr_mock_hand_to main held 0.4;wait5;vr_mock_hand_to main held 0.4;wait5;+grabright;vr_mock_button main grip 1;wait30;$WALK"
LIST=${*:-held held_anywhere handoff_carried ground_anywhere ground_spot1 carried_both off_held off_held_main_anywhere}
# The carries expected (the mock's take off the floor sometimes lands on the handle instead: tried again, three times).
declare -A WANT=([handoff_carried]=carried [ground_anywhere]=carried [ground_spot1]=carried [carried_both]=carried2h)
fail=0
for k in $LIST; do
  printf '%s\n' "${S[$k]}" > $D/$k.txt
  for try in 1 2 3; do
    out=$(bash $KIT/run.sh $AGENT -Script "$(cat $D/$k.txt)" -Filter "gunwall|===|2h grip: .*(carries|both hands)" 2>&1)
    after=$(echo "$out" | sed -n '/=== after/,$p' | grep "^gunwall")
    how=$(echo "$after" | grep -o "^gunwall: [a-z]* [a-z+0-9]*" | grep -v " held$" | head -1)
    [ -z "${WANT[$k]}" ] || echo "$how" | grep -q " ${WANT[$k]}$" && break
  done
  deep=$(echo "$after" | grep -o "deep [0-9.]*" | awk '{if($2>m)m=$2} END{print m+0}')
  if [ -n "${WANT[$k]}" ] && ! echo "$how" | grep -q " ${WANT[$k]}$"; then r="FAIL (not ${WANT[$k]})"; fail=1
  elif awk "BEGIN{exit !($deep <= 6)}"; then r=PASS; else r=FAIL; fail=1; fi
  echo "$r $k: deepest ${deep} cm (${how:-held by the handle})"
  echo "$after" | sed 's/^/    /'
done
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
