#!/bin/bash
# flatstance_test.sh <agent> -- the flatscreen stance (docs/vr-port/FLATPLAY.md, "The stance"): on a long gun the free hand is
# drawn on the weapon's own two-handed hotspot from Weapon Offsets, and nothing else decides its place or its turn.
#   a. every long gun, hipfire and sight: the status's `stance:` lines (the share of the arm used), the hand's distance off
#      the hotspot where it is on it, printed for each;
#   b. a hotspot edited in Weapon Offsets moves the drawn hand by as much, and turns it;
#   c. vr_flat_stance 0: the torso faces the view, the hand hangs;
#   d. the rest's old lines are quiet.
# Fast mode: each run ends with its own script. Prints `flatstance: N passed, M failed` (qvrtest.sh's verdict).
AGENT=${1:-flatstance}
source "$(dirname "$0")/qvrtest.sh"
export QVR_AUTHOR_WAIT_ALWAYS=1
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
P="map e1m1; god; notarget; developer 1; r_norefresh 0; setpos 480 -352 88 0 0 0"
# wait60: the hand's ease (about 0.4 s of 1/72 s frames, 29 frames) and the sight's travel (18) are both over.
GUN() { echo "vr_flat_aim 0; wait3; vr_test_give_wid $1; impulse 149; wait60; echo gun $1 hipfire; vr_flatplay_status; vr_flat_aim 1; wait60; echo gun $1 sight; vr_flatplay_status;"; }

# a. Every weapon: which have a hotspot, how much of the arm it takes.
ALL=""
for i in $(seq 1 31); do ALL="$ALL $(GUN $i)"; done
log=$HERE/scratch/flatstance_a.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; wait30; $ALL vr_flat_aim 0; wait2; toggleconsole; quit" -Filter "^gun |^  stance|^  weapon|^  main hand" -Timeout 200 > "$log" 2>&1
grep -a -E "^gun |^  stance" "$log" | cut -c1-210 > "$HERE/scratch/flatstance_a.txt"
on=$(grep -a -c "free hand going on it, share 1.00" "$log")
far=$(grep -a -c "out of reach" "$log")
none=$(grep -a -c "no two-handed hotspot" "$log")
off=$(grep -a -A3 "free hand going on it, share 1.00" "$log" | grep -a "stance hand" | awk '{ for (i = 1; i <= NF; i++) if ($i == "off") { x = $(i+1) + 0; if (x > m) m = x } } END { printf "%.2f", m + 0 }')
echo "  $on reads with the hand on its hotspot, $far out of reach, $none on guns with no hotspot; the drawn hand at most $off units off it: $HERE/scratch/flatstance_a.txt"
check $(awk -v n="$on" 'BEGIN { exit !(n >= 4) }' && echo 1 || echo 0) "the free hand reaches long guns' hotspots"
check $(awk -v o="${off:-9}" 'BEGIN { exit !(o <= 0.05) }' && echo 1 || echo 0) "the drawn hand stands on the hotspot"

# b. The shotgun's hotspot edited: 3 units along, 25 degrees of pitch.
log=$HERE/scratch/flatstance_b.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait90; vr_flatplay_status; wait; vr_wofs_hs1_x_02 6.66; wait30; vr_flatplay_status; wait; vr_wofs_hs1_x_02 9.66; vr_wofs_hs1_pitch_02 25; wait30; vr_flatplay_status; wait; vr_wofs_hs1_pitch_02 0; wait2; toggleconsole; quit" -Filter "^  stance" -Timeout 60 > "$log" 2>&1
grep -a "^  stance" "$log" | cut -c1-170 | sed 's/^/ /'
read -r s0 s1 s2 <<<"$(grep -a "^  stance spot" "$log" | awk '{ for (i = 1; i <= NF; i++) if ($i == "fwd") { printf "%s ", $(i+1) } }')"
read -r m0 m1 m2 <<<"$(grep -a "^  stance hand" "$log" | awk '{ for (i = 1; i <= NF; i++) if ($i == "fwd") { printf "%s ", $(i+1) } }')"
read -r a0 a1 a2 <<<"$(grep -a "^  stance hand" "$log" | awk '{ for (i = 1; i <= NF; i++) if ($i == "turned") { printf "%s ", $(i+1) } }')"
echo "  the hotspot forward of the held hand: $s0 to $s1 (the weapon's scale); the drawn hand: $m0 to $m1; pitched: $a0 to $a2"
check $(awk -v a="$m0" -v b="$m1" -v c="$s0" -v d="$s1" 'BEGIN { e = (b - a) - (d - c); exit !(d - c > 1 && e > -0.05 && e < 0.05) }' && echo 1 || echo 0) "Weapon Offsets' hotspot moved: the flat hand moved as far"
check $(awk -v a="$a0" -v b="$a2" 'BEGIN { d = b - a; if (d < 0) d = -d; exit !(d > 5) }' && echo 1 || echo 0) "Weapon Offsets' hotspot turned: the flat hand turned"

# c. Off: the torso faces the view, the hand hangs. d. The old lines say nothing.
log=$HERE/scratch/flatstance_c.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait90; tune vr_flat_stance 0; wait60; vr_flatplay_status; wait; vr_flat_rest 1; vr_flat_rest_along 0.5; vr_flat_rest_below 2; vr_flat_rest_relief 7; echo old lines done; tune_reset; wait2; toggleconsole; quit" -Filter "^  stance|Unknown|old lines" -Timeout 60 > "$log" 2>&1
grep -a "^  stance:" "$log" | cut -c1-170 | sed 's/^/ /'
check $(grep -a "^  stance:" "$log" | grep -a -q -E "off, torso -?0.0 deg, free hand hanging, share 0.00" && echo 1 || echo 0) "vr_flat_stance 0: torso on the view, the hand hanging"
check $(grep -a -q "Unknown" "$log" && echo 0 || echo 1) "vr_flat_rest's old lines are quiet"
verdict; exit $fail
