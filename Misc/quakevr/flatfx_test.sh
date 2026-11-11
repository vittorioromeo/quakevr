#!/bin/bash
# flatfx_test.sh <agent> -- his flat-screen notes of 2026-11-11 (docs/vr-port/ROUND24.md, "Flat-screen: the super
# shotgun's wisps, the revolver's shells, the centurion's plasma gun"). Headless, fast mode; each run ends on its own last
# line (-QuitOn); prints "flatfx: N passed, M failed". The revolver's shells: flateject_test.sh (rv, vr).
#   ssg        flat: the super shotgun fired and broken open: its muzzle wisps come out of its barrels' ends as drawn
#              open (vr_debug_gunfx 2: each bore's end against the barrels' own part, 0 off), not where they were shut
#   centurion  flat: AD's centurion's plasma gun (vr_test_give_ad 1: without AD's data) in the main hand: its sight line
#              (vr_sightalign_table.inc, clear of its drum), the stance with the free hand on its hotspot, and the
#              ironsight lining its sights up (flatsightcheck: 0 off); the AD plasma rifle the same, as a control
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
QT_NAME=flatfx
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatfx
mkdir -p "$S"
FLAT="-ExtraArgs -flatscreenmode"
BASE="vr_flat_holsters 0; map vrtesthall; god; notarget; developer 1; wait5; setpos 0 300 25 0 90 0; wait5"

# ssg: one pull fires both barrels; it breaks open 0.15 s later and is drawn open 0.2 s after: 60 frames see it open.
bash $KIT/run.sh $AGENT $FLAT -Script "$BASE; vr_debug_gunfx 2; vr_flat_autoreload 0; vr_test_give_wid 5; impulse 149; give s 50; wait30; +attack; wait3; -attack; wait60; vr_debug_gunfx 0; echo flatfx_test: ssg done; toggleconsole; quit" \
  -QuitOn "^flatfx_test: ssg done" -Timeout 120 -Filter "^(gunfx ssg open|flatfx_test)" > "$S/ssg.log" 2>&1
L=$S/ssg.log
check $(grep -aq "^flatfx_test: ssg done" "$L" && echo 1 || echo 0) "ssg: the run reached its end"
n=$(grep -ac "gunfx ssg open hand [01] 45.0 deg" "$L")
moved=$(grep -a "gunfx ssg open hand [01] 45.0 deg" "$L" | tail -1 | awk '{ print $10 }')
worst=$(grep -a "gunfx ssg open" "$L" | awk '{ v = $17 + 0; if (v > m) m = v } END { print m + 0 }')
check $([ "${n:-0}" -ge 20 ] && echo 1 || echo 0) "ssg: drawn fully open (45 deg) for $n frames"
check $(awk -v m="${moved:-0}" 'BEGIN { exit !(m > 5) }' && echo 1 || echo 0) "ssg: its muzzle (wisps, heat) moved down with the barrels ($moved units from where it was shut)"
check $(awk -v w="$worst" 'BEGIN { exit !(w < 0.01) }' && echo 1 || echo 0) "ssg: each bore's end on the barrels as drawn open (worst $worst units off)"

# centurion: each gun given (vr_test_give_ad 1), hipfire then sighted (vr_flat_aim 1).
G=""
for w in 22 25; do
  G="$G echo flatfx_test: wid $w; vr_test_give_wid $w; impulse 149; wait90; vr_flatplay_status; vr_flat_aim 1; wait60; echo flatfx_test: wid $w ads; vr_flat_sight_check; screenshot; vr_flat_aim 0; wait30;"
done
bash $KIT/run.sh $AGENT $FLAT -Clean -Out "$S/centurion.png" -Script "$BASE; vr_test_give_ad 1; $G echo flatfx_test: centurion done; toggleconsole; quit" \
  -QuitOn "^flatfx_test: centurion done" -Timeout 150 -Filter "^(flatfx_test|flatsightcheck|sight line|  stance:)" > "$S/centurion.log" 2>&1
L=$S/centurion.log
block() { awk -v m="flatfx_test: wid $1" 'index($0,m)==1{on=1;next} /^flatfx_test: (wid|centurion done)/{on=0} on' "$L"; }
check $(grep -aq "^flatfx_test: centurion done" "$L" && echo 1 || echo 0) "centurion: the run reached its end"
for w in 22 25; do
  check $(block $w | grep -aq "stance: on.*free hand going on it, share 1.00" && echo 1 || echo 0) "wid $w: two-handed (the stance on, the free hand on its hotspot)"
  check $(block "$w ads" | grep -aqE "flatsightcheck: .* t 1.00 rear off 0.00[0-9]* front off 0.00[0-9]* " && echo 1 || echo 0) "wid $w: the ironsight lines its sights up"
done
check $(grep -aq "sight line: learnt for progs/v_gruntgun.mdl#adv5" "$L" && echo 0 || echo 1) "centurion: its sight line the table's (clear of its drum), not learnt over it"
verdict
exit $fail
