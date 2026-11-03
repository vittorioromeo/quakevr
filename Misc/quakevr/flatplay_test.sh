#!/bin/bash
# flatplay_test.sh <agent> -- flatscreen play (-flatscreenmode; docs/vr-port/FLATPLAY.md), headless:
#   1. the hands are locked to the view: with vr_flat_sway, vr_flat_bob and vr_flat_turnlag 0, the drawn main weapon
#      in the camera's frame (vr_flatplay_debug 2's "flatview:" lines) stays within 0.05 units and degrees of
#      where it stands still while he strafes and turns (it was 23.7 units strafing, 7.5 degrees turning:
#      ROUND22.md, "Flatscreen hands: locked to the view"); with the defaults, it sways, bobs and trails the turn
#      within bounds and settles, and the shots the moves send stay on the crosshair (Misc/quakevr/flatplay_lock.py).
#   2. the mode is the player's own (.vr_flatscreen from his move): with the flag, "the player reloads 0 (flatscreen 1)";
#      without it, "reloads 1 (flatscreen 0)".
#   3. a punch from the fire buttons: +attack (what LMB is bound to) punches with the main hand and +offhandattack
#      (RMB) with the off hand when that hand holds a weapon a swing strikes with, and the punch deals the game's own
#      melee damage to what stands within the fist's arc (vr_test_spawn 5, vr_test_spawn_dist 24, and vr_test_hurt 1
#      read twice: what its line finds before its own damage is what the punch left the monster at). A gun's fire
#      button is its own shot (the knight it is aimed at takes the blast: this port refills a magazine from the
#      reserve as it empties, so the clip is not the measure) and never a punch; a chainsaw's trigger runs its chain; and a
#      hand that already punched this frame is joined by the other only when both hold a swinging weapon.
#   4. the drawn hand stays on its arm (the probe's "flatarm:" lines, vr_flatplay_debug 2): no more than 0.6 units of
#      the hand past what the arm can hold it at, through a punch and at a look straight up (it was 14 units past,
#      401 mm off the wrist, at the off hand's punch: the hand hung off its arm). The mm the probe prints with it is
#      the hand's trail behind the wrist its arm solver puts there — about one frame of the hand's own speed, so it is
#      reported, not gated: a punch throws the fist at some 250 units a second, a frame of that is 3 units.
#   5. none of it runs in the headset: vr_flat_punch says the mode is not running and "flatarm:" prints nothing.
#   6. the ironsight (vr_flat_aim, what the mouse wheel's button is, and vr_flat_aim_fov) and the free hand's grip: with
#      one hand holding a weapon that has sights, the held hand is brought onto the crosshair's ray (the muzzle's distance
#      off it, before and after the move), the field of view narrows by vr_flat_aim_fov and only while it is on (0 keeps
#      it wide, 30 narrows it further), a weapon without sights and dual wielding refuse it, and a shot fired with it on
#      still hits what the crosshair points at. The empty hand is carried to the held weapon's two-handed grip when that
#      grip stands where a second hand can reach it (the rocket launcher's, the proximity gun's: vr_twohand takes the hold,
#      helpKind 1) and left on the mode's support pose when it does not (the shotgun's stands behind the held hand). None
#      of it runs in the headset: vr_flat_aim says the mode is not running, and no ironsight or grip line prints.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
for MODE in locked feel; do
  SET=""; [ "$MODE" = "locked" ] && SET="vr_flat_sway 0; vr_flat_bob 0; vr_flat_turnlag 0;"
  # (setpos turns noclip on, noclip off again: walking. The waits are the phases' frames: strafe, stop, turn, settle.)
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; setpos 480 -352 88 0 0 0; noclip; impulse 2; $SET cl_yawspeed 360; wait30; vr_flatplay_debug 2; wait20; +moveleft; wait80; -moveleft; wait30; +right; wait60; -right; wait80; vr_flatplay_debug 0; toggleconsole; quit" -Filter "^flat(view|move):" > "$HERE/scratch/flatlock_$MODE.log" 2>&1
  while read -r line; do
    echo "$line"; case "$line" in FAIL*) fail=1;; esac
  done < <(python "$HERE/flatplay_lock.py" "$HERE/scratch/flatlock_$MODE.log" $MODE)
done
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; vr_reload_test 0; impulse 125; wait3; toggleconsole; quit" -Filter "^reload: the player" 2>&1)
check $(echo "$log" | grep -q "reloads 0 (flatscreen 1)" && echo 1 || echo 0) "flatscreen: the player's own mode, no reloading"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1; wait60; vr_reload_test 0; impulse 125; wait3; toggleconsole; quit" -Filter "^reload: the player" 2>&1)
check $(echo "$log" | grep -q "reloads 1 (flatscreen 0)" && echo 1 || echo 0) "in the headset: reloading, not flatscreen"
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; vr_test_weaponinst 0; impulse 2; wait5; impulse 2; wait5; impulse 2; wait5; impulse 120; wait3; impulse 2; wait5; impulse 120; wait3; toggleconsole; quit" -Filter "^weaponinst: main" 2>&1)
check $(echo "$log" | grep -q "main - |" && echo "$log" | grep "^weaponinst: main" | tail -1 | grep -q "main weapon" && echo 1 || echo 0) "flatscreen: cycling reaches the empty hand and comes back"
# 3 and 4: both hands' fire buttons punch, deal the damage, and stay on the arm. (The knight stands at 24, measured
# rather than guessed: 16 cannot work because the weapon rests inside the target's box there, so every contact comes back
# trace_startsolid and is handed to the wall rule instead — VR_Melee_BoxTarget only credits a monster for a rib with
# 0 < trace_fraction < 1 and trace_startsolid clear — and 40 is past what the throw can touch at all (measured 2026-11-03:
# 4 creditable knight ribs at 24, none at 16 or with the hand rested inside at 0.6 reach, no knight contact at 40). The
# waits are the phases' frames at the cadence the runs use: a punch is 0.19 s folding the arm — the hand comes to 6 units
# out from its shoulder and 7 below it, since a fist that already rests 16 units out has only the arm's 22-unit span to
# throw through — then 0.12 s of throw over some 17 of them along the arm's own line, 0.07 s holding that extension, then
# 0.22 s drawing back, so its fist is at its fastest in the middle of the throw, around wait14 after the press; the
# damage bracket below reads at wait30, past the contact, and the cycle is over by wait45.
# These two runs carry r_norefresh 0: with the refresh off the held weapon is not redrawn, and the blow line the game
# measures is the drawn weapon's, so a refreshing run measures the weapon instead of the hand. What the check is really
# up against is how finely the game sees the punch — it counts a motion only from a pose the client actually sent, and
# how often a flatscreen pose is built follows the mock's rendering. At the mock's default eyes (vr_mock_eye_size 1024)
# the client managed about 18 poses a second, four ticks apart, so a 0.12 s throw arrived as three frames of 7, 16 and
# 22 units — longer than the 3 units a blow has to find in its sweep. With the eyes at their smallest (192; the mock
# clamps it up to 256) a pose lands on nearly every tick — 539 of 764 tracked frames at 0.0139 s — the throw arrives as
# frames of 0.3 to 3.9 units and mh_fast peaks at 7.76 m/s against the 4.3 a landed fist asks for (measured 2026-11-03).
# Both melee runs set vr_mock_eye_size 256 — what the mock clamps its floor up to — so they keep measuring at that cadence
# and nobody has to re-derive it.
# Each attempt is bracketed by two vr_test_hurt 1 reads, so the gap between them is what its punch left the knight at;
# the reads in between the attempts should find nothing. Before each fresh knight the live ones are killed (vr_test_kill
# 1, twice: by the third attempt two can be standing), because vr_test_hurt reads the live monster NEAREST the player
# and a knight left over from an earlier attempt stands at the very same 24 units ahead: measured 2026-11-03, the third
# attempt's off-hand blow landed (19 to that knight) while its read reported the leftover at 75 -> 73, so the gap came
# out 0. A knight is still spawned fresh before the second and third attempt in case a blow killed the first. Measured 2026-11-03 with the punch above at 24 and the hand at its normal
# rest: the main hand left a gap of 8, the off hand 19 (melee swish: blow, hand 1 and hand 0, 1.74 of the fist's landing
# speed), so these two checks gate on the knight's health actually moving rather than being known failures. The second
# attempt drops to vr_flat_reach 0.6, kept as the other lever that was on the table: it clears the pull-back and the
# wiggle refusals on its own and lifts the grip to 8.12 m/s, but the weapon then rests inside the knight's box, so no
# rib of its sweep is creditable — which is why the settled lever is the spawn distance and not the rest offset.)
log=$HERE/scratch/flatpunch.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; r_norefresh 0; vr_mock_eye_size 256; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait10; impulse 152; wait15; vr_flatplay_debug 2; wait10; vr_test_hurt 1; wait3; +attack; wait30; screenshot; -attack; vr_test_hurt 1; wait15; vr_test_kill 1; wait10; vr_test_kill 1; wait10; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait15; vr_flat_reach 0.6; wait25; vr_test_hurt 1; wait3; +attack; wait30; screenshot; -attack; vr_test_hurt 1; wait15; vr_flat_reach 1; vr_test_kill 1; wait10; vr_test_kill 1; wait10; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait15; impulse 172; wait25; vr_test_hurt 1; wait3; +offhandattack; wait30; screenshot; -offhandattack; vr_test_hurt 1; wait15; setpos 480 -352 88 -89 0 0; wait25; +attack; wait30; screenshot; -attack; wait10; vr_flatplay_status; toggleconsole; quit" -Filter "flatarm:|test hurt:|punches:|flatplay: " -Out scratch/flatpunch.png > "$log" 2>&1
read -r okmain okoff okarm probes past mm gaps <<<"$(grep -a "" "$log" | awk '
/test hurt:/ { if (match($0, /health [0-9.]+ -> [0-9.]+/)) { split(substr($0, RSTART + 7, RLENGTH - 7), b, " ->"); if (seen) { g[++n] = prev - b[1]; s = s (n > 1 ? "," : "") sprintf("%.0f", g[n]) } prev = b[2]; seen = 1 } }
/flatarm:/ { for (i = 1; i <= NF; i++) if ($i == "past") { v = $(i + 2) + 0; if (v > past) past = v } if (match($0, /\(([0-9]+) mm\)/)) { m = substr($0, RSTART + 1, RLENGTH - 4) + 0; if (m > mm) mm = m } }
END { printf "%d %d %d %d %.2f %.0f %s\n", (g[1] > 0 || g[3] > 0), (g[5] > 0), (past <= 0.6), n, past, mm, s }')"
echo "  flatarm worst: $past units of the hand past its arm, $mm mm off its wrist; the gaps the punches left the knight at: $gaps ($log)"
check $okmain "flatscreen melee: +attack (LMB) punches with the axe in the main hand and hurts the knight in front of it"
check $okoff "flatscreen melee: +offhandattack (RMB) punches with the axe in the off hand and hurts the knight in front of it"
check $okarm "flatscreen arm: the drawn hand stays on its arm through a punch and looking straight up"
log=$HERE/scratch/flatgate.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; r_norefresh 0; vr_mock_eye_size 256; impulse 152; impulse 174; wait15; vr_flatplay_debug 2; wait10; vr_flatplay_status; +attack; +offhandattack; wait20; -attack; -offhandattack; wait10; vr_flatplay_status; impulse 154; wait15; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait5; vr_test_hurt 1; wait15; vr_flatplay_status; impulse 164; wait15; +attack; wait10; -attack; wait5; vr_flatplay_status; vr_flat_punch main; wait3; vr_flat_punch off; wait3; toggleconsole; quit" -Filter "punches:|vr_flat_punch:|flatplay: |test hurt:" > "$log" 2>&1
counts=$(grep -a "punches: the main hand" "$log" | grep -ao "main hand [0-9]*, the off hand [0-9]*" | awk '{ print $3 + 0, $7 + 0 }')
read -r s1m s1o <<<"$(echo "$counts" | sed -n 1p)"; read -r s2m s2o <<<"$(echo "$counts" | sed -n 2p)"
read -r s3m s3o <<<"$(echo "$counts" | sed -n 3p)"; read -r s4m s4o <<<"$(echo "$counts" | sed -n 4p)"
shot=$(grep -a 'test hurt:' "$log" | awk '{ if (match($0, /health [0-9]+ -> [0-9]+/)) { split(substr($0, RSTART + 7, RLENGTH - 7), b, " -> "); if (seen) { print prev - b[1]; exit } prev = b[2]; seen = 1 } }')
echo "  punches after each step: $s1m/$s1o then $s2m/$s2o then $s3m/$s3o then $s4m/$s4o; the shotgun's blast left the knight $shot units of damage ($log)"
check $([ "${s2m:-0}" -gt "${s1m:-0}" ] && [ "${s2o:-0}" = "${s1o:-0}" ] && echo 1 || echo 0) "flatscreen melee: the other hand's fire button does not disturb a hand that already punched, unless both hold a swinging weapon"
check $([ "${shot:-0}" -gt 0 ] && [ "$s3m" = "$s2m" ] && [ "$s3o" = "$s2o" ] && echo 1 || echo 0) "flatscreen melee: a gun's fire button is its own shot (the knight it is aimed at takes the blast) and not a punch"
check $([ "$s4m" = "$s3m" ] && [ "$s4o" = "$s3o" ] && grep -q "does not strike with" "$log" && echo 1 || echo 0) "flatscreen melee: a chainsaw's trigger runs its chain, and vr_flat_punch says so"
# 5: in the headset none of this is running.
log=$HERE/scratch/flatvr.log
bash $KIT/run.sh $AGENT -Script "map e1m1; wait60; developer 1; vr_flatplay_debug 2; wait10; vr_flat_punch main; wait3; vr_flat_punch off; wait5; +attack; wait10; -attack; wait10; vr_flatplay_status; toggleconsole; quit" -Filter "vr_flat_punch:|flatarm:|flatscreen play|punches:" > "$log" 2>&1
check $(grep -q "vr_flat_punch: flatscreen play is not running" "$log" && ! grep -q "flatarm:" "$log" && grep -q "the mode off" "$log" && echo 1 || echo 0) "in the headset: vr_flat_punch does nothing, the arm probe prints nothing, the mode is off"
# 6 to 9: the ironsight (vr_flat_aim, what the mouse wheel's button is; vr_flat_aim_fov) and the free hand carried to
# the held weapon's grip (docs/vr-port/FLATPLAY.md, "The ironsight, and the free hand's grip"). The fov numbers come
# from vr_flatplay_debug 2's "flatsight:" lines (the field of view the view is drawing, and the muzzle's distance off
# the crosshair's ray before and after the held hand's move, with vr_flat_aim_fov's value at the line's end); the grip
# measures come from vr_flatplay_status.
maxfov() { awk '{ if (match($0, / fov [0-9.]+/)) { f = substr($0, RSTART + 5, RLENGTH - 5) + 0; if (f > best) best = f } } END { printf "%.2f", best + 0 }'; }
# The field of view is read at the last record of a class: the frame a change is asked for still reports the value from
# before it (the pose and its probe run before the view is made), so a class's first line or two are stale.
lastfov() { awk '{ if (match($0, / fov [0-9.]+/)) { f = substr($0, RSTART + 5, RLENGTH - 5) + 0; if (f > 0) last = f } } END { printf "%.2f", last + 0 }'; }
# The console wraps a long line, so a "flatsight:" record is read as that line plus the lines that follow it.
flatrecs() { awk '{ if ($0 ~ /^flatsight: /) { if (rec != "") print rec; rec = $0 } else if (rec != "") { rec = rec " " $0 } } END { if (rec != "") print rec }' "$1"; }
log=$HERE/scratch/flatads.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; vr_mock_eye_size 256; vr_flatplay_debug 2; impulse 154; wait25; vr_flat_aim 1; wait25; vr_flat_aim_fov 30; wait25; vr_flat_aim_fov 0; wait25; vr_flat_aim_fov 12; wait25; impulse 172; wait30; vr_flatplay_status; toggleconsole; quit" -Filter "^flatsight: " > "$log" 2>&1
wide=$(flatrecs "$log" | grep -a "ads off" | maxfov)
on12=$(flatrecs "$log" | grep -a "fov take 12" | lastfov)
on30=$(flatrecs "$log" | grep -a "fov take 30" | lastfov)
on0=$(flatrecs "$log" | grep -a "fov take 0" | lastfov)
refused=$(flatrecs "$log" | grep -a "ads asked for" | lastfov)
read -r offbefore offafter handmoved <<<"$(flatrecs "$log" | grep -a "fov take 12" | tail -1 | awk '{
  if (match($0, /crosshair [0-9.]+ -> [0-9.]+/)) { split(substr($0, RSTART + 10, RLENGTH - 10), m, " -> "); b = m[1] + 0; a = m[2] + 0 }
  if (match($0, /moved [0-9.]+/)) { mv = substr($0, RSTART + 6, RLENGTH - 6) + 0 }
 } END { printf "%.2f %.2f %.2f", b, a, mv }')"
echo "  muzzle off the crosshair's ray $offbefore units before the held hand's move, $offafter after it ($handmoved units of move); field of view $wide plain, $on12 with the ironsight on (vr_flat_aim_fov 12), $on30 at 30, $on0 at 0, $refused while dual wielding ($log)"
check $(awk -v b="$offbefore" -v a="$offafter" -v m="$handmoved" 'BEGIN { exit !(b > 0 && a > 0 && m > 0 && b - a >= 2 && m >= 2) }' && echo 1 || echo 0) "flatscreen aim: the ironsight brings the held weapon onto the crosshair's ray (the muzzle off it, before and after the move)"
check $(awk -v w="$wide" -v o="$on12" 'BEGIN { exit !(w > 0 && o > 0 && w - o >= 8 && w - o <= 16) }' && echo 1 || echo 0) "flatscreen aim: the field of view narrows by vr_flat_aim_fov (12) while the ironsight is on"
check $(awk -v w="$wide" -v o="$on30" 'BEGIN { exit !(w > 0 && o > 0 && w - o >= 24) }' && echo 1 || echo 0) "flatscreen aim: vr_flat_aim_fov 30 narrows it by more; vr_flat_aim_fov is the whole of it"
check $(awk -v w="$wide" -v o="$on0" 'BEGIN { exit !(w - o <= 1.5 && w - o >= -1.5) }' && echo 1 || echo 0) "flatscreen aim: vr_flat_aim_fov 0 leaves the view as wide as it is with the ironsight off"
check $(grep -q "nothing held one-handed" "$log" && awk -v w="$wide" -v r="$refused" 'BEGIN { exit !(r - w <= 1.5 && r - w >= -1.5) }' && echo 1 || echo 0) "flatscreen aim: dual wielding is posed as it was and its field of view is untouched, and the ironsight is refused"
# 7: the shot. The knight stands where the crosshair points it; the ironsight is on when the gun is fired (the pair of
# vr_test_hurt 1 reads brackets the damage, as in the check above).
log=$HERE/scratch/flatadsshot.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; r_norefresh 0; vr_mock_eye_size 256; impulse 154; wait15; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_flatplay_debug 2; vr_flat_aim 1; wait25; vr_test_hurt 1; wait3; +attack; wait10; -attack; wait5; vr_test_hurt 1; wait10; toggleconsole; quit" -Filter "test hurt:|^flatsight: " > "$log" 2>&1
shot=$(grep -a 'test hurt:' "$log" | awk '{ if (match($0, /health [0-9.]+ -> [0-9.]+/)) { split(substr($0, RSTART + 7, RLENGTH - 7), b, " ->"); if (seen) { print prev - b[1]; exit } prev = b[2]; seen = 1 } }')
adsline=$(grep -a "ads on" "$log" | head -1)
echo "  with the ironsight on the knight takes $(awk -v s="${shot:-0}" 'BEGIN { printf "%.1f", s + 0 }') units of damage; $adsline ($log)"
check $(awk -v s="${shot:-0}" -v n="$(grep -c "ads on" "$log")" 'BEGIN { exit !(s + 0 > 0 && n > 0) }' && echo 1 || echo 0) "flatscreen aim: with the ironsight on the shot still hits what the crosshair points at"
# 8: the free hand and the held weapon's grip. The shotgun's hotspot (measured here, and the same in the headset's view:
# 5 units off the held hand, 3.2 to its rear and 3.8 below it) is not a place a second hand can take, so the mode keeps
# its support pose; the rocket launcher's and the proximity gun's stand 12.5 and 12.8 units ahead of it, and vr_twohand
# takes the hold there (its own goodDistance, goodDot and foregrip stickiness decide it; vr_flatsight only supplies the
# press a flatscreen hand is always making).
log=$HERE/scratch/flatgrip.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; vr_mock_eye_size 256; vr_debug_2h_grip 1; impulse 154; wait40; vr_flatplay_status; wait5; impulse 158; wait40; vr_flatplay_status; wait5; impulse 159; wait60; vr_flatplay_status; toggleconsole; quit" -Filter "^  two hands:|^  grip:|^  hotspot:|^2h grip: " > "$log" 2>&1
grep -a "^  two hands:|^  hotspot:" "$log" | sed 's/^/  /'
check $(grep -q "off hand where the mode's pose puts it" "$log" && awk '/hotspot: / { if (match($0, /fwd -[0-9.]+/)) behind = 1 } END { exit !behind }' "$log" && echo 1 || echo 0) "flatscreen grip: a grip standing behind the held hand (the shotgun's) is left alone: the free hand stays on the mode's support pose, and nothing is folded onto the held hand"
check $(grep -q "on its grip hotspot (held holding: helpKind 1" "$log" && grep -q "2h grip: off hand took it" "$log" && echo 1 || echo 0) "flatscreen grip: a grip ahead of the held hand (the rocket launcher's, the proximity gun's) is taken: helpKind 1, the transition in full, on vr_twohand's own rules"
# 9: in the headset the ironsight is not running at all: vr_flat_aim says so, the ironsight and the grip lines never
# print, and nothing of the mode's prints either (VR_FlatAdsFov hands gl_screen.c back the field of view it was asked
# for: the headset's own, whatever vr_flat_aim_fov says).
log=$HERE/scratch/flatadsinheadset.log
bash $KIT/run.sh $AGENT -Script "map e1m1; wait60; developer 1; vr_flatplay_debug 2; vr_flat_aim_fov 30; vr_flat_aim 1; wait20; vr_flat_aim; wait5; vr_flatplay_status; toggleconsole; quit" -Filter "^flatsight:|Flatscreen aim:|flatscreen play:|  ironsight:|  two hands:|^2h grip: " > "$log" 2>&1
check $(grep -q "Flatscreen aim: flatscreen play only" "$log" && grep -q "the mode off" "$log" && ! grep -q "^flatsight:" "$log" && ! grep -q "  ironsight:" "$log" && ! grep -q "^  two hands:" "$log" && echo 1 || echo 0) "in the headset: vr_flat_aim says flatscreen play is not running, and no ironsight or grip line prints"
verdict
exit $fail
