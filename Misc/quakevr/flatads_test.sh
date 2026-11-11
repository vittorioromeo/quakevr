#!/bin/bash
# flatads_test.sh <agent> -- flatscreen ironsight items of 2026-11-05 (docs/vr-port/ROUND24.md, "Flat pose: grips, ADS recoil,
# the sights", agent/flatsolve6). Fast mode; each run ends with its own script.
#   a. recoil: a shotgun shot hipfire, then sighted: the drawn recoil sighted is vr_flat_ads_recoil (0.1) of hipfire's.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
export QVR_AUTHOR_WAIT_ALWAYS=1
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
P="map e1m1; god; notarget; developer 1; r_norefresh 0; setpos 480 -352 88 0 0 0"
SECTIONS=${SECTIONS:-abcde}  # every section (one run each); SECTIONS=a etc. for one

if [[ $SECTIONS == *a* ]]; then
  log=$HERE/scratch/flatads_a.log
  SHOT="vr_flat_recoil_probe; +attack; wait4; -attack; wait50; vr_flat_recoil_probe;"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait40; $SHOT vr_flat_aim 1; wait60; $SHOT vr_flat_aim 0; wait30; toggleconsole; quit" -Filter "^flatrecoil" -Timeout 60 > "$log" 2>&1
  hip=$(grep -a "^flatrecoil" "$log" | sed -n 2p); ads=$(grep -a "^flatrecoil" "$log" | sed -n 4p)
  echo "  hipfire: $hip"; echo "  sighted: $ads"
  hp=$(awk '{ print $3 }' <<<"$hip"); ha=$(awk '{ print $5 }' <<<"$hip"); ap=$(awk '{ print $8 }' <<<"$ads"); aa=$(awk '{ print $10 }' <<<"$ads")
  check $(awk -v p="${hp:-0}" -v a="${ha:-0}" 'BEGIN { exit !(p > 0.1 && a > 0.1) }' && echo 1 || echo 0) "recoil: a hipfire shot kicks the drawn gun ($hp u, $ha deg)"
  check $(awk -v hp="${hp:-0}" -v ha="${ha:-0}" -v ap="${ap:-9}" -v aa="${aa:-9}" 'BEGIN { exit !(hp > 0 && ha > 0 && ap / hp > 0.08 && ap / hp < 0.12 && aa / ha > 0.08 && aa / ha < 0.12) }' && echo 1 || echo 0) "recoil: sighted it kicks 0.1 of that (vr_flat_ads_recoil) ($ap u, $aa deg)"
fi

if [[ $SECTIONS == *b* ]]; then
  # b. the fingers: the holding hand grips (middle, ring, pinky closed: 5; the index on the trigger: 2.5), hipfire and sighted,
  #    and the free hand on the two-handed hotspot grips too (index closed with the grip).
  log=$HERE/scratch/flatads_b.log
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait60; vr_dumpview; vr_flat_aim 1; wait60; vr_dumpview; vr_flat_aim 0; wait30; toggleconsole; quit" -Filter "hand: trigger" -Timeout 60 > "$log" 2>&1
  grep -a "hand: trigger" "$log" | sed 's/^/  /'
  # Middle, ring and pinky (fields 12-14) and the index (11) per line: main hipfire, off hipfire, main sighted, off sighted.
  ok=$(grep -a "hand: trigger" "$log" | awk '{ if ($12 >= 4.9 && $13 >= 4.9 && $14 >= 4.9 && $11 >= 2.4) n++ } END { print n + 0 }')
  check $([ "$ok" = "4" ] && echo 1 || echo 0) "fingers: both hands grip the shotgun, hipfire and sighted ($ok of 4 hands closed)"
fi

if [[ $SECTIONS == *c* ]]; then
  # c. the grenade and rocket launchers at the others' height: hipfire, level, the muzzle's pitch on the screen and the hand's height
  #    within 2 degrees and 1.5 units of the shotgun's (they take their two-handed grip for real, and missed the stance's lift).
  log=$HERE/scratch/flatads_c.log
  S=""; for i in 4 8 10; do S="$S vr_test_give_wid $i; impulse 149; wait50; vr_flat_sight_check;"; done
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; wait30; $S toggleconsole; quit" -Filter "^flatgunview" -Timeout 60 > "$log" 2>&1
  grep -a "^flatgunview" "$log" | awk '{ gsub(/[(]/, "", $15); printf "  %s: hand up %s, muzzle %s deg on the screen\n", $2, $8, $15 }'
  ok=$(grep -a "^flatgunview" "$log" | awk '{ gsub(/[(]/, "", $15); if (NR == 1) { z = $8; d = $15; next } dz = $8 - z; dd = $15 - d; if (dz < 0) dz = -dz; if (dd < 0) dd = -dd; if (dz <= 1.5 && dd <= 2) n++ } END { print n + 0 }')
  check $([ "$ok" = "2" ] && echo 1 || echo 0) "launchers: the grenade and rocket launchers at the shotgun's height hipfire ($ok of 2)"
fi

if [[ $SECTIONS == *d* ]]; then
  # d. the ironsight's zoom and relief follow the fov: at his settings (fov 95, vr_flat_aim_fov 45, relief 4) the view is 50 and
  #    the relief 4 as before; at fov 70 and 110 the same magnification (tangents, 2.340) and the same sight picture
  #    (relief * tan(half the sighted fov) = 4 tan 25); vr_flat_model_fov 70 at fov 95 with relief 14: 14 tan 25 / tan 35 = 9.32.
  log=$HERE/scratch/flatads_d.log
  ONE() { echo "$1; wait5; vr_flat_aim 1; wait40; vr_flat_sight_check; vr_flat_aim 0; wait30;"; }
  H="tune vr_flat_aim_fov 45; tune vr_flat_ads_relief 4"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait30; $H; $(ONE 'fov 95') $(ONE 'fov 70') $(ONE 'fov 110') $(ONE 'fov 95; tune vr_flat_ads_relief 14; tune vr_flat_model_fov 70') tune_reset; fov 90; toggleconsole; quit" -Filter "^flatsightpose" -Timeout 90 > "$log" 2>&1
  grep -a "^flatsightpose" "$log" | awk '{ printf "  fov %s of %s: relief %s (scale %s)\n", $19, $21, $3, $NF }'
  r=$(grep -a "^flatsightpose" "$log" | awk 'function t(d) { return sin(d * 3.14159265 / 360) / cos(d * 3.14159265 / 360) }
    { n++; f = $19; b = $21; rel = $3; mag = t(b) / t(f); want = (n == 4) ? 14 * t(50) / t(70) : 4 * t(50) / t(f)
      okm = (mag > 2.340 * 0.99 && mag < 2.340 * 1.01); okr = (rel > want * 0.98 && rel < want * 1.02)
      if (n == 1 && (f < 49.95 || f > 50.05 || rel < 3.95 || rel > 4.05)) okr = 0
      if (okm && okr) good++; printf "%s mag %.3f relief %.2f want %.2f; ", n, mag, rel, want }
    END { printf "| %d", good + 0 }')
  echo "  $r"
  check $([ "${r##*| }" = "4" ] && echo 1 || echo 0) "fov: his look kept at fov 95 (50 deg, relief 4); zoom and sight picture kept at fov 70 and 110 and with a model fov"
fi

if [[ $SECTIONS == *e* ]]; then
  # e. the crosshairs fade with the ironsight: the visor crosshair (vr_flat_xhair; the 2D one gives way to it) and vr_crosshair's dot drawn hipfire (the middle of the screen
  #    differs with them on and off), gone sighted (no pixel differs); the fade read part way (4 frames into the 11-frame travel),
  #    at its end, and back after release.
  log=$HERE/scratch/flatads_e.log
  ON="crosshair 1; vr_crosshair 1; vr_flat_xhair 1; wait2; screenshot; crosshair 0; vr_crosshair 0; vr_flat_xhair 0; wait2; screenshot; crosshair 1; vr_crosshair 1; vr_flat_xhair 1; wait2; screenshot;"
  bash $KIT/run.sh $AGENT -Clean -ExtraArgs "-flatscreenmode" -Script "$P; vr_flat_bob 0; vr_flat_xhair_opacity 1; impulse 154; wait40; $ON vr_flat_aim 1; wait4; vr_flat_sight_check; wait40; vr_flat_sight_check; $ON vr_flat_aim 0; wait40; vr_flat_sight_check; reset vr_flat_xhair_opacity; toggleconsole; quit" -Filter "^flatcrosshair" -Out flatads_e.png -Timeout 60 > "$log" 2>&1
  fades=$(grep -a "^flatcrosshair" "$log" | awk '{ printf "%s ", $2 }')
  shots=$(grep -a "full size in" "$log" | sed 's/.*full size in //' | tr -d '\r' | tail -1)
  px=$(python "$HERE/flatads_xhair.py" "$shots")
  echo "  fade part way, sighted, released: $fades; $px"
  check $(awk -v f="$fades" 'BEGIN { split(f, a, " "); exit !(a[1] > 0.05 && a[1] < 0.95 && a[2] == 0 && a[3] == 1) }' && echo 1 || echo 0) "crosshair: fades over the ironsight's travel, gone sighted, back on release ($fades)"
  check $(awk -v s="$px" 'BEGIN { split(s, a, " "); exit !(a[3] > a[5] + 50 && a[7] <= a[9] + 10) }' && echo 1 || echo 0) "crosshair: drawn hipfire, no pixel of it sighted (past the scene's own frame-to-frame change)"
fi

verdict; exit $fail
