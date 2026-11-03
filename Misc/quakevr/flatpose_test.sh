#!/bin/bash
# flatpose_test.sh <agent> -- the numbers this measures, in one place (docs/vr-port/FLATPLAY.md, "Where the weapon and
# the hands sit" and "Where the camera stands"). Five headless flatscreen runs (-flatscreenmode, -Low), each of them at
# vr_mock_eye_size 256, r_norefresh 0, developer 1, vr_flatplay_debug 2, standing at e1m1's 480 -352 88 with a look due
# east and no pitch, the shotgun in the main hand (impulse 154), and prints what it read rather than gating it:
#   a. the defaults: the muzzle's distance off the crosshair's ray with the ironsight off, then with it settled on, and
#      the field of view it settles at (vr_flat_aim_fov 12);
#   b. the hipfire pose moved (vr_flat_gun_fwd 26 vr_flat_gun_down 4 vr_flat_gun_out 8 vr_flat_gun_near 14): the muzzle's
#      place, and the direction the shots go (flatmove:'s degrees off the view), which the pose is not allowed to move;
#   c. the ironsight's own set moved (vr_flat_ads_fwd 6 vr_flat_ads_up 3 vr_flat_ads_out -4): the muzzle's distance off
#      the ray, and the damage to a knight standing on the crosshair 24 units away;
#   d. the eye nudged (vr_flat_cam_fwd 12 vr_flat_cam_up 6 vr_flat_cam_side 4) with the sight on: the muzzle's distance
#      off the ray the nudged camera reads, the same knight's damage, the drawn eye's height, and his own origin;
#   e. the same nudges with Left Ctrl held (+flatcrouch, the crouch suite's way): his origin, which must not move, and
#      the eye's height, which is the nudges on top of the crouch drop.
# Each run's log is kept under Misc/quakevr/scratch. Prints the readings; a PASS/FAIL line for the two things that must
# hold whatever these are set to: the shot stays on the crosshair, and his origin does not move.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
P="map e1m1; god; notarget; developer 1; r_norefresh 0; vr_mock_eye_size 256; setpos 480 -352 88 0 0 0; impulse 154"
# A record of a printed class, the console's wrapped lines joined back: flatsight: the muzzle's distance off the
# crosshair's ray, flatcam: the drawn eye's and his origin's place, flatmove: the frame's shot against the view.
recs() { awk '{ if ($0 ~ /^(flatsight|flatcam|flatmove): /) { if (rec != "") print rec; rec = $0 } else if (rec != "") { rec = rec " " $0 } } END { if (rec != "") print rec }' "$1"; }
# The last record's "crosshair B -> A" pair: B is the muzzle's distance off the ray before the ironsight moved the hand,
# A after it (a class's first records are stale: the pose and its probe run before the view is made).
muzzle() { awk '{ for (i = 1; i <= NF; i++) if ($i == "crosshair") { b = $(i+1) + 0; a = $(i+3) + 0 } } END { printf "%.2f %.2f", b + 0, a + 0 }'; }
# The widest fov_x a run's flatsight: lines carry, and the last one's (the settled value).
maxfov() { grep -a -o "fov [0-9.]* x" "$1" | awk '{ if ($2 > m) m = $2 } END { printf "%.2f", m + 0 }'; }
lastfov() { grep -a -o "fov [0-9.]* x" "$1" | tail -1 | awk '{ printf "%.2f", $2 + 0 }'; }
# The settled field of view with the ironsight still on (the last of the records that carry it).
lastfovOn() { grep -a " ads on " "$1" | grep -a -o "fov [0-9.]* x" | tail -1 | awk '{ printf "%.2f", $2 + 0 }'; }
# The worst shot-off-the-view over the run's flatmove: lines, and how many records carried one.
shotdeg() { recs "$1" | grep -a "^flatmove: " | awk '{ if (match($0, /off the view [0-9.]+/)) { d = substr($0, RSTART + 11, RLENGTH - 11) + 0; if (d > best) best = d; n++ } } END { printf "%.4f %d", best + 0, n + 0 }'; }
# What each shot left the monster at: vr_test_hurt 1 reads bracketing it, their own 1 unit of damage left out. Each shot
# has its own fresh monster, and each group is closed by a vr_flatplay_status line, so a wounded or a replaced monster
# cannot be paired with the wrong shot (the spawn's own print does not reach the console the harness reads).
gaps() { grep -a -E '^(test spawn: |test hurt: |  ironsight: )' "$1" | awk '
    !/^test hurt: / { prev = ""; seen = 0; next }
    { if (match($0, /health [0-9.]+ -> [0-9.]+/)) { split(substr($0, RSTART + 7, RLENGTH - 7), b, " ->")
      if (seen) printf "%.1f ", prev - b[1]; prev = b[2]; seen = 1 } }'; }
# The drawn eye's height above his origin, and his origin's z, from the flatcam: lines (the last record of each).
camread() { recs "$1" | grep -a "^flatcam: " | awk '{ for (i = 1; i <= NF; i++) { if ($i == "above") h = $(i+2) + 0; if ($i == "origin") z = $(i+3) + 0
             if ($i == "nudge") { nx = $(i+1) + 0; ny = $(i+2) + 0; nz = $(i+3) + 0 } } }
    END { printf "%.1f %.1f %.2f %.2f %.2f", h + 0, z + 0, nx + 0, ny + 0, nz + 0 }'; }

# a. the defaults, the ironsight off and then settled on.
log=$HERE/scratch/pose_default.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; wait30; vr_flat_aim 1; wait40; vr_flat_aim 0; wait30; wait; toggleconsole; quit" -Filter "^flatsight: " > "$log" 2>&1
read -r off0 off1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads off" | tail -4 | muzzle)"
read -r on0 on1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads on" | tail -4 | muzzle)"
echo "  defaults: the muzzle $off0 units off the crosshair's ray with the ironsight off, $on0 before the held hand's move and $on1 after it with the sight settled on; field of view $(maxfov "$log") plain, $(lastfovOn "$log") settled with the sight on (vr_flat_aim_fov 12), $log"

# b. the hipfire pose moved, the ironsight off so the measure is the pose's own.
log=$HERE/scratch/pose_gun.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; wait30; vr_flat_gun_fwd 26; vr_flat_gun_down 4; vr_flat_gun_out 8; vr_flat_gun_near 14; wait40; vr_flatplay_status; wait; toggleconsole; quit" -Filter "^flatsight: |^flatmove: |^  where the weapon" > "$log" 2>&1
read -r g0 g1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads off" | tail -4 | muzzle)"
deg=$(shotdeg "$log")
grep -a "^  where the weapon" "$log" | head -1 | sed 's/^/  status:/' | cut -c1-400
echo "  vr_flat_gun_fwd 26 vr_flat_gun_down 4 vr_flat_gun_out 8 vr_flat_gun_near 14: the muzzle $g0 units off the crosshair's ray (it was $off0 at the defaults); the shots over the run at most ${deg% *} deg off the view, over ${deg#* } records, $log"
check $(awk -v d="${deg% *}" -v n="${deg#* }" 'BEGIN { exit !(n > 20 && d <= 0.1) }' && echo 1 || echo 0) "pose: moved, and every shot still on the crosshair"

# c. the ironsight's own set moved, onto a knight standing on the crosshair 24 units away. Three shots, each at a fresh
# knight: the offsets, then the same sight with the offsets at zero (the control), then the sideways part alone.
log=$HERE/scratch/pose_ads.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; vr_flat_aim 1; wait30; vr_flat_ads_fwd 6; vr_flat_ads_up 3; vr_flat_ads_out -4; wait20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait5; vr_test_kill 1; vr_flat_ads_fwd 0; vr_flat_ads_up 0; vr_flat_ads_out 0; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait5; vr_test_kill 1; vr_flat_ads_out 20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait10; wait; toggleconsole; quit" -Filter "test spawn: |test hurt: |^flatsight: |^flatmove: |^  ironsight: " > "$log" 2>&1
read -r a0 a1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads 6 3 -4" | tail -4 | muzzle)"
read -r c0 c1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads 0 0 0" | tail -4 | muzzle)"
read -r w0 w1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads 0 0 20" | tail -4 | muzzle)"
deg=$(shotdeg "$log"); read -r s1 s2 s3 <<<"$(gaps "$log")"
echo "  vr_flat_ads_fwd 6 vr_flat_ads_up 3 vr_flat_ads_out -4: the muzzle $a0 units off the crosshair's ray before the weapon is brought onto it ($a1 after), the knight on the crosshair took $s1 units; the same sight with the offsets at zero: $c0 before, $c1 after, $s2 units; vr_flat_ads_out 20 alone: $w0 before, $w1 after, $s3 units; the shots at most ${deg% *} deg off the view, over ${deg#* } records, $log"
check $(awk -v d="${deg% *}" -v n="${deg#* }" 'BEGIN { exit !(n > 20 && d <= 0.1) }' && echo 1 || echo 0) "pose: the ironsight's offsets go on before the centring, and every shot still points along the crosshair"
check $(awk -v a="$a1" -v c="$c1" -v w="$w1" -v s1="$s1" -v s2="$s2" -v s3="$s3" 'BEGIN { exit !(a < 1 && c < 1 && w < 1 && s1 + 0 > 0 && s2 + 0 > 0 && s3 + 0 > 0) }' && echo 1 || echo 0) "pose: the centring brings the muzzle back onto the crosshair's ray whatever the ironsight's offsets asked for, and each shot hits the foe standing on it"

# d. the eye nudged, with the sight on: one shot at a fresh knight 24 units down the line the game aims along before the
#    nudges, and one at another after them. Only the first is gated on hurting him. impulse 241 stands the knight on the
#    player's own axis, at the yaw the game sees, while the nudges move the crosshair's ray 12 forward, 6 up and 4 to the
#    view's right off that axis -- so the second shot leaves the drawn muzzle along a line the test knight does not stand
#    on (and, 12 units further forward, at 24 the muzzle stands inside his box: a knight's box is 32 across). Measured
#    2026-11-03 at vr_test_spawn_dist 24, 40 and 64: 36 units of damage before the nudges, none after them. The echo
#    prints both numbers; what this case gates on is the aiming's own readings, below.
log=$HERE/scratch/pose_cam.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; vr_flat_aim 1; wait20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait5; vr_test_kill 1; vr_flat_cam_fwd 12; vr_flat_cam_up 6; vr_flat_cam_side 4; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait; toggleconsole; quit" -Filter "^flatsight: |^flatcam: |test spawn: |test hurt: |^  ironsight: " > "$log" 2>&1
read -r eye0 org0 nx ny nz <<<"$(recs "$log" | grep -a "^flatcam: " | tail -4 | camread)"
read -r n0b n0a n1b n1a <<<"$(recs "$log" | awk '
    /^flatcam: / { for (i = 1; i <= NF; i++) if ($i == "nudge") { nudged = $(i+1) + 0 != 0 || $(i+2) + 0 != 0 || $(i+3) + 0 != 0 }
                  next }
    /^flatsight: / { if ($0 !~ / ads on /) next
                  for (i = 1; i <= NF; i++) if ($i == "crosshair") { b = $(i+1) + 0; a = $(i+3) + 0 }
                  if (nudged) { b2 = b; a2 = a } else { b1 = b; a1 = a } }
    END { printf "%.2f %.2f %.2f %.2f", b1 + 0, a1 + 0, b2 + 0, a2 + 0 }')"
read -r dmg0 dmg1 <<<"$(gaps "$log")"
echo "  vr_flat_cam_fwd 12 vr_flat_cam_up 6 vr_flat_cam_side 4: the drawn eye $eye0 units above his origin, the nudges read as $nx $ny $nz in the world at this look, his origin $org0; the muzzle $n0a units off the crosshair's ray with the sight settled before the nudges and $n1b / $n1a after them; the knight on the crosshair took ${dmg0:-no read} units from the shot before the nudges and ${dmg1:-no read} from the one after them, $log"
check $(awk -v o="$org0" 'BEGIN { exit o == 88 }' && echo 1 || echo 0) "view: the nudges move the drawn eye and leave his origin at 88"
check $(awk -v e="$eye0" -v x="$nx" -v y="$ny" -v z="$nz" 'BEGIN { exit !(e > 27 && e < 29 && x == 12 && y == 4 && z == 6) }' && echo 1 || echo 0) "view: at this look the nudges read as 12 forward, 4 to the world's east and 6 up"
check $(awk -v b="$n1b" -v a="$n1a" 'BEGIN { exit !(a < 1 && b > 5) }' && echo 1 || echo 0) "view: the sight closes the muzzle back onto the nudged camera's crosshair ray"
check $(awk -v d0="${dmg0:-0}" 'BEGIN { exit !(d0 + 0 > 0) }' && echo 1 || echo 0) "view: the foe standing on the crosshair is hit by the shot fired before the nudges"

# e. the same nudges with Left Ctrl held: the crouch comes down on top of them. The game's own report (vr_flatkeys_test 1)
# is the one that says whether the *player* moved: its eye is his head above the bottom of his box (headpos_z - absmin_z)
# and its origin is the server's player.origin_z.
log=$HERE/scratch/pose_camcrouch.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flat_cam_fwd 12; vr_flat_cam_up 6; vr_flat_cam_side 4; wait30; vr_flatplay_debug 2; wait20; vr_flatkeys_test 1; wait10; +flatcrouch; wait40; vr_flatkeys_test 1; wait10; -flatcrouch; wait30; vr_flatkeys_test 1; wait10; wait; toggleconsole; quit" -Filter "^flatcam: |^flatkeys: crouched" > "$log" 2>&1
fk() { grep -a "^flatkeys: crouched" "$log" | sed -n "${1}p" | awk '{ for (i = 1; i <= NF; i++) { if ($i == "crouched") c = $(i+1) + 0; if ($i == "eye") e = $(i+1) + 0; if ($i == "origin") o = $(i+1) + 0 } printf "%.1f %.1f %.1f", c + 0, e + 0, o + 0 }'; }
read -r k0 h0 o0 <<<"$(fk 1)"; read -r k1 h1 o1 <<<"$(fk 2)"; read -r k2 h2 o2 <<<"$(fk 3)"
read -r eyeStand eyeCouch orgC <<<"$(recs "$log" | grep -a "^flatcam: " | awk '
    { for (i = 1; i <= NF; i++) { if ($i == "above") h = $(i+2) + 0; if ($i == "origin") z = $(i+3) + 0 }
      if (h > top) top = h; if (bottom == 0 || h < bottom) bottom = h }
    END { printf "%.1f %.1f %.1f", top + 0, bottom + 0, z + 0 }')"
echo "  the nudges held, Left Ctrl with them: the game's own readings -- eye $h0 units above the bottom of his box standing, $h1 held, $h2 let go again, his origin $o0 / $o1 / $o2; the drawn eye $eyeStand units above his origin standing and $eyeCouch held (his origin $orgC), $log"
check $(awk -v a="$o0" -v b="$o1" -v c="$o2" 'BEGIN { exit !(a == 88 && b == 88 && c == 88) }' && echo 1 || echo 0) "view: his origin is 88 standing, crouched and let go again, with the nudges held"
check $(awk -v a="$h0" -v b="$h1" -v c="$k1" 'BEGIN { exit !(c == 1 && a - b > 17 && a - b < 19) }' && echo 1 || echo 0) "view: the crouch still lets his head 18 units down with the nudges held"
verdict
