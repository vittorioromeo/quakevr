#!/bin/bash
# flatads_test.sh <agent> -- flatscreen ironsight items of 2026-11-05 (docs/vr-port/ROUND24.md, "Flat pose: grips, ADS recoil,
# the sights", agent/flatsolve6). Fast mode; each run ends with its own script.
#   a. recoil: a shotgun shot hipfire, then sighted: the drawn recoil sighted is vr_flat_ads_recoil (0.35) of hipfire's.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
export QVR_AUTHOR_WAIT_ALWAYS=1
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
P="map e1m1; god; notarget; developer 1; r_norefresh 0; setpos 480 -352 88 0 0 0"
SECTIONS=${SECTIONS:-a}

if [[ $SECTIONS == *a* ]]; then
  log=$HERE/scratch/flatads_a.log
  SHOT="vr_flat_recoil_probe; +attack; wait4; -attack; wait50; vr_flat_recoil_probe;"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; impulse 154; wait40; $SHOT vr_flat_aim 1; wait60; $SHOT vr_flat_aim 0; wait30; toggleconsole; quit" -Filter "^flatrecoil" -Timeout 60 > "$log" 2>&1
  hip=$(grep -a "^flatrecoil" "$log" | sed -n 2p); ads=$(grep -a "^flatrecoil" "$log" | sed -n 4p)
  echo "  hipfire: $hip"; echo "  sighted: $ads"
  hp=$(awk '{ print $3 }' <<<"$hip"); ha=$(awk '{ print $5 }' <<<"$hip"); ap=$(awk '{ print $8 }' <<<"$ads"); aa=$(awk '{ print $10 }' <<<"$ads")
  check $(awk -v p="${hp:-0}" -v a="${ha:-0}" 'BEGIN { exit !(p > 0.1 && a > 0.1) }' && echo 1 || echo 0) "recoil: a hipfire shot kicks the drawn gun ($hp u, $ha deg)"
  check $(awk -v hp="${hp:-0}" -v ha="${ha:-0}" -v ap="${ap:-9}" -v aa="${aa:-9}" 'BEGIN { exit !(hp > 0 && ha > 0 && ap / hp > 0.30 && ap / hp < 0.40 && aa / ha > 0.30 && aa / ha < 0.40) }' && echo 1 || echo 0) "recoil: sighted it kicks 0.35 of that ($ap u, $aa deg)"
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

verdict; exit $fail
