#!/bin/bash
# polepose_test.sh <agent> -- flat-screen play, every pole weapon by one path (polepose, his flat notes 2026-11-25;
# ROUND26.md "polepose"): two headless flat runs in fast mode on vrtesthall, one with the spear (scene flat_spear), one with
# Dwell's Crystal Lance (vr_ad_test 207; needs Dwell in the base dir), each standing 200 units from vrtesthall's north wall
# (setpos 0 440, facing it), the thrust's own tip turns 0 (vr_flat_thrust_wind_pitch, _pitch: they turn it off the line by
# design), the pole track on (vr_flat_pole_track: its reports):
#   1. the other hand on the pole closed round it (view::drawnGripCurl, the four fingers' joints: over 0.5; it was 0, open),
#      held, through a thrust (V) and in the parry guard;
#   2. the pole's tip line (from the tip along the shot's way) on what the crosshair is on, 200 away: under 1 unit off,
#      held and through the thrust (the spear's was 44 off held, the Lance's 25 in the thrust);
#   3. the right mouse button held: the parry guard, the pole laid across the view (vr_flatplay_status "spear:", left over
#      80 degrees), the other hand on its shaft (off its line within 1 of where it stands held: the pole track's own bound),
#      closed round it; for the Lance the same pose and no ironsight (vr_flat_pole_rmb_parry), and QC's parry probe
#      (impulse 249, a blow from straight ahead) PARRIES in its guard.
# Each run ends on "pp: done" (-QuitOn); prints "polepose: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch
mkdir -p "$S"
PR="vr_flat_pole_track report"
one() { # <tag> <give>
  local SC="map vrtesthall; wait30; developer 1; vr_flatplay_debug 1; $2; wait60; god; notarget; setpos 0 440 25 0 90 0; wait40"
  SC="$SC; tune vr_flat_thrust_wind_pitch 0; tune vr_flat_thrust_pitch 0"
  # (36 frames held; the thrust 0.64 s: 46 frames, 50; the guard 0.05 s in: 40 frames.)
  SC="$SC; vr_flat_pole_track 1; wait36; echo pp: still; $PR; vr_flat_punch main v; wait50; echo pp: thrust; $PR"
  SC="$SC; +offhandattack; wait40; echo pp: parry; $PR; vr_flatplay_status; vr_flatplay_debug 2; wait1; vr_flatplay_debug 1; impulse 249; wait3; -offhandattack; wait30"
  SC="$SC; tune_reset vr_flat_thrust_wind_pitch; tune_reset vr_flat_thrust_pitch; echo pp: done; toggleconsole; quit"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SC" -QuitOn "^pp: done" -Timeout 300 \
    -Filter "^pp:|^poletrack: |spear: main|parry guard:|^flatsight: ironsight|parry probe|^flatsight: t " > "$S/polepose_$1.log" 2>&1
}
one spear "vr_scenario flat_spear"
one lance "vr_ad_test 207"
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
for w in spear lance; do
  L="$S/polepose_$w.log"
  block() { awk -v m="pp: $1" '$0==m || $0==m" "{on=1;next} /^pp:/{on=0} on' "$L"; }
  # A report's numbers: off lo hi, along lo hi, place lo hi (the first line); curl lo hi, the miss, its distance (the second).
  rep() { block "$1" | grep -a "poletrack: report" | head -1 | sed 's/^poletrack: report//; s/ \.\. / /g' | tr -c '0-9.\n-' ' ' | awk -v i="$2" '{ print $i }'; }
  grp() { block "$1" | grep -a "poletrack: grip curl" | head -1 | sed 's/ \.\. / /g' | tr -c '0-9.\n-' ' ' | awk -v i="$2" '{ print $i }'; }
  check $(grep -aq "^pp: done" "$L" && echo 1 || echo 0) "$w: the run reached its end"
  for b in still thrust parry; do
    lo=$(grp $b 1); hi=$(grp $b 2)
    check $(ok "\"$lo\" != \"\" && $lo > 0.5") "$w $b: the other hand closed round the pole (finger curl ${lo:-?} .. ${hi:-?})"
  done
  for b in still thrust; do
    m=$(grp $b 3); d=$(grp $b 4)
    check $(ok "\"$m\" != \"\" && $m < 1 && $d > 190 && $d < 210") "$w $b: the tip line on the crosshair's point ($m off, ${d:-?} away)"
  done
  so=$(rep still 2); po=$(rep parry 3); st=$(block parry | grep -a "spear: main" | head -1)
  left=$(grep -ao "left -\?[0-9.]*" <<<"$st" | head -1 | grep -ao -- '-\?[0-9.]*$')
  check $(ok "\"$left\" != \"\" && $left > 80") "$w parry: the guard, the pole across the view (left ${left:-?})"
  check $(ok "\"$po\" != \"\" && \"$so\" != \"\" && $po <= $so + 1") "$w parry: the other hand on the shaft (off ${po:-?}, held ${so:-?})"
  check $(block parry | grep -aq "parry probe.*PARRIES" && echo 1 || echo 0) "$w parry: QC's probe parries in the guard ($(block parry | grep -a 'parry probe' | head -1 | cut -c1-80))"
done
sa=$(awk '$0=="pp: parry" || $0=="pp: parry "{on=1;next} /^pp:/{on=0} on' "$S/polepose_lance.log" | grep -a "^flatsight: t " | head -1 | grep -ao "ads [a-z ]* t [0-9.]*")
check $(grep -q "ads [a-z ]* t 0.00" <<<"$sa" && ! grep -q "ads on\|ads zoom" <<<"$sa" && echo 1 || echo 0) "lance parry: no ironsight in the guard (${sa:-no status})"
verdict
exit $fail
