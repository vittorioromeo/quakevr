#!/bin/bash
# flatinput_test.sh <agent> -- flat-screen play: hand motion the game makes up (the mouse look, the wall tilt) triggers no
# gesture; the buttons still act (ROUND24.md, "Hand motion source"; Quake/vr/vr_handsource.hpp). Headless, fast mode;
# each run ends on its own last line (-QuitOn), and the script prints "flatinput: N passed, M failed".
#   flickaxe   an axe in the main hand, a grunt right ahead: vr_flat_flick_test 144 40 (2 s of 2880 deg/s flicks; with vr_handsource_gate 0 they chop: a blow, swishes) -- no blow
#   flickrv    the revolver held (its flicks open it in the headset): no swing out, no eject, no blow
#   flickssg   the super shotgun held (its flick and pry break it open in the headset): no flick reload, no pry
#   wall       the axe, walked into the wall and back out (the wall tilt in and out): no blow
#   punch      V's punch (vr_flat_punch main) at the grunt: a blow lands (the button's motion is an action)
#   shove      the shove (vr_flat_shove_now, mouse 5's) at the grunt, both hands empty: it lands with both hands (impulse 55,
#              vr_flat_shove_both); again at once: the cooldown, nothing; again after it: it lands
#   shove1     an axe in the main hand: the off hand alone shoves (impulse 53), and lands
#   shovereach a grunt 55 units ahead: Shove Reach 1 (the old reach) misses it, 1.15 (the default, his 15% more) reaches it
#   vr         the mock headset: the hands are tracked (no synthesized mark; a headset's gestures untouched)
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatinput
mkdir -p "$S"
FILTER="^(flatflick|flatinput|melee|flick reload|ssg:|revolver:|flatplay: shove|handsource)"
run() { local tag=$1 extra=$2 script=$3 quit=$4
        bash $KIT/run.sh $AGENT $extra -Script "$script" -QuitOn "$quit" -Timeout 120 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
# A grunt 16 units ahead, facing you (vr_test_spawn 0: QC vr_impulse_test.qc, impulse 241), on vrtesthall's open floor.
GRUNT="vr_test_spawn 0; vr_test_spawn_dist 16; impulse 241; wait5"
BASE="map vrtesthall; god; notarget; developer 1; vr_reload_debug 1; vr_flatplay_debug 1; wait5; setpos 0 300 25 0 90 0; wait5"
FLAT="-ExtraArgs -flatscreenmode"
blows() { grep -aE "^melee (event|swish: blow|sound)" "$S/$1.log" | grep -av "stance" | wc -l; }

run flickaxe "$FLAT" "$BASE; impulse 152; wait10; $GRUNT; vr_handsource_debug 1; vr_flat_flick_test 144 40" "^flatflick: done"
check $(grep -aq "^flatflick: done" "$S/flickaxe.log" && echo 1 || echo 0) "flickaxe: the flicks ran to their end"
check $([ "$(blows flickaxe)" = 0 ] && echo 1 || echo 0) "flickaxe: no melee event from 2 s of hard flicks with an axe at a grunt ($(blows flickaxe))"
check $(grep -aq "^flatflick: done.*hands now synthesized, synthesized" "$S/flickaxe.log" && echo 1 || echo 0) "flickaxe: the hands' motion is marked synthesized"

# The control: the same flicks with the gate off (vr_handsource_gate 0, the old way) chop (the test can see a blow).
run flickaxe0 "$FLAT" "$BASE; vr_handsource_gate 0; impulse 152; wait10; $GRUNT; vr_flat_flick_test 144 40" "^flatflick: done"
check $([ "$(blows flickaxe0)" -ge 1 ] && echo 1 || echo 0) "flickaxe0: with vr_handsource_gate 0 the same flicks do strike ($(blows flickaxe0) melee lines): the test sees the bug"

run flickrv "$FLAT" "$BASE; impulse 181; wait10; $GRUNT; vr_flat_flick_test 144 40" "^flatflick: done"
check $(grep -aq "^flatflick: done" "$S/flickrv.log" && echo 1 || echo 0) "flickrv: the flicks ran to their end"
check $([ "$(grep -aEc "swung out|swings out|eject|^revolver:.*(open|out)" "$S/flickrv.log")" = 0 ] && [ "$(blows flickrv)" = 0 ] && echo 1 || echo 0) "flickrv: no swing out, no eject, no blow"

run flickssg "$FLAT" "$BASE; impulse 155; wait10; vr_flat_flick_test 144 40" "^flatflick: done"
check $(grep -aq "^flatflick: done" "$S/flickssg.log" && echo 1 || echo 0) "flickssg: the flicks ran to their end"
check $([ "$(grep -aEc "^flick reload|^ssg: (pried|the barrels)|broken open" "$S/flickssg.log")" = 0 ] && [ "$(blows flickssg)" = 0 ] && echo 1 || echo 0) "flickssg: no flick reload, no pry, no blow"

# The wall tilt: vrtesthall's wall at y 640 (flatwall_test.sh), walked into from 80 units off and back (1 s each way).
run wall "$FLAT" "map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; impulse 152; wait10; setpos 0 560 25 0 90 0; wait20; +forward; wait72; -forward; +back; wait72; -back; wait10; echo flatinput: wall done" "^flatinput: wall done"
check $(grep -aq "^flatinput: wall done" "$S/wall.log" && [ "$(blows wall)" = 0 ] && echo 1 || echo 0) "wall: the axe walked into the wall's tilt and out: no blow ($(blows wall))"

# V's punch: the punch takes 0.6 s (vr_flatplay.cpp punchTotal), 50 frames; 60 counted.
run punch "$FLAT" "$BASE; $GRUNT; vr_handsource_debug 1; vr_flat_punch main; wait60; echo flatinput: punch done" "^flatinput: punch done"
check $([ "$(grep -a "^melee event:" "$S/punch.log" | grep -ac "m/s")" -ge 1 ] && echo 1 || echo 0) "punch: V's punch lands a blow on the grunt"
check $(grep -aq "^handsource: main hand action (punch)" "$S/punch.log" && echo 1 || echo 0) "punch: the punch is marked an action"

# The shove: lands as the hand reaches out (a third of vr_flat_shove_time 0.4 s); again at once: the cooldown (0.8 s);
# again after it (60 frames: 0.83 s), at a second grunt 96 units to the left (he steps there: the first, thrown back about
# 20 units, stays at the edge of the shove's reach, and whether it counts too varied with the hands' rest pose): it lands
# on that one alone. Each wait is the shove's own timing.
run shove "$FLAT" "$BASE; $GRUNT; vr_flat_shove_now; wait20; vr_flat_shove_now; wait60; setpos -96 300 25 0 90 0; wait5; $GRUNT; vr_flat_shove_now; wait20; echo flatinput: shove done" "^flatinput: shove done"
check $([ "$(grep -ac "^melee event: shove, hand 1, two hands, 1 hit" "$S/shove.log")" = 2 ] && echo 1 || echo 0) "shove: two two-handed shoves land on the grunt ($(grep -ac "^melee event: shove" "$S/shove.log"))"
run shove1 "$FLAT" "$BASE; impulse 152; wait10; $GRUNT; vr_flat_shove_now; wait20; echo flatinput: shove1 done" "^flatinput: shove1 done"
check $(grep -aq "^melee event: shove, hand 0, one hand, 1 hit" "$S/shove1.log" && grep -aq "^flatplay: shove (vr_flat_shove_now): off hand" "$S/shove1.log" && echo 1 || echo 0) "shove1: an axe in the main hand, the off hand alone shoves and lands"
FAR="vr_test_spawn 0; vr_test_spawn_dist 55; impulse 241; wait5"
run shovereach "$FLAT" "$BASE; $FAR; vr_flat_shove_reach 1; vr_flat_shove_width 1; vr_flat_shove_now; wait60; vr_flat_shove_reach 1.15; vr_flat_shove_width 1.15; vr_flat_shove_now; wait20; echo flatinput: shovereach done" "^flatinput: shovereach done"
check $(grep -a "^melee event: \(flat \)\?shove" "$S/shovereach.log" | head -2 | tr '\n' '|' | grep -q "nothing in reach.*two hands, 1 hit" && echo 1 || echo 0) "shovereach: a grunt 55 ahead: reach 1 misses, 1.15 lands ($(grep -a "^melee event: \(flat \)\?shove" "$S/shovereach.log" | head -2 | tr '\n' '|'))"
check $(grep -aq "^flatplay: shove (vr_flat_shove_now): cooling down" "$S/shove.log" && echo 1 || echo 0) "shove: the press within the cooldown does nothing"

run vr "" "$BASE; impulse 152; wait10; vr_handsource_debug 1; wait5; echo flatinput: vr done" "^flatinput: vr done"
check $(grep -aq "^flatinput: vr done" "$S/vr.log" && ! grep -aq "^handsource:.*synthesized" "$S/vr.log" && echo 1 || echo 0) "vr: the mock headset's hands are never synthesized"

verdict; exit $fail
