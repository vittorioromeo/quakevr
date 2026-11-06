#!/bin/bash
# flatsight_test.sh <agent> -- the flatscreen ironsight solved from each gun's sight line, and the held weapon against the
# walls (docs/vr-port/FLATPLAY.md, "The ironsight's solve", "Against the walls"). Reads vr_flat_sight_check, which measures the
# weapon as drawn, not the solve: how far its rear and front sights stand off the ray through the crosshair, and where the
# sight line is 4096 past the rear sight, as an angle. Fast mode; the runs end with their own scripts.
#   a. every weapon (vr_test_give_wid, impulse 149), at three fields of view, then with the eye nudged: within tolerance on every gun that has
#      a sight line, and the ones without are listed;
#   b. looks up and down, a run forward with the looks on, a bigger eye relief: the same on the shotgun;
#   c. the picture moved on the screen: the sights lie on the moved ray, and the gun still points at the crosshair's point;
#   d. a wall: walked into, in hipfire and in the sight: the gun goes back, its points stay out of the level, it points at the
#      crosshair's point, the sights stay on the ray, and it comes out with no jump as the wall clears.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
export QVR_AUTHOR_WAIT_ALWAYS=1
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
TOL=0.02 # units off the ray, each sight; degrees far along
P="map e1m1; god; notarget; developer 1; r_norefresh 0; setpos 480 -352 88 0 0 0"
# (wait60 after the press: the sight's travel is 18 frames.)
ONE() { echo "vr_flat_aim 0; wait3; vr_test_give_wid $1; impulse 149; wait25; vr_flat_aim 1; wait60; vr_flat_sight_check;"; }
# Worst of: rear off, front off, far degrees, over the lines of a log; and how many lines with sights.
worst() { grep -a "^flatsightcheck: .* rear off" "$1" | awk '{ for (i = 1; i <= NF; i++) { if ($i == "rear" && $(i+1) == "off") { r = $(i+2) + 0 } if ($i == "front" && $(i+1) == "off") f = $(i+2) + 0; if ($i == "far") { d = $(i+3); gsub(/[(]/, "", d); d = d + 0 } } if (r > a) a = r; if (f > b) b = f; if (d > c) c = d; n++ } END { printf "%.4f %.4f %.4f %d", a + 0, b + 0, c + 0, n + 0 }'; }
good() { awk -v r="$1" -v f="$2" -v d="$3" -v n="$4" -v t="$TOL" -v m="$5" 'BEGIN { exit !(r <= t && f <= t && d <= t && n >= m) }' && echo 1 || echo 0; }

# a. Every weapon, at three fields of view and with the eye nudged.
ALL=""
for i in $(seq 1 31); do ALL="$ALL $(ONE $i)"; done
logs=""
for F in "fov 90" "fov 110" "fov 70" "fov 90; tune vr_flat_cam_fwd 6; tune vr_flat_cam_up -4; tune vr_flat_cam_side 3"; do
  n=$((n + 1)); log=$HERE/scratch/flatsight_a$n.log; logs="$logs $log"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; $F; wait30; $ALL vr_flat_aim 0; wait2; tune_reset; toggleconsole; quit" -Filter "^flatsightcheck" -Timeout 110 > "$log" 2>&1
  read -r r f d c <<<"$(worst "$log")"
  echo "  $F: $c guns with sights, at most rear $r front $f units off the ray, $d degrees far along"
  check $(good "$r" "$f" "$d" "$c" 15) "every gun's sights on the ray: $F"
done
echo "  no sight line: $(grep -a -h "no sight line" $logs | awk '{ print $2 }' | sort -u | tr '\n' ' ')"

# b. Looks up and down, a run with the looks on, the eye relief moved.
log=$HERE/scratch/flatsight_b.log
A="setpos 480 -352 88"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait30; vr_flat_aim 1; wait30; $A 60 0 0; wait8; vr_flat_sight_check; $A -60 0 0; wait8; vr_flat_sight_check; $A 0 0 0; wait3; tune vr_flat_ads_relief 20; wait5; vr_flat_sight_check; tune vr_flat_ads_relief 4; wait5; vr_flat_sight_check; tune_reset vr_flat_ads_relief; wait3; +forward; wait10; vr_flat_sight_check; wait7; vr_flat_sight_check; wait7; vr_flat_sight_check; -forward; wait5; vr_flat_aim 0; wait2; toggleconsole; quit" -Filter "^flatsight(check|pose)" -Timeout 60 > "$log" 2>&1
read -r r f d c <<<"$(worst "$log")"
echo "  looks, relief, a run: $c reads, at most rear $r front $f units off the ray, $d degrees far along"
grep -a "^flatsightpose" "$log" | awk '{ printf "%s ", $3 } END { print "(the relief read each time)" }' | sed 's/^/  /'
check $(good "$r" "$f" "$d" "$c" 7) "the shotgun's sights on the ray: looks up and down 60 degrees, the eye relief moved, a run with the looks on"
# Looks near straight up and down: the weapon's own turns in the view are Euler angles added (the gun's model pitch), which stop being one
# rotation near the pole. Read on their own, at their own limit: half a degree.
log=$HERE/scratch/flatsight_b2.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait30; vr_flat_aim 1; wait30; $A 85 0 0; wait8; vr_flat_sight_check; $A -85 0 0; wait8; vr_flat_sight_check; vr_flat_aim 0; wait2; toggleconsole; quit" -Filter "^flatsightcheck" -Timeout 60 > "$log" 2>&1
read -r r f d c <<<"$(worst "$log")"
echo "  looks at 85 degrees: at most rear $r front $f units off the ray, $d degrees far along"
check $(awk -v d="$d" -v c="$c" 'BEGIN { exit !(d <= 0.5 && c >= 2) }' && echo 1 || echo 0) "the shotgun's sights near the ray at looks of 85 degrees (within half a degree: the pole)"

# c. The picture moved on the screen.
log=$HERE/scratch/flatsight_c.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait30; tune vr_flat_ads_screen_x 5; tune vr_flat_ads_screen_y -8; vr_flat_aim 1; wait40; vr_flat_sight_check; vr_flatplay_debug 2; wait10; vr_flatplay_debug 0; vr_flat_aim 0; wait3; tune_reset; toggleconsole; quit" -Filter "^flatsightcheck|^flataim: main" -Timeout 60 > "$log" 2>&1
read -r r f d c <<<"$(worst "$log")"
aim=$(grep -a "^flataim: main" "$log" | tail -4 | awk '{ for (i = 1; i <= NF; i++) if ($i == "point") { x = $(i+1) + 0; if (x > m) m = x } } END { printf "%.4f", m + 0 }')
echo "  the picture at 5 -8: rear $r front $f units off its ray, $d degrees far along; the shot $aim degrees off the crosshair's point"
check $(good "$r" "$f" "$d" "$c" 1) "the picture moved: the sights lie on the moved ray"
check $(awk -v a="${aim:-9}" 'BEGIN { exit !(a <= 0.1) }' && echo 1 || echo 0) "the picture moved: the shot points at the moved crosshair's point"

# d. A wall: the test hall's start has a tall panel on its right: stepped against, turned to face. Hipfire, then the sight, then a
# turn away from it, frame by frame. The push back alone (vr_flat_wall_tilt 0): the muzzle's tilt past it is flatwall_test.sh's.
log=$HERE/scratch/flatsight_d.log
W="+moveright; wait60; -moveright; wait3; cl_yawspeed 90; +right; wait72; -right; wait10"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map vrtesthall; god; notarget; developer 1; r_norefresh 0; tune vr_flat_wall_tilt 0; impulse 154; wait30; $W; vr_flatplay_debug 2; wait10; vr_gun_wall_depth; vr_flat_aim 1; wait30; vr_flat_sight_check; vr_gun_wall_depth; wait5; +left; wait72; -left; wait20; vr_flatplay_debug 0; vr_flat_aim 0; wait3; tune_reset; toggleconsole; quit" -Filter "^flatsightcheck|^flataim: main|^flatsight: |^gunwall:" -Timeout 60 > "$log" 2>&1
grep -a "^gunwall:" "$log" | cut -c1-150 | sed 's/^/  /'
back=$(grep -a -o "back [0-9.]*" "$log" | awk '{ print $2 + 0 }')
peak=$(awk '{ if ($1 > m) m = $1 } END { printf "%.2f", m + 0 }' <<<"$back")
step=$(awk 'NR > 1 { d = p - $1; if (d > m) m = d } { p = $1 } END { printf "%.2f", m + 0 }' <<<"$back")
last=$(tail -1 <<<"$back")
read -r r f d c <<<"$(worst "$log")"
aim=$(grep -a "^flataim: main" "$log" | awk '{ for (i = 1; i <= NF; i++) if ($i == "point") { x = $(i+1) + 0; if (x > m) m = x } n++ } END { printf "%.4f %d", m + 0, n + 0 }')
echo "  the wall: back at most $peak units, its biggest step out a frame $step, $last left at the end; sights rear $r front $f; the shots at most ${aim% *} deg off the point (${aim#* } reads)"
check $(awk -v p="$peak" 'BEGIN { exit !(p >= 2) }' && echo 1 || echo 0) "the wall: the weapon goes back"
check $(awk -v s="$step" 'BEGIN { exit !(s <= 2.5) }' && echo 1 || echo 0) "the wall: it comes out with no jump"
check $(good "$r" "$f" "$d" "$c" 1) "the wall: the sights stay on the ray, pushed back"
check $(awk -v a="${aim% *}" 'BEGIN { exit !(a <= 0.1) }' && echo 1 || echo 0) "the wall: pushed back, it points at the crosshair's point"
verdict; exit $fail
