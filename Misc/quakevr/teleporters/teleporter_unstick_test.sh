#!/bin/bash
# teleporter_unstick_test.sh <agent>: headless checks of the safety net that gets a player out of the wall behind a
# teleporter (vr_portals_unstick; ROUND21.md, "Teleporters: shooting yourself, stuck behind a gate"), with the agent kit
# (C:/OHWorkspace/qvr-kit). Each line prints vr_portals_stuck after the player is put (setpos) in the flush player gate's
# wall (its plane y 640, the far gate's y 928), standing still a moment, and what it must say.
#   behind 20           his torso 20 past the plane: still a straddle (the split body holds within 24 of it): stays
#   behind 30, 44       his torso that far past the plane, his box wholly inside the wall: carried on through (y
#                       about 928 + it, stuck 0, unstuck 1)
#   in front, back out  his torso 4 in front of the plane, his box half in the wall (a straddle): stays (stuck 0,
#                       unstuck 0: the split body is not stuck)
#   back out 12, 20     his torso that far past the plane (not carried), walking back out: he goes back out into
#                       the room (y under 640, not carried, unstuck 0; he stood frozen in the wall before)
#   off                 vr_portals_unstick 0, 30 behind: Quake's own SV_CheckStuck puts him back where he last stood
#                       free (here the spawn, 0 -160, before setpos; in play the straddle he slid out of, so he stayed
#                       in the wall): not carried (a control)
#   rebound             the cooldown softlock: in through the gate, straight back out, in again at once, then back
#                       out of the exit: never stuck (stuck 0), carried each time
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
START="developer 1;vr_fixed_frames 1;vr_fixed_frames_rate 90;vr_mock_eye_size 160;map vrteleporters;wait60;god;notarget"
probe() { bash $KIT/run.sh $AGENT -Filter "portal stuck" -Script "$1;toggleconsole;quit" | grep "portal stuck" | sed 's/portal stuck: //' | tr '\n' '|'; }
put() { probe "$START;$2;setpos -256 $1 24 0 90 0;wait5;noclip 0;wait20;vr_portals_stuck"; }
echo "behind 20:  $(put 660) (stuck 0 at y 660, unstuck 0)"
echo "behind 30:  $(put 670) (stuck 0 at y ~958, unstuck 1)"
echo "behind 44:  $(put 684) (stuck 0 at y ~972, unstuck 1)"
echo "in front 4: $(put 636) (stuck 0 at y 636, unstuck 0)"
echo "off:        $(put 670 "vr_portals_unstick 0") (stuck 0 at 0 -160, unstuck 0: a control)"
back() { probe "$START;setpos -256 $1 24 0 90 0;wait5;noclip 0;wait5;vr_mock_stick off 0 -1;wait30;vr_mock_stick off 0 0;wait5;vr_portals_stuck"; }
echo "back out 12: $(back 652) (stuck 0 at y < 600, unstuck 0)"
echo "back out 20: $(back 660) (stuck 0 at y < 600, unstuck 0)"
S="$START;setpos -256 600 24 0 90 0;wait5;noclip 0;wait3;vr_mock_stick off 0 1;wait12;vr_mock_stick off 0 -1;wait10;vr_mock_stick off 0 1;wait40;vr_mock_stick off 0 0;wait10;vr_portals_stuck;vr_mock_stick off 0 -1;wait60;vr_mock_stick off 0 0;vr_portals_stuck"
echo "rebound:    $(probe "$S") (stuck 0 at y ~1100, then stuck 0 at y < 640; unstuck 0)"
