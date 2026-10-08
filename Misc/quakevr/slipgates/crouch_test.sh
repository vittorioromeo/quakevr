#!/bin/bash
# crouch_test.sh <agent>: headless checks of the crouched box (vr_crouch_hull; ROUND21.md, "Crouching: a smaller box";
# HULLS.md "Crouching") in vrslipgates' crouching room, with the agent kit (C:/OHWorkspace/qvr-kit). One game run.
# The mock headset's head at 1.7 m stands (eyes 55.5 units over the feet), at 1.3 m half crouches (42.9: the 44 box),
# at 1.0 m crouches (33.5: the 36 box). Each case prints its name, then `crouch:` (vr_crouch_status: box 0 standing).
#   tunnel_*   walk east into the 40-high tunnel: standing stops at its mouth (x 552), crouched comes out (x > 688);
#              with vr_crouch_hull 0 crouched stops too
#   gap_*      the 48-high gap: standing stops, half crouched passes
#   rise_*     crouched inside the tunnel, the head back up: the 36 box kept (standfits 0); walked out east: standing
#   gate_*     walk into the small teleporter (48x48) in the east wall: standing not carried, crouched carried (to the
#              south wall's, west of the tunnel); gate_rise: crouched halfway into it (setpos), the head up: the
#              tallest box the opening takes (44, standfits 0). (Not walked on: put straddling a gate by setpos, a
#              player falls through it uncarried, standing too: not the crouch's)
#   cover_*    `vr_crouch_test 12` (vr_crouch_test.qc): a grunt beyond the 32-high cover fires 12 bullets: crouched behind
#              it no damage, standing damage; crouched in the open damage (the aim lower: aim, units over his feet)
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
LOG=$KIT/bases/$AGENT/qbase/qconsole.log
STAND="vr_mock_hand head"; HALF="vr_mock_hand head 0 1.3 0"; LOW="vr_mock_hand head 0 1.0 0"
walk() { echo -n "echo CASE $1;$2;wait5;setpos $3;wait5;noclip 0;wait5;vr_mock_stick off 0 1;wait${4:-120};vr_mock_stick off 0 0;wait5;vr_crouch_status;"; }
S="developer 1;map vrslipgates;wait60;god;notarget;"
S+=$(walk tunnel_standing "$STAND" "480 -320 24 0 0 0")
S+=$(walk tunnel_crouched "$LOW" "480 -320 24 0 0 0")
S+=$(walk tunnel_crouched_off "vr_crouch_hull 0;$LOW" "480 -320 24 0 0 0")
S+="vr_crouch_hull 1;"
S+=$(walk gap_standing "$STAND" "480 -192 24 0 0 0")
S+=$(walk gap_half "$HALF" "480 -192 24 0 0 0")
S+="echo CASE rise_inside;$LOW;wait5;setpos 624 -320 24 0 0 0;wait5;noclip 0;wait5;$STAND;wait10;vr_crouch_status;"
S+="echo CASE rise_walked_out;vr_mock_stick off 0 1;wait90;vr_mock_stick off 0 0;wait10;vr_crouch_status;"
S+=$(walk gate_standing "$STAND" "1060 -304 24 0 0 0" 90)
S+=$(walk gate_crouched "$LOW" "1060 -304 24 0 0 0" 90)
S+="echo CASE gate_rise;$LOW;wait5;setpos 1100 -304 24 0 0 0;wait5;noclip 0;wait5;setpos 1149 -304 24 0 0 0;wait2;$STAND;wait5;vr_crouch_status;"
S+="god;"
S+="echo CASE cover_crouched;$LOW;wait5;setpos 760 120 24 0 0 0;wait10;vr_crouch_test 12;wait5;"
S+="echo CASE cover_standing;$STAND;wait5;setpos 760 120 24 0 0 0;wait10;vr_crouch_test 12;wait5;"
S+="echo CASE open_crouched;$LOW;wait5;setpos 900 168 24 0 0 0;wait10;vr_crouch_test 12;wait5;"
bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" > /dev/null
awk '/^CASE /{n=$2} /^crouch: /{print n": "$0} /^crouchtest: /{print n": "$0}
     /VR portal: carried edict 1 through side/{print n": "$0}
     /CRASH|ENGINE ERROR/{print}' "$LOG"
