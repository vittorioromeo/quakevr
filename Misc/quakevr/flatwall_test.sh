#!/bin/bash
# flatwall_test.sh <agent> -- flat-screen held weapons against a wall (docs/vr-port/FLATPLAY.md, "Against the walls"): what the
# way back can't clear, the drawn muzzle tilts up out of (vr_flat_wall_tilt). Fast mode; each run ends on its own last line.
#   a. every weapon (vr_test_give_wid, impulse 149) with his box against the vrtesthall wall, facing it and at 45 degrees,
#      level and looking down 30 (and 60 off it looking up 30), hipfire and sighted: how deep its drawn points go into the
#      level (vr_gun_wall_depth), the tilt off (vr_flat_wall_tilt 0: as before it) and on; and the two-handed holds the same.
#   b. the shotgun sighted walked into the wall and back off it, frame by frame: it tilts, the shots still go through the
#      crosshair's point while it is tilted, and it comes back down with no jump.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
export QVR_AUTHOR_WAIT_ALWAYS=1
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
LIMIT=2.0      # cm: a weapon's drawn points no deeper than this in the wall (the trace margin and the tilt's own 0.25 units)
LOOK=8.0       # cm: the same looking 30 down or up, his box pressed on the wall
SKIP=" 12 14 " # melee weapons longer than the room in front at a wall even upright (reported, not checked)

# a. Every weapon.
A="map vrtesthall; god; notarget; developer 1; r_norefresh 0"
for i in $(seq 1 31); do
  A="$A; vr_flat_aim 0; wait3; vr_test_give_wid $i; impulse 149; wait25"
  for YP in "90 0" "45 0" "90 30" "60 -30"; do
    set -- $YP
    A="$A; setpos 0 623.5 25 $2 $1 0; vr_flat_aim 0; wait40; echo flatwallat w $i y $1 p $2 hip; vr_gun_wall_depth; vr_flat_aim 1; wait40; echo flatwallat w $i y $1 p $2 ads; vr_gun_wall_depth"
  done
done
table() { grep -a "^flatwallat\|^gunwall: main" "$1" | awk '/^flatwallat/ { t = $3 " " $5 " " $7 " " $8; next } { d = $7; sub(/cm/, "", d); print t, $3, d + 0 }'; }
for T in off on; do
  log=$HERE/scratch/flatwall_a_$T.log
  X=""; [ $T = off ] && X="tune vr_flat_wall_tilt 0;"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$X $A; echo flatwallat done; tune_reset; toggleconsole; quit" -Filter "^flatwallat|^gunwall: main" -QuitOn "flatwallat done" -Timeout 600 > "$log" 2>&1
  table "$log" > "$HERE/scratch/flatwall_a_$T.txt"
done
read -r n0 deep0 <<<"$(awk -v s="$SKIP" -v l=$LIMIT 'index(s, " " $1 " ") == 0 { if ($6 > l) n++; if ($6 > m) m = $6 } END { printf "%d %.1f", n + 0, m + 0 }' "$HERE/scratch/flatwall_a_off.txt")"
read -r n1 deep1 reads <<<"$(awk -v s="$SKIP" -v l=$LIMIT 'index(s, " " $1 " ") == 0 { if ($6 > l) n++; if ($6 > m) m = $6; r++ } END { printf "%d %.1f %d", n + 0, m + 0, r + 0 }' "$HERE/scratch/flatwall_a_on.txt")"
# (Level: within LIMIT. Looking 30 down or up with his box pressed on the wall, a long gun upright still reaches it: within LOOK.)
read -r lvl look <<<"$(awk -v s="$SKIP" -v l=$LIMIT -v k=$LOOK 'index(s, " " $1 " ") == 0 { if ($3 == 0 && $6 > l) a++; if ($3 != 0 && $6 > k) b++ } END { printf "%d %d", a + 0, b + 0 }' "$HERE/scratch/flatwall_a_on.txt")"
echo "  tilt off: $n0 reads deeper than $LIMIT cm, deepest $deep0; on: $n1 of $reads, deepest $deep1"
awk -v l=$LIMIT '$6 > l { printf "    over: w %s yaw %s pitch %s %s %s %.1f cm\n", $1, $2, $3, $4, $5, $6 }' "$HERE/scratch/flatwall_a_on.txt" | head -12
holds=$(paste -d' ' "$HERE/scratch/flatwall_a_off.txt" "$HERE/scratch/flatwall_a_on.txt" | awk '$5 == "held+hotspot" && $11 == "held" { n++ } END { print n + 0 }')
echo "  two-handed holds lost to the tilt: $holds"
check $(awk -v a="$lvl" -v r="$reads" 'BEGIN { exit !(a == 0 && r >= 220) }' && echo 1 || echo 0) "level, every weapon out of the wall within $LIMIT cm, tilted (but the long melee weapons:$SKIP)"
check $(awk -v b="$look" 'BEGIN { exit !(b == 0) }' && echo 1 || echo 0) "looking 30 down or up, within $LOOK cm (was $deep0 at most with no tilt)"
check $([ "$holds" = 0 ] && echo 1 || echo 0) "the tilt keeps every two-handed hold"

# b. Walked in and back off, sighted.
log=$HERE/scratch/flatwall_b.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map vrtesthall; god; notarget; developer 1; r_norefresh 0; vr_test_give_wid 4; impulse 149; wait25; setpos 0 590 25 0 90 0; vr_flat_aim 1; wait40; vr_flatplay_debug 2; +forward; wait50; -forward; wait10; +back; wait50; -back; wait40; vr_flatplay_debug 0; echo flatwallwalk done; toggleconsole; quit" -Filter "^flataim: main|^flatsight: |^flatwallwalk" -QuitOn "flatwallwalk done" -Timeout 120 > "$log" 2>&1
tilts=$(grep -a -o "tilt [0-9.]* *$\|tilt [0-9.]* need" "$log" | awk '{ print $2 + 0 }')
peak=$(awk '{ if ($1 > m) m = $1 } END { printf "%.1f", m + 0 }' <<<"$tilts")
step=$(awk 'NR > 1 { d = $1 - p; if (d < 0) d = -d; if (d > m) m = d } { p = $1 } END { printf "%.2f", m + 0 }' <<<"$tilts")
last=$(tail -1 <<<"$tilts")
aim=$(grep -a "^flataim: main" "$log" | awk '{ for (i = 1; i <= NF; i++) if ($i == "point") { x = $(i+1) + 0; if (x > m) m = x } n++ } END { printf "%.4f %d", m + 0, n + 0 }')
echo "  walked in sighted: tilt at most $peak deg, its biggest step a frame $step, $last at the end; the shots at most ${aim% *} deg off the point (${aim#* } reads)"
check $(awk -v p="$peak" 'BEGIN { exit !(p >= 20) }' && echo 1 || echo 0) "walked into the wall sighted, the muzzle tilts up"
check $(awk -v s="$step" 'BEGIN { exit !(s <= 8) }' && echo 1 || echo 0) "it tilts and comes back down with no jump (a frame's step at most 8 deg)"
check $(awk -v l="${last:-99}" 'BEGIN { exit !(l <= 0.5) }' && echo 1 || echo 0) "off the wall, the tilt is gone"
check $(awk -v a="${aim% *}" -v n="${aim#* }" 'BEGIN { exit !(a <= 0.1 && n >= 50) }' && echo 1 || echo 0) "tilted, the shots still go through the crosshair's point"
verdict; exit $fail
