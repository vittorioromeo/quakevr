#!/bin/bash
# flatpose_test.sh <agent> -- the numbers this measures, in one place (docs/vr-port/FLATPLAY.md, "Where the weapon and
# the hands sit" and "Where the camera stands"). Five headless flatscreen runs (-flatscreenmode, -Low), each of them at
# vr_mock_eye_size 256, r_norefresh 0, developer 1, vr_flatplay_debug 2, standing at e1m1's 480 -352 88 with a look due
# east and no pitch, the shotgun in the main hand (impulse 154), and prints what it read rather than gating it:
#   a. the defaults: the muzzle's distance off the crosshair's ray with the ironsight off, then with it settled on, and
#      the field of view it settles at (vr_flat_aim_fov 12);
#   b. the hipfire pose moved (vr_flat_gun_ahead 26 vr_flat_gun_down 4 vr_flat_gun_out 8 vr_flat_gun_near 14): the muzzle's
#      place, and the direction the shots go (flatmove:'s degrees off the view), which the pose is not allowed to move;
#   c. the ironsight's own numbers moved (vr_flat_ads_relief 16, the picture at vr_flat_ads_screen_x 2 _y -2): the muzzle's distance off
#      the ray, and the damage to a knight standing on the crosshair 24 units away (not forward: a hand further forward
#      stands past his middle there, and a shot starts beyond him; he is the floor's edge away from falling further off);
#   d. the eye nudged (vr_flat_cam_fwd 12 vr_flat_cam_up 6 vr_flat_cam_side 4) with the sight on: the muzzle's distance
#      off the ray the nudged camera reads, the same knight's damage, the drawn eye's height, and his own origin;
#   e. the same nudges with Left Ctrl held (+flatcrouch, the crouch suite's way): his origin, which must not move, and
#      the eye's height, which is the nudges on top of the crouch drop.
#   f. the crosshair's point against the server's shot, at offsets and with the eye nudged, near and far: in vrtesthall, from
#      its start, a knight at 64 and at 300, the far wall (the shot lines of vr_debug_shots against the flataim: lines' point),
#      and with vr_flat_aim_point 0 the control: the same offsets, and the shot misses.
#   g. the near plane: a gun held at the camera, as shipped against the desktop's plane (vr_nearclip 4, vr_nearclip_held 0).
# Each run's log is kept under Misc/quakevr/scratch. Prints the readings; a PASS/FAIL line for the two things that must
# hold whatever these are set to: the shot goes through what the crosshair is on, and his origin does not move.
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
shotdeg() { grep -a "^flataim: " "$1" | tail -n +8 | awk '{ if (match($0, /off its point [0-9.]+/)) { d = substr($0, RSTART + 14, RLENGTH - 14) + 0; if (d > best) best = d; n++ } } END { printf "%.4f %d", best + 0, n + 0 }'; }
# How far the last record's muzzle is off the ray against what the sight's place asks (its "asked" number), then its distance.
offasked() { awk '{ for (i = 1; i <= NF; i++) { if ($i == "crosshair") a = $(i+3) + 0; if ($i == "asked") k = $(i+1) + 0 } } END { d = a - k; printf "%.2f %.2f", (d < 0 ? -d : d), a + 0 }'; }
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
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; wait30; vr_flat_aim 1; wait40; vr_flat_aim 0; wait30; wait; tune_reset; toggleconsole; quit" -Filter "^flatsight: " > "$log" 2>&1
read -r off0 off1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads off" | tail -4 | muzzle)"
read -r on0 on1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads on" | tail -4 | muzzle)"
echo "  defaults: the muzzle $off0 units off the crosshair's ray with the ironsight off, $on0 before the held hand's move and $on1 after it with the sight settled on; field of view $(maxfov "$log") plain, $(lastfovOn "$log") settled with the sight on (vr_flat_aim_fov 12), $log"

# b. the hipfire pose moved, the ironsight off so the measure is the pose's own.
log=$HERE/scratch/pose_gun.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; wait30; tune vr_flat_gun_ahead 26; tune vr_flat_gun_down 4; tune vr_flat_gun_out 8; tune vr_flat_gun_near 14; wait40; vr_flatplay_status; wait; tune_reset; toggleconsole; quit" -Filter "^flatsight: |^flataim: |^  where the weapon" > "$log" 2>&1
read -r g0 g1 <<<"$(recs "$log" | grep -a "^flatsight: " | grep -a "ads off" | tail -4 | muzzle)"
deg=$(shotdeg "$log")
grep -a "^  where the weapon" "$log" | head -1 | sed 's/^/  status:/' | cut -c1-400
echo "  vr_flat_gun_ahead 26 vr_flat_gun_down 4 vr_flat_gun_out 8 vr_flat_gun_near 14: the muzzle $g0 units off the crosshair's ray (it was $off0 at the defaults); the shots over the run at most ${deg% *} deg off the point the crosshair is on, over ${deg#* } records, $log"
check $(awk -v d="${deg% *}" -v n="${deg#* }" 'BEGIN { exit !(n > 20 && d <= 0.1) }' && echo 1 || echo 0) "pose: moved, and every shot still points at what the crosshair is on"

# c. the ironsight's own numbers moved, onto a knight standing on the crosshair 24 units away. Three shots, each at a fresh
# knight: moved, then at the defaults (the control), then the eye relief alone, far out.
log=$HERE/scratch/pose_ads.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; vr_flat_aim 1; wait30; tune vr_flat_ads_relief 16; tune vr_flat_ads_screen_x 2; tune vr_flat_ads_screen_y -2; wait20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait5; vr_test_kill 1; tune_reset vr_flat_ads_relief; tune vr_flat_ads_screen_x 0; tune vr_flat_ads_screen_y 0; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait5; vr_test_kill 1; tune vr_flat_ads_relief 20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait10; wait; tune_reset; toggleconsole; quit" -Filter "test spawn: |test hurt: |^flatsight: |^flataim: |^  ironsight: " > "$log" 2>&1
# The three shots' sights, each the last settled "ads on" record before its group's vr_flatplay_status line (the
# printed relief is the solved one, scaled by the fov and the arm: it never reads as the cvar's 16 or 20, and a grep on
# "relief 16.00" read nothing and passed on the empty value).
group() { awk -v g="$2" '
    function flush() { if (rec != "" && rec ~ / ads on / && grp == g) last = rec; rec = "" }
    /^flatsight: / { flush(); rec = $0; next }
    /^  ironsight: / { flush(); grp++; next }
    /^[a-z]+( [a-z]+)?: / { flush(); next }
    { if (rec != "") rec = rec " " $0 }
    END { flush(); print last }' "$1"; }
read -r a0 a1 <<<"$(group "$log" 0 | offasked)"
read -r c0 c1 <<<"$(group "$log" 1 | offasked)"
read -r w0 w1 <<<"$(group "$log" 2 | offasked)"
nsight=$(for g in 0 1 2; do group "$log" $g; done | grep -ac "asked")
deg=$(shotdeg "$log"); read -r s1 s2 s3 <<<"$(gaps "$log")"
echo "  vr_flat_ads_relief 16 and the picture moved: the muzzle $a1 units off the crosshair's ray ($a0 off what its place asks), the knight on the crosshair took $s1 units; the same sight at the defaults: $c1 off it ($c0 off what is asked), $s2 units; vr_flat_ads_relief 20 alone: $w1 off it ($w0 off what is asked), $s3 units; the shots at most ${deg% *} deg off the point the crosshair is on, over ${deg#* } records, $log"
check $(awk -v d="${deg% *}" -v n="${deg#* }" 'BEGIN { exit !(n > 20 && d <= 0.1) }' && echo 1 || echo 0) "pose: the ironsight's numbers, and every shot still points at what the crosshair is on"
check $(awk -v a="$a0" -v c="$c0" -v w="$w0" -v o="$w1" -v s1="$s1" -v s2="$s2" -v s3="$s3" -v n="$nsight" 'BEGIN { exit !(n == 3 && a <= 0.5 && c <= 0.5 && w <= 0.5 && s2 + 0 > 0) }' && echo 1 || echo 0) "pose: the muzzle stands where the solved gun's does, and the foe at 24 is hit by the shipped sight (the moved ones printed: the gun's muzzle stands inside his box there; section f gates moved sights at 64 and 300)"

# d. the eye nudged, with the sight on: one shot at a fresh knight 24 units down the line the game aims along before the
#    nudges, and one at another after them, printed: their 12 forward carries the gun past a knight at 24. Section f gates
#    the shot with the eye nudged.
log=$HERE/scratch/pose_cam.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; vr_flatplay_debug 2; vr_flat_aim 1; wait20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait5; vr_test_kill 1; tune vr_flat_cam_fwd 12; tune vr_flat_cam_up 6; tune vr_flat_cam_side 4; vr_test_spawn 5; vr_test_spawn_dist 64; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait10; vr_test_hurt 1; wait10; vr_flatplay_status; wait; tune_reset; toggleconsole; quit" -Filter "^flatsight: |^flatcam: |test spawn: |test hurt: |^  ironsight: " > "$log" 2>&1
read -r eye0 org0 nx ny nz <<<"$(recs "$log" | grep -a "^flatcam: " | tail -4 | camread)"
# (k: what the sight's place asks the muzzle to be off the ray, "asked": the bore stands under the sights, by the model's
# own sight line -- the shotgun's 0.98 became 1.02 when its irons' tops became the aim line, c91b135b6 -- so the muzzle
# is gated against that, not against a fixed 1 unit.)
read -r n0b n0a n1b n1a n1k <<<"$(recs "$log" | awk '
    /^flatcam: / { for (i = 1; i <= NF; i++) if ($i == "nudge") { nudged = $(i+1) + 0 != 0 || $(i+2) + 0 != 0 || $(i+3) + 0 != 0 }
                  next }
    /^flatsight: / { if ($0 !~ / ads on /) next
                  for (i = 1; i <= NF; i++) { if ($i == "crosshair") { b = $(i+1) + 0; a = $(i+3) + 0 } if ($i == "asked") k = $(i+1) + 0 }
                  if (nudged) { b2 = b; a2 = a; k2 = k } else { b1 = b; a1 = a } }
    END { printf "%.2f %.2f %.2f %.2f %.2f", b1 + 0, a1 + 0, b2 + 0, a2 + 0, k2 + 0 }')"
read -r dmg0 dmg1 <<<"$(gaps "$log")"
echo "  vr_flat_cam_fwd 12 vr_flat_cam_up 6 vr_flat_cam_side 4: the drawn eye $eye0 units above his origin, the nudges read as $nx $ny $nz in the world at this look, his origin $org0; the muzzle $n0a units off the crosshair's ray with the sight settled before the nudges and $n1b / $n1a after them (the sight asks $n1k: the bore under the sights); the knight on the crosshair took ${dmg0:-no read} units from the shot before the nudges and ${dmg1:-no read} from the one after them, $log"
check $(awk -v o="$org0" 'BEGIN { exit o == 88 }' && echo 1 || echo 0) "view: the nudges move the drawn eye and leave his origin at 88"
check $(awk -v e="$eye0" -v x="$nx" -v y="$ny" -v z="$nz" 'BEGIN { exit !(e > 27 && e < 29 && x == 12 && y == 4 && z == 6) }' && echo 1 || echo 0) "view: at this look the nudges read as 12 forward, 4 to the world's east and 6 up"
check $(awk -v b="$n1b" -v a="$n1a" -v k="$n1k" 'BEGIN { d = a - k; exit !((d < 0 ? -d : d) <= 0.25 && k < 2 && b > 4) }' && echo 1 || echo 0) "view: the sight closes the muzzle back onto the nudged camera's crosshair ray (to what the bore under the sights asks)"
check $(awk -v d0="${dmg0:-0}" 'BEGIN { exit !(d0 + 0 > 0) }' && echo 1 || echo 0) "view: the foe standing on the crosshair is hit by the shot fired before the nudges"

# e. the same nudges with Left Ctrl held: the crouch comes down on top of them. The game's own report (vr_flatkeys_test 1)
# is the one that says whether the *player* moved: its eye is his head above the bottom of his box (headpos_z - absmin_z)
# and its origin is the server's player.origin_z.
log=$HERE/scratch/pose_camcrouch.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$P; wait60; tune vr_flat_cam_fwd 12; tune vr_flat_cam_up 6; tune vr_flat_cam_side 4; wait30; vr_flatplay_debug 2; wait20; vr_flatkeys_test 1; wait10; +flatcrouch; wait40; vr_flatkeys_test 1; wait10; -flatcrouch; wait30; vr_flatkeys_test 1; wait10; wait; tune_reset; toggleconsole; quit" -Filter "^flatcam: |^flatkeys: crouched" > "$log" 2>&1
fk() { grep -a "^flatkeys: crouched" "$log" | sed -n "${1}p" | awk '{ for (i = 1; i <= NF; i++) { if ($i == "crouched") c = $(i+1) + 0; if ($i == "eye") e = $(i+1) + 0; if ($i == "origin") o = $(i+1) + 0 } printf "%.1f %.1f %.1f", c + 0, e + 0, o + 0 }'; }
read -r k0 h0 o0 <<<"$(fk 1)"; read -r k1 h1 o1 <<<"$(fk 2)"; read -r k2 h2 o2 <<<"$(fk 3)"
read -r eyeStand eyeCouch orgC <<<"$(recs "$log" | grep -a "^flatcam: " | awk '
    { for (i = 1; i <= NF; i++) { if ($i == "above") h = $(i+2) + 0; if ($i == "origin") z = $(i+3) + 0 }
      if (h > top) top = h; if (bottom == 0 || h < bottom) bottom = h }
    END { printf "%.1f %.1f %.1f", top + 0, bottom + 0, z + 0 }')"
echo "  the nudges held, Left Ctrl with them: the game's own readings -- eye $h0 units above the bottom of his box standing, $h1 held, $h2 let go again, his origin $o0 / $o1 / $o2; the drawn eye $eyeStand units above his origin standing and $eyeCouch held (his origin $orgC), $log"
check $(awk -v a="$o0" -v b="$o1" -v c="$o2" 'BEGIN { exit !(a == 88 && b == 88 && c == 88) }' && echo 1 || echo 0) "view: his origin is 88 standing, crouched and let go again, with the nudges held"
check $(awk -v a="$h0" -v b="$h1" -v c="$k1" 'BEGIN { exit !(c == 1 && a - b > 17 && a - b < 19) }' && echo 1 || echo 0) "view: the crouch still lets his head 18 units down with the nudges held"

# f. The crosshair's point against the shot the server fires, near and far. From the test hall's start, its flat floor
# ahead: for each of the three aims -- the sight moved well off the ray, the eye nudged with the sight on, and
# hipfire -- a shot at a knight 64 away, one at a knight 300 away, and one at the wall. vr_debug_shots' line says where the
# server's shot left from and along what; the last flataim: line says what the crosshair was on. Then the control: the first
# aim's offsets with vr_flat_aim_point 0, a shot along the view, at the knight at 64.
# Two things keep it the same every run (2026-11-09): the shots come from the reserve (vr_flat_reload 0, put back at the
# end): with the clips of 2026-11-08 the shotgun's 8 ran dry before the hipfire wall and the control, which never fired;
# and the look is 2 degrees down (setpos at the hall's start), so the crosshair crosses the knight at 300 at his chest:
# level, at the eye's 22 units, it grazed the top of his animated head, and his idle frames took it off him now and then
# (the hipfire and nudged-eye knight at 300 failing at random). The wall at 900 is still the wall (the floor is 1300 away).
AT="tune vr_flat_sway 0; tune vr_flat_bob 0; tune vr_flat_turnlag 0; vr_flat_reload 0; setpos 0 -288 25 2 90 0"
K() { echo "vr_test_spawn 5; vr_test_spawn_dist $1; impulse 241; wait25; +attack; wait3; -attack; wait30; vr_test_kill 1; wait10"; }
W="wait10; +attack; wait3; -attack; wait30"
ALL="$(K 64); $(K 300); $W"
log=$HERE/scratch/pose_point.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "map vrtesthall; wait60; god; notarget; developer 1; r_norefresh 0; vr_mock_eye_size 256; vr_debug_shots 1; impulse 154; $AT; wait60; vr_flatplay_debug 2; vr_flat_aim 1; tune vr_flat_ads_screen_y -1; tune vr_flat_ads_screen_x 1; wait40; $ALL; tune vr_flat_ads_screen_y 0; tune vr_flat_ads_screen_x 0; tune vr_flat_cam_fwd 12; tune vr_flat_cam_up -6; tune vr_flat_cam_side 4; wait30; $ALL; tune vr_flat_cam_fwd 0; tune vr_flat_cam_up 0; tune vr_flat_cam_side 0; vr_flat_aim 0; wait40; $ALL; vr_flat_aim 1; tune vr_flat_ads_screen_y -15; tune vr_flat_ads_screen_x 25; tune vr_flat_aim_point 0; wait40; $(K 64); tune_reset; vr_flat_reload 1; toggleconsole; quit" -Filter "^flataim: main|^shot: from" > "$log" 2>&1
# One line a shot: how far the point stands off the line it left on, what was at that point, how far, what the pellets hit.
# (A flataim: record whose point is under 1 unit away is skipped: the crosshair's ray started inside something at his
# head for that frame -- seen for a few frames now and then on next 2026-11-09, the server's shot unaffected -- and it says
# nothing of what the crosshair is on. Their count is printed.)
shots() { grep -aE "^(flataim: main|shot: from)" "$1" | tr -d "'" | awk '
    /^flataim:/ { for (i = 1; i <= NF; i++) if ($i == "away") aw = $(i+1) + 0; if (aw < 1) next
                  for (i = 1; i <= NF; i++) { if ($i == "at") { x = $(i+1); y = $(i+2); z = $(i+3) } if ($i == "away") a = $(i+1) } next }
    /^shot:/ { sx = $3; sy = $4; sz = $5; dx = $7; dy = $8; d3 = $9; sub(":", "", d3)
      t = (x - sx) * dx + (y - sy) * dy + (z - sz) * d3
      ex = x - sx - dx * t; ey = y - sy - dy * t; ez = z - sz - d3 * t
      on = $10; printf "%.2f %.0f %d\n", sqrt(ex * ex + ey * ey + ez * ez), a, on }'; }
shots "$log" > "$HERE/scratch/pose_point.txt"
n=0; bad=0; names="sight-near sight-mid sight-far eye-near eye-mid eye-far hip-near hip-mid hip-far control"
pointline() { sed -n "$1p" "$HERE/scratch/pose_point.txt"; }
i=0
for name in $names; do
  i=$((i + 1)); read -r off away on <<<"$(pointline $i)"
  echo "  $name: the crosshair on something ${away:-(no shot)} away, ${off:-(no shot)} units off the line the server's shot left on, ${on:-no} pellets on the knight"
done
echo "  $(grep -ac . "$HERE/scratch/pose_point.txt") shots read of the 10 fired; $(grep -ac "^flataim: main.* away 0\.[0-9] " "$log") crosshair records skipped (the ray started inside something), $log"
gate() { read -r off away on <<<"$(pointline $1)"; awk -v o="${off:-999}" -v a="${away:-0}" -v p="${on:-0}" -v w="$2" -v k="$3" 'BEGIN { exit !(o <= 1.5 && a > w * 0.5 && (k == 0 || p + 0 > 0)) }' && echo 1 || echo 0; }
check $(gate 1 48 1) "point: the picture moved, a knight 64 away: the server's shot goes through the crosshair's point, and hits him"
check $(gate 2 280 1) "point: the picture moved, a knight 300 away: the same"
check $(gate 3 500 0) "point: the picture moved, the far wall: the server's shot goes through the crosshair's point"
check $(gate 4 40 1) "point: the eye nudged, a knight 64 away: the same"
check $(gate 5 270 1) "point: the eye nudged, a knight 300 away: the same"
check $(gate 6 500 0) "point: the eye nudged, the far wall: the same"
check $(gate 7 48 1) "point: hipfire, a knight 64 away: the same"
check $(gate 8 280 1) "point: hipfire, a knight 300 away: the same"
check $(gate 9 500 0) "point: hipfire, the far wall: the same"
read -r coff caway con <<<"$(pointline 10)"
check $(awk -v o="${coff:-0}" -v p="${con:-1}" 'BEGIN { exit !(o > 8) }' && echo 1 || echo 0) "point: the control: the picture moved further, the shot along the view, and it passes more than 8 units off the crosshair's point"

# g. The near plane at a gun held at the camera: the sight at its nearest eye relief, pinned here ($CLOSE) -- the case came from the pose his
# 2026-11-03 tuning shipped (vr_flat_gun_ahead 3.5 with the eye nudged 3.5 forward and up, which puts the gun at the drawn
# eye). His 2026-11-04 tuning ships gun_fwd 5 with no nudge, and that pose leaves the gun well out from the eye: the case
# keeps its own numbers rather than following the defaults, because what it measures is a gun standing at the near plane.
# Two frames at the pinned pose, and
# one with vr_nearclip 4 and vr_nearclip_held 0, the desktop's plane this mode drew at. Without the mode's near plane
# (VR_FlatNearPlane) vr_nearclip does nothing here: the third frame reads as the first and the check fails.
log=$HERE/scratch/pose_near.log
CLOSE="tune vr_flat_ads_relief 3; tune vr_flat_aim_fov 40; tune vr_flat_aim_time 0.15; tune vr_flat_cam_fwd 3.5; tune vr_flat_cam_up 3.5; tune vr_flat_bob 0"
bash $KIT/run.sh $AGENT -Low -Clean -ExtraArgs "-flatscreenmode" -Script "$P; wait60; $CLOSE; vr_flat_aim 1; wait40; vr_flatplay_status; screenshot; wait2; screenshot; vr_nearclip 4; vr_nearclip_held 0; wait2; screenshot; toggleconsole; quit" -Full -Filter "near plane" > "$log" 2>&1
SHOTS="$KIT/bases/$AGENT/qbase/quakevr/screenshots"
mapfile -t near < <(ls -tr "$SHOTS"/*.png | tail -3)
mad() { python "$HERE/pixdiff_ab.py" "$1" "$2" | grep -a -o "per channel [0-9.]*" | awk '{ print $3 }'; }
same=$(mad "${near[0]}" "${near[1]}"); cut=$(mad "${near[0]}" "${near[2]}")
grep -a "near plane:" "$log" | head -1 | sed 's/^/  status:/' | cut -c1-120
echo "  a gun at the camera: two frames at the pinned pose differ by $same a channel, the desktop's near plane against them by $cut"
check $(grep -aq "near plane: 0.1 units (vr_nearclip), float depth" "$log" && echo 1 || echo 0) "near: the mode's near plane is the headset's, on float depth"
check $(awk -v s="${same:-99}" -v c="${cut:-0}" 'BEGIN { exit !(s <= 0.5 && c >= 3) }' && echo 1 || echo 0) "near: the gun at the camera is not cut as the desktop's near plane cuts it"
verdict; exit $fail
