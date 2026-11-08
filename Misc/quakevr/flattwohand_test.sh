#!/bin/bash
# flattwohand_test.sh <agent> -- flat-screen play, AD mounted: the two-handed weapons held in both hands (ROUND24.md,
# "Flat two-handed holds"). One headless run in fast mode; each weapon given into the main hand, then vr_flatplay_status:
#   widow, plasma, adv2   the Widowmaker, the Plasma Gun, the plasma soldier's rifle: a sight line learnt from the mesh
#                         (vr_flat_sight_learn), the stance on and the free hand going on the hotspot
#   xbow                  the AD crossbow: the free hand holding its grip; its grips still (hs1_still, hs2_still 1)
#   saw                   the chainsaw: running by itself (vr_flat_saw_autostart), the free hand holding its front handle
#                         (vr_flat_melee_two_hand)
# Ends on its own last line (-QuitOn); prints "flattwohand: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flattwohand
mkdir -p "$S"
L="$S/run.log"
ST="vr_flatplay_status"
SCRIPT="developer 1;map vrtesthall;wait30;god;notarget;setpos 0 300 25 0 90 0;wait5"
SCRIPT="$SCRIPT;echo ftw: saw;impulse 164;wait90;impulse 228;wait2;$ST"
SCRIPT="$SCRIPT;echo ftw: widow;vr_ad_test 3;wait90;$ST"
SCRIPT="$SCRIPT;echo ftw: plasma;vr_ad_test 4;wait90;$ST"
SCRIPT="$SCRIPT;echo ftw: adv2;vr_ad_test 75;wait90;$ST"
SCRIPT="$SCRIPT;echo ftw: xbow;vr_ad_test 184;wait90;$ST;vr_wofs_hs1_still_33;vr_wofs_hs2_still_33"

SCRIPT="$SCRIPT;echo flattwohand done;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Games '-game ad -game quakevr' -ExtraArgs '-nomapindex -noaddons -flatscreenmode' -Script "$SCRIPT" \
  -QuitOn "^flattwohand done" -Timeout 240 -Filter '^ftw:|^flattwohand|two hands:|^  stance:|sight line: learnt|chainsaw: main hand|hs[12]_still|ERROR|CRASH' > "$L" 2>&1
# The lines of one weapon's block: from its marker to the next.
block() { awk -v m="ftw: $1" 'index($0,m)==1{on=1;next} /^ftw:|^flattwohand done/{on=0} on' "$L"; }
check $(grep -aq "^flattwohand done" "$L" && echo 1 || echo 0) "the run reached its end"
for w in widow plasma adv2; do
  check $(block $w | grep -aq "stance: on.*free hand going on it, share 1.00" && echo 1 || echo 0) "$w: the stance on, the free hand on its hotspot"
done
check $(grep -aq "sight line: learnt for progs/v_shot3.mdl#vr" "$L" && grep -aq "sight line: learnt for progs/v_plasma.mdl#vr" "$L" && echo 1 || echo 0) "widow, plasma: their sight lines learnt"
check $(block xbow | grep -aq "the off hand on its grip hotspot (held" && echo 1 || echo 0) "xbow: the free hand holding its grip"
check $(block xbow | grep -aEc 'hs[12]_still.* is "1"' | grep -q 2 && echo 1 || echo 0) "xbow: both grips still"
check $(block saw | grep -aq "running 1" && echo 1 || echo 0) "saw: running by itself"
check $(block saw | grep -aq "the off hand on its grip hotspot (held" && echo 1 || echo 0) "saw: the free hand holding its front handle"
verdict
