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
#   7. the shotgun's (V) overhead swing: the same arc;
#   8. (round 2, 2026-11-10) the axe raised behind the head (vr_flat_swing_raise_fwd -12, his quick 0.075 s strike) still
#      hurts a fresh knight as hard (it missed: the melee rules took the strike for a wave; a button's blow is deliberate);
#   9. the swing's end on the crosshair's ray (vr_flat_melee_aim_hit: what the ray hits, clamped to the arm's reach): the
#      axe's and the shotgun's hands under 1.5 units beside it (the shotgun's at the old point, vr_flat_melee_aim_hit 0,
#      printed for comparison);
#  10. a grunt 16 ahead shoved (the hands pass through it on their way back, vr_flat_shove_pass_bodies): read every few
#      frames from the landing, it never comes back towards him and ends over 30 units further than it was.
# Every wait is a blow's own frames at 72 a second (a shove 0.4 s: 29, and its 0.8 s cooldown: 58; a punch 0.6 s: 44; a
# swing 0.79 s: 57) with some to spare; the run ends on the probe's own "flatmelee: done" line. Prints PASS/FAIL per
# check; exits 1 on a failure. (The order is the hands': empty first, then the launcher, then a shotgun in each.)
AGENT=${1:-phases}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
QT_NAME=flatmelee
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
log=$HERE/scratch/flatmelee.log
S="map e1m1; wait60; god; notarget; developer 1; vr_flatplay_debug 1; vr_flat_melee_probe 1; wait5"
S="$S; vr_flat_shove_now; wait65"
S="$S; vr_flat_punch main; vr_flat_punch off; wait60"
S="$S; impulse 160; wait60; tune vr_flat_shove_both_always 0; vr_flat_shove_now; wait65; tune_reset vr_flat_shove_both_always" # (the one-handed shove: both_always 1 by default since 2026-11-11)
S="$S; tune vr_flat_shove_both_always 1; vr_flat_shove_now; wait65; tune_reset vr_flat_shove_both_always"
S="$S; impulse 154; wait20; impulse 174; wait20; vr_flat_shove_now; wait65"
S="$S; impulse 152; wait20; vr_test_spawn 5; vr_test_spawn_dist 24; impulse 241; wait20; vr_flat_punch main; wait70; vr_test_hurt 1; wait5"
S="$S; vr_test_kill 1; wait5; impulse 154; wait20; vr_flat_punch main v; wait70"
S="$S; tune vr_flat_melee_aim_hit 0; vr_flat_punch main v; wait70; tune_reset vr_flat_melee_aim_hit"
S="$S; impulse 152; wait20; tune vr_flat_swing_raise_fwd -12; tune vr_flat_swing_strike 0.075; impulse 241; wait20; vr_flat_punch main; wait70; vr_test_hurt 1; wait5"
S="$S; tune_reset vr_flat_swing_raise_fwd; tune_reset vr_flat_swing_strike"
R="vr_test_hurt -1"
S="$S; vr_test_kill 1; wait5; vr_test_spawn 0; vr_test_spawn_dist 16; impulse 241; wait30; vr_flat_shove_now; wait14; $R; wait6; $R; wait6; $R; wait6; $R; wait6; $R; wait10; $R; wait30; $R"
S="$S; vr_flat_melee_probe done; wait2; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$S" -QuitOn "flatmelee: done" -Timeout 120 \
    -Filter "^flatmelee:|test hurt:|still out|vr_flat_punch:|flatplay: .*(shove|punch)" > "$log" 2>&1
grep -a "flatmelee: \(main\|off\)\|flatplay: shove" "$log" | sed 's/^/  /'
# The n-th probe line of a kind: `pick <kind> <n> <field>` prints the number after <field> (n eye up dn palm fing vp roll
# both); `hi` the second of vp's two (the view's most down; vp's first is its most up).
pick() { grep -a "flatmelee: $1" "$log" | sed -n "${2}p" | grep -ao " $3 [-0-9.]*" | head -1 | awk '{ print $NF + 0 }'; }
hi() { grep -a "flatmelee: $1" "$log" | sed -n "${2}p" | grep -ao " vp [-0-9.]* [-0-9.]*" | awk '{ print $3 + 0 }'; }
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
check $(ok "$(pick 'off shove both' 1 palm) > 0.7 && $(pick 'off shove both' 1 fing) > 0.7 && $(pick 'main shove both' 1 palm) > 0.7 && $(pick 'main shove both' 1 fing) > 0.7") "shove, hands free: both push, palms at the target, fingers up"
check $(ok "$(pick 'off shove both' 3 palm) > 0.7 && $(pick 'off shove both' 3 fing) > 0.7 && $(pick 'main shove both' 3 palm) > 0.7 && $(pick 'main shove both' 3 fing) > 0.7") "shove, a gun in each hand: both push, the guns held, palms out"
check $(ok "$(pick 'off shove:' 1 fing) > 0.7 && $(pick 'off shove:' 1 palm) > 0.7") "shove, one two-handed gun: the free hand lets go of the grip and pushes"
check $(ok "$(pick 'off shove both' 2 fing) > 0.7 && $(pick 'main shove both' 2 fing) > 0.7") "shove, vr_flat_shove_both_always 1: the two-handed gun shoves with both hands"
check $(grep -aq "off punch (vr_flat_punch): the other hand's punch is still out" "$log" && [ "$(grep -a 'flatmelee: off punch' "$log" | wc -l)" = "0" ] && echo 1 || echo 0) "punch: no two hands at once"
check $(ok "$(hi 'main punch' 1) > 1 && $(pick 'main punch' 1 roll) != 0") "punch: the view tips into it (pitch down, a roll)"
for n in 1 2; do
  what=$([ $n = 1 ] && echo "the axe" || echo "the shotgun")
  check $(ok "$(pick 'main swing' $n eye) > 0 && $(pick 'main swing' $n up) > 60 && $(pick 'main swing' $n dn) < -20 && $(pick 'main swing' $n vp) < -0.5 && $(hi 'main swing' $n) > 1") "overhead swing, $what: the hand over the eye, the weapon up then down, the view back then forward"
done
# (vr_test_hurt reads the nearest live monster: at vr_flat_swing_strike 0.075, the default since 2026-11-11, the axe's
# swing can kill the knight outright, and the line is then a grunt far off: the knight's health is 0.)
health=$(grep -a "test hurt:" "$log" | head -1 | awk '/monster_knight/ { for(i = 1; i <= NF; i++) if($i == "health") print $(i + 1) + 0; next } { print 0 }')
echo "  the knight after the axe's swing: ${health:-?} of 75 ($log)"
check $(ok "${health:-75} < 75") "overhead swing, the axe: the knight in front is hurt"
back=$(grep -a "test hurt:" "$log" | sed -n 2p | grep -ao "health [0-9.]*" | awk '{ print 75 - $2 }')
echo "  the axe raised behind the head (raise_fwd -12, strike 0.075 s): ${back:-?} damage"
check $(ok "${back:-0} >= 30") "overhead swing, the axe raised behind the head: it still lands (30 damage or more)"
offAxe=$(pick 'main swing' 1 off); offGun=$(pick 'main swing' 2 off); offOld=$(pick 'main swing' 3 off)
echo "  the hand beside the crosshair's ray at the strike's end: the axe ${offAxe:-?}, the shotgun ${offGun:-?} units (the shotgun at the old fixed point: ${offOld:-?})"
check $(ok "${offAxe:-99} >= 0 && ${offAxe:-99} < 1.5 && ${offGun:-99} >= 0 && ${offGun:-99} < 1.5") "overhead swing: it ends on the crosshair's ray"
ds=$(grep -a "test hurt: monster_army -1 damage" "$log" | grep -ao "[0-9.]* u away" | awk '{ print $1 }' | tr '\n' ' ')
echo "  the shoved grunt, from the landing on: $ds"
check $(echo "$ds" | awk '{ ok = NF >= 6; for(i = 2; i <= NF; i++) if($i < $(i - 1) - 1) ok = 0; if($NF < $1 + 30) ok = 0; print ok }') "shove: the grunt is never pulled back by the hands' way back, and ends 30 further"
verdict
exit $fail
