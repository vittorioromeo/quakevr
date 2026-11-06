#!/bin/bash
# flatwall_test.sh <agent> -- flat-screen held weapons against a wall (docs/vr-port/FLATPLAY.md, "Against the walls"): what the
# way back can't clear, the drawn muzzle tilts up out of (vr_flat_wall_tilt). Fast mode; each run ends on its own last line.
#   a. every weapon (vr_test_give_wid, impulse 149) with his box against the vrtesthall wall, facing it and at 45 degrees,
#      level and looking down 30 (and 60 off it looking up 30), hipfire and sighted (vr_flat_wall_tilt_sighted 1): how deep its drawn points go into the
#      level (vr_gun_wall_depth), the tilt off (vr_flat_wall_tilt 0: as before it) and on; and the two-handed holds the same.
#   b. the shotgun sighted (vr_flat_wall_tilt_sighted 1) walked into the wall and back off it, frame by frame: it tilts, the shots still go through the
#      crosshair's point while it is tilted, and it comes back down with no jump.
#   c. the tilted pose eases in and out over vr_flat_wall_tilt_time (0.2 s, the default, then 0.6); a fixed pitch with yaw and roll, the
#      left hand's mirrored.
#   d. vr_flat_wall_block_fire: tilted at the wall a click fires nothing (no shell spent); the option off, or off the wall, it fires.
# flatwall_test.sh <agent> [sections]: only those sections (default abcd).
AGENT=${1:-pickup}
ONLY=${2:-abcd}
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
LIMIT=2.0      # cm: a weapon's drawn points no deeper than this in the wall (the trace margin and the tilt's own 0.25 units)
LOOK=8.0       # cm: the same looking 30 down or up, his box pressed on the wall
SKIP=" 12 14 " # melee weapons longer than the room in front at a wall even upright (reported, not checked)

# a. Every weapon.
if [[ $ONLY == *a* ]]; then
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
  X="tune vr_flat_wall_tilt_sighted 1;"; [ $T = off ] && X="tune vr_flat_wall_tilt 0;"
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
fi

# b. Walked in and back off, sighted.
if [[ $ONLY == *b* ]]; then
log=$HERE/scratch/flatwall_b.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map vrtesthall; god; notarget; developer 1; r_norefresh 0; tune vr_flat_wall_tilt_sighted 1; vr_test_give_wid 4; impulse 149; wait25; setpos 0 590 25 0 90 0; vr_flat_aim 1; wait40; vr_flatplay_debug 2; +forward; wait50; -forward; wait30; +back; wait50; -back; wait40; vr_flatplay_debug 0; tune_reset; echo flatwallwalk done; toggleconsole; quit" -Filter "^flataim: main|^flatsight: |^flatwallwalk" -QuitOn "flatwallwalk done" -Timeout 120 > "$log" 2>&1
tilts=$(grep -a -o "tilt [0-9.]* *$\|tilt [0-9.]* need" "$log" | awk '{ print $2 + 0 }')
peak=$(awk '{ if ($1 > m) m = $1 } END { printf "%.1f", m + 0 }' <<<"$tilts")
step=$(awk 'NR > 1 { d = $1 - p; if (d < 0) d = -d; if (d > m) m = d } { p = $1 } END { printf "%.2f", m + 0 }' <<<"$tilts")
last=$(tail -1 <<<"$tilts")
aim=$(grep -a "^flataim: main" "$log" | awk '{ for (i = 1; i <= NF; i++) if ($i == "point") { x = $(i+1) + 0; if (x > m) m = x } n++ } END { printf "%.4f %d", m + 0, n + 0 }')
echo "  walked in sighted: tilt at most $peak deg, its biggest step a frame $step, $last at the end; the shots at most ${aim% *} deg off the point (${aim#* } reads)"
# (Walking in, the way back clears the sighted gun a few frames after it meets the wall: the tilt is wanted only those
# frames, so vr_flat_wall_tilt_time's ease takes it a few degrees up and back down; it went to 20 with the old 300 deg/s.)
check $(awk -v p="$peak" 'BEGIN { exit !(p >= 3) }' && echo 1 || echo 0) "walked into the wall sighted, the muzzle tilts up while the way back can't clear it"
check $(awk -v s="$step" 'BEGIN { exit !(s <= 8) }' && echo 1 || echo 0) "it tilts and comes back down with no jump (a frame's step at most 8 deg)"
check $(awk -v l="${last:-99}" 'BEGIN { exit !(l <= 0.5) }' && echo 1 || echo 0) "off the wall, the tilt is gone"
check $(awk -v a="${aim% *}" -v n="${aim#* }" 'BEGIN { exit !(a <= 0.1 && n >= 50) }' && echo 1 || echo 0) "tilted, the shots still go through the crosshair's point"
fi

# c. The tilted pose's ease and its mirroring: the shotgun hipfire, a fixed pitch of 40 with yaw 20 and roll 15, put on the
#    wall and taken off it (vr_flat_wall_tilt_time 0.2, the default, then 0.6), then in the left hand (impulse 48 swaps the hands).
# (Looking 20 down: there the way back leaves the shotgun in the wall, level it clears it. It was 30 down until his pose
# of 2026-11-06 became the defaults: Gun Down 2 and Chest Share Down 0 move the gun, and 30 down its way back clears.)
ON="setpos 0 623.5 25 20 90 0"; OFF="setpos 0 500 25 20 90 0"; F=90 # (frames: the longest ease, 0.6 s, is 43)
if [[ $ONLY == *c* ]]; then
log=$HERE/scratch/flatwall_c.log
M() { echo "echo flatwallease mark $1"; }
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map vrtesthall; god; notarget; developer 1; r_norefresh 0; tune vr_flat_wall_tilt_fixed 1; tune vr_flat_wall_tilt 40; tune vr_flat_wall_tilt_yaw 20; tune vr_flat_wall_tilt_roll 15; vr_test_give_wid 4; impulse 149; wait25; $OFF; wait20; vr_flatplay_debug 2; $(M in03); $ON; wait$F; $(M out03); $OFF; wait$F; tune vr_flat_wall_tilt_time 0.6; $(M in06); $ON; wait$F; $(M out06); $OFF; wait$F; tune vr_flat_wall_tilt_time 0.2; $(M main); $ON; wait$F; $(M swap); impulse 48; $OFF; wait$F; $(M off); $ON; wait$F; vr_flatplay_debug 0; tune_reset; echo flatwallease done; toggleconsole; quit" -Filter "^flatwallturn: |^flatwallease" -QuitOn "flatwallease done" -Timeout 180 > "$log" 2>&1
# Each phase: the ease's time (in: from the first frame off 0 to all of it; out: from the first frame under all to none),
# and the last frame's turn (the muzzle's way up and right, the top's lean right).
grep -a "^flatwallturn: \|^flatwallease mark" "$log" | awk '
  /^flatwallease mark/ { p = $3; next }
  { t = $3 + 0; on = $5 + 0
    if (p ~ /^in/) { if (!(p in a) && on > 0) a[p] = t; if (!(p in b) && on >= 0.999) b[p] = t }
    if (p ~ /^out/) { if (!(p in a) && on < 0.999) a[p] = t; if (!(p in b) && on <= 0.001) b[p] = t }
    last[p] = $9 " " $11 " " $13 }
  END { for (p in b) printf "ease %s %.3f\n", p, b[p] - a[p]; for (p in last) print "turn", p, last[p] }' > "$HERE/scratch/flatwall_c.txt"
e() { awk -v p="$1" '$1 == "ease" && $2 == p { print $3 }' "$HERE/scratch/flatwall_c.txt"; }
tn() { awk -v p="$1" '$1 == "turn" && $2 == p { print $3, $4, $5 }' "$HERE/scratch/flatwall_c.txt"; }
read -r mu ms mc <<<"$(tn main)"; read -r ou os oc <<<"$(tn off)"
echo "  ease in/out at 0.2 s: $(e in03) / $(e out03) s; at 0.6 s: $(e in06) / $(e out06) s"
echo "  turned (up side cant): right hand $mu $ms $mc; left hand $ou $os $oc"
near() { awk -v x="$1" -v lo="$2" -v hi="$3" 'BEGIN { exit !(x != "" && x >= lo && x <= hi) }' && echo 1 || echo 0; }
check $(near "$(e in03)" 0.16 0.22) "vr_flat_wall_tilt_time 0.2 (the default): the pose eases in over 0.2 s"
check $(near "$(e out03)" 0.16 0.22) "and back out off the wall over 0.2 s"
check $( [ "$(near "$(e in06)" 0.56 0.62)" = 1 ] && [ "$(near "$(e out06)" 0.56 0.62)" = 1 ] && echo 1 || echo 0) "0.6: over 0.6 s, in and out"
check $(awk -v mu="$mu" -v ms="$ms" -v mc="$mc" -v ou="$ou" -v os="$os" -v oc="$oc" 'function a(x) { return x < 0 ? -x : x }
  BEGIN { exit !(mu > 0.5 && ms < -0.1 && mc < -0.1 && a(ou - mu) < 0.02 && a(os + ms) < 0.02 && a(oc + mc) < 0.02) }' && echo 1 || echo 0) \
  "pitch up, yaw and roll inwards; the left hand's yaw and roll mirrored, its pitch the same"
fi

# d. Firing at the wall (vr_flat_wall_block_fire): tilted, a click fires nothing (no shell spent); with the option off it
#    fires; off the wall it fires. The default pose (the least tilt that clears).
if [[ $ONLY == *d* ]]; then
log=$HERE/scratch/flatwall_d.log
K="vr_mock_key mouse1 down; wait30; vr_mock_key mouse1 up; wait40; vr_flathud_report; wait2"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map vrtesthall; god; notarget; developer 1; r_norefresh 0; vr_flat_wall_block_fire 1; vr_test_give_wid 4; impulse 149; give s 50; wait25; $ON; wait$F; vr_flathud_report; wait2; vr_flatplay_debug 2; echo flatwallfire click; $K; vr_flatplay_debug 0; vr_flat_wall_block_fire 0; $K; vr_flat_wall_block_fire 1; $OFF; wait$F; $K; vr_flat_wall_block_fire 1; echo flatwallfire done; toggleconsole; quit" -Filter "^flathud: pools|^flatwallfire|^flatwallturn" -QuitOn "flatwallfire done" -Timeout 120 > "$log" 2>&1
read -r s0 s1 s2 s3 <<<"$(grep -a "^flathud: pools" "$log" | sed 's/.*shells \([0-9]*\).*/\1/' | tr '\n' ' ')"
turned=$(grep -a "^flatwallturn" "$log" | awk '{ if ($7 < m || !n) m = $7; n++; b += $15 } END { printf "%.1f deg at least, blocked %d of %d frames", m, b, n }')
echo "  shells: at the wall $s0, clicked $s1 (turned $turned); the option off, clicked $s2; off the wall, clicked $s3"
check $( [ -n "$s3" ] && [ "$s1" = "$s0" ] && echo 1 || echo 0) "tilted at the wall, a click fires nothing (no shell spent)"
check $( [ -n "$s3" ] && [ "$s2" -lt "$s1" ] && echo 1 || echo 0) "vr_flat_wall_block_fire 0: it fires tilted"
check $( [ -n "$s3" ] && [ "$s3" -lt "$s2" ] && echo 1 || echo 0) "off the wall, it fires"
fi
verdict; exit $fail
