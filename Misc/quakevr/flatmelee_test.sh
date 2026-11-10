#!/bin/bash
# flatmelee_test.sh <agent> -- flat-screen play's shove, punch and overhead weapon swing (his notes 2026-11-10; FLATPLAY.md,
# "Shove" and "Punch and swing"). One run, the probe on (vr_flat_melee_probe: a "flatmelee:" line as each blow ends):
#   1. both hands free: the shove pushes with both, palms at the target, fingers up (cosines over 0.7 at full push);
#   2. a gun in each hand: both hands shove, the guns held, turned palms out the same way;
#   3. one two-handed gun (the rocket launcher, the free hand on its grip): the free hand lets go and shoves (it was held
#      on the grip and its push never showed);
#   4. vr_flat_shove_both_always 1: that same gun shoves with both hands;
#   5. punches: a second hand's punch while the first is out is refused (no two at once), and a punch tips the view
#      (pitch down, a roll);
#   6. the axe's overhead swing: the hand over the eye, the weapon turned up past 60 degrees and down past 20, the view
#      tipped back and then forward, and the knight 24 units ahead hurt by it (one read after it, against his 75);
#   7. the shotgun's (V) overhead swing: the same arc.
# Every wait is a blow's own frames at 72 a second (a shove 0.4 s: 29; a punch 0.6 s: 44; a swing 0.79 s: 57) with some
# to spare; the run ends on the probe's own "flatmelee: done" line. Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-phases}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
QT_NAME=flatmelee
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
log=$HERE/scratch/flatmelee.log
S="map e1m1; wait60; god; notarget; developer 1; vr_flatplay_debug 1; vr_flat_melee_probe 1; wait5"
S="$S; vr_flat_shove_now; wait45"
S="$S; impulse 154; impulse 174; wait20; vr_flat_shove_now; wait45"
S="$S; impulse 170; impulse 160; wait60; vr_flat_shove_now; wait45"
S="$S; tune vr_flat_shove_both_always 1; vr_flat_shove_now; wait45; tune_reset vr_flat_shove_both_always"
S="$S; impulse 150; wait20; vr_flat_punch main; vr_flat_punch off; wait60"
S="$S; impulse 152; wait20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_flat_punch main; wait70; vr_test_hurt 1; wait5"
S="$S; vr_test_kill 1; wait5; impulse 154; wait20; vr_flat_punch main v; wait70; vr_flat_melee_probe done; wait2; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$S" -QuitOn "flatmelee: done" -Timeout 120 \
    -Filter "^flatmelee:|test hurt:|still out|flatplay: .*(shove|punch)" > "$log" 2>&1
grep -a "flatmelee: \(main\|off\)" "$log" | sed 's/^/  /'
# n-th line of a kind: `field <kind regex> <n> <name>` prints the number after <name>.
pick() { grep -a "flatmelee: $1" "$log" | sed -n "${2}p" | grep -ao "$3 [-0-9.]*" | head -1 | awk '{ print $NF + 0 }'; }
ok() { awk "BEGIN { exit !($1) }" && echo 1 || echo 0; }
check $(ok "$(pick 'off shove both' 1 palm) > 0.7 && $(pick 'off shove both' 1 fingers) > 0.7 && $(pick 'main shove both' 1 palm) > 0.7 && $(pick 'main shove both' 1 fingers) > 0.7") "shove, hands free: both push, palms at the target, fingers up"
check $(ok "$(pick 'off shove both' 2 fingers) > 0.7 && $(pick 'main shove both' 2 fingers) > 0.7 && $(pick 'main shove both' 2 palm) > 0.7") "shove, a gun in each hand: both push, the guns held, palms out"
check $(ok "$(pick 'off shove:' 1 fingers) > 0.7 && $(pick 'off shove:' 1 palm) > 0.7") "shove, one two-handed gun: the free hand lets go of the grip and pushes"
check $(ok "$(pick 'off shove both' 3 fingers) > 0.7 && $(pick 'main shove both' 3 fingers) > 0.7") "shove, vr_flat_shove_both_always 1: the two-handed gun shoves with both hands"
check $(grep -aq "off punch (vr_flat_punch): the other hand's punch is still out" "$log" && [ "$(grep -a 'flatmelee: off punch' "$log" | wc -l)" = "0" ] && echo 1 || echo 0) "punch: no two hands at once"
hi() { grep -a "flatmelee: $1" "$log" | sed -n "${2}p" | sed -n 's/.*view pitch [-0-9.]*\.\.\([-0-9.]*\).*/\1/p' | awk '{ print $1 + 0 }'; }
check $(ok "$(hi 'main punch' 1) > 1 && $(pick 'main punch' 1 roll) != 0") "punch: the view tips into it (pitch down, a roll)"
for n in 1 2; do
  what=$([ $n = 1 ] && echo "the axe" || echo "the shotgun")
  check $(ok "$(pick 'main swing' $n 'over eye') > 0 && $(pick 'main swing' $n 'weapon up') > 60 && $(pick 'main swing' $n down) < -20 && $(pick 'main swing' $n 'view pitch') < -0.5 && $(hi 'main swing' $n) > 1") "overhead swing, $what: the hand over the eye, the weapon up then down, the view back then forward"
done
health=$(grep -a "test hurt:" "$log" | head -1 | grep -ao "health [0-9.]*" | awk '{ print $2 + 0 }')
echo "  the knight after the axe's swing: ${health:-?} of 75 ($log)"
check $(ok "${health:-75} < 75") "overhead swing, the axe: the knight in front is hurt"
verdict
exit $fail
