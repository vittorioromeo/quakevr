#!/bin/bash
# flatparry2_test.sh <agent> -- flat-screen play, AD mounted: the parry guard's pose (flatparry2; his flat notes
# 2026-11-26, items 1 and 4-6). Headless runs in fast mode, each ending on its own last line (-QuitOn):
#   flip      the knight's sword and the judicator's sword in the flipped (inverted) grip (impulse 43): the right mouse
#             button raises the guard and the grip is drawn upright (drawn flip 0.00) within vr_flat_parry_unflip_time
#             (0.1 s; two frames' slack), the guard parries QC's probe (impulse 249); let go, the grip flips back (1.00)
#             within the same time.
#   sword fury judge hammer mace   one weapon alone in the main hand (the knight's sword; vr_ad_test 171..174), the guard
#             held: the free hand on the weapon's line towards the tip (twohand's blade grip; a Blade hotspot, or the
#             shaft's share vr_flat_parry_shaft_at where it has none), the posed hand within 3 u of that point, the point at
#             least 8 u up the weapon from the holding hand (off the handle); the tip's distance printed. A screenshot of
#             each guard: scratch/fp2_<weapon>.png.
# Prints "flatparry2: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
W=C:/OHWorkspace/qvr-agents/$AGENT
S=$W/scratch/flatparry2
mkdir -p "$S"
ST="vr_flatplay_status"
BASE="developer 1;map vrtesthall;wait30;god;notarget;vr_flatplay_debug 1;vr_debug_2h_grip 1;vr_parry_stamina 0;setpos 0 300 25 0 90 0;wait5"
FILTER='^fp2|^flatparry|parry probe|parry flip:|blade grip:|parry guard:|2h grip:|admelee|ERROR|CRASH'
run() { # name script
  bash $KIT/run.sh $AGENT -Games '-game ad -game quakevr' -ExtraArgs '-nomapindex -noaddons -flatscreenmode' -Script "$2" \
    -QuitOn "^fp2 done" -Timeout 240 -Filter "$FILTER" $3 > "$S/$1.log" 2>&1
}
block() { awk -v m="fp2: $2" '$0==m || $0==m" "{on=1;next} /^fp2:|^fp2 done/{on=0} on' "$S/$1.log"; }
flip() { block flip "$2" | grep -a "parry flip: main" | grep -ao 'drawn flip [0-9.]*' | grep -ao '[0-9.]*$' | head -1; }

# 1. The flipped grip's guard: the knight's sword (impulse 163) and the judicator's (vr_ad_test 172).
SC="$BASE"
for w in "sword impulse 163" "judge vr_ad_test 172"; do
  set -- $w
  n=$1; shift
  # (The flip: vr_revgrip_flip_time 0.18 s, 13 frames; the guard 0.05 s and the unflip 0.1 s, 8 frames: 30 is plenty.)
  SC="$SC;$*;wait60;impulse 43;wait40;echo fp2: $n flipped;$ST;echo fp2: $n guard;+offhandattack;wait30;impulse 249;wait3;$ST"
  SC="$SC;echo fp2: $n let go;-offhandattack;wait40;$ST;wait2"
done
run flip "$SC;echo fp2 done;toggleconsole;quit"
L=$S/flip.log
check $(grep -aq "^fp2 done" "$L" && echo 1 || echo 0) "flip run reached its end"
for n in sword judge; do
  a=$(flip $n "$n flipped"); b=$(flip $n "$n guard"); c=$(flip $n "$n let go")
  check $([ "$a" = 1.00 ] && echo 1 || echo 0) "$n: flipped before the guard (drawn flip ${a:-?})"
  check $([ "$b" = 0.00 ] && echo 1 || echo 0) "$n: drawn upright in the guard (drawn flip ${b:-?})"
  check $([ "$c" = 1.00 ] && echo 1 || echo 0) "$n: flipped again after it (drawn flip ${c:-?})"
  t1=$(block flip "$n guard" | grep -ao "grip upright, [0-9.]* s" | grep -ao '[0-9.]* s' | head -1)
  t2=$(block flip "$n let go" | grep -ao "grip flipped again, [0-9.]* s" | grep -ao '[0-9.]* s' | head -1)
  check $([ -n "$t1" ] && awk -v t="${t1% s}" 'BEGIN{exit !(t <= 0.1 + 2/72 + 0.001)}' && echo 1 || echo 0) "$n: upright in ${t1:-never} (vr_flat_parry_unflip_time 0.1)"
  check $([ -n "$t2" ] && awk -v t="${t2% s}" 'BEGIN{exit !(t <= 0.1 + 2/72 + 0.001)}' && echo 1 || echo 0) "$n: flipped back in ${t2:-never}"
  check $(block flip "$n guard" | grep -aq "parry probe.*PARRIES" && echo 1 || echo 0) "$n: the flipped weapon's guard parries"
done

# 2. The free hand in the guard, per weapon (each its own run: its screenshot).
for w in "sword impulse 163" "fury vr_ad_test 171" "judge vr_ad_test 172" "hammer vr_ad_test 173" "mace vr_ad_test 174"; do
  set -- $w
  n=$1; shift
  # (The guard 0.05 s, the blade 0.12 s more and vr_twohand's take 0.2 s: 60 frames are plenty.)
  run $n "$BASE;$*;wait60;echo fp2: $n guard;+offhandattack;wait60;$ST;screenshot;wait2;-offhandattack;wait30;echo fp2 done;toggleconsole;quit" "-Clean -Out fp2_$n.png"
  b=$(block $n "$n guard")
  g=$(grep -a "blade grip: main hand," <<<"$b" | head -1)
  off=$(grep -a "blade grip: the off hand" <<<"$b" | grep -ao 'hand -\?[0-9.]* u off' | grep -ao -- '-\?[0-9.]*' | head -1)
  pt=$(grep -a "blade grip: point" <<<"$b" | head -1)
  p=$(grep -ao 'point [0-9.]* u' <<<"$pt" | grep -ao '[0-9.]*' | head -1)
  tip=$(grep -a "blade grip: tip" <<<"$b" | grep -ao 'tip [0-9.]* u' | grep -ao '[0-9.]*' | head -1)
  check $(grep -aq "asked for, share 1.00, held by its blade" <<<"$g" && echo 1 || echo 0) "$n: the free hand holds the weapon up its line (${g#*hand, })"
  check $([ -n "$off" ] && awk -v d="$off" 'BEGIN{exit !(d >= 0 && d <= 3)}' && echo 1 || echo 0) "$n: the posed free hand on the point (${off:-?} u off)"
  dr=$(grep -a "blade grip: the off hand" <<<"$b" | grep -ao 'drawn -\?[0-9.]* u' | grep -ao -- '-\?[0-9.]*' | head -1)
  check $([ -n "$dr" ] && awk -v d="$dr" 'BEGIN{exit !(d >= 0 && d <= 4)}' && echo 1 || echo 0) "$n: the drawn free hand there too (${dr:-?} u off: the arm reaches it)"
  # (The hammer and the mace: the hand near the head, within 12 u of its end; the swords: off the handle, 8 u up at least.)
  most=1000; case $n in hammer|mace) most=12;; esac
  check $([ -n "$p" ] && [ -n "$tip" ] && awk -v p="$p" -v t="$tip" -v m="$most" 'BEGIN{exit !(p >= 8 && t - p <= m)}' && echo 1 || echo 0) "$n: the point ${p:-?} u up from the holding hand, $(awk -v p="${p:-0}" -v t="${tip:-0}" 'BEGIN{printf "%.1f", t - p}') u from the tip (tip ${tip:-?} u)"
done
verdict
