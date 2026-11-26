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
#             (flatparry3) Before the guard, not parrying: the free hand on the handle, at or below the holding hand
#             (the judicator's grip lay up its blade: vr_flat_melee_grip_below).
#   (flatparry3; his flat notes 2026-11-26, items 3, 4 and 8) The flip runs above take vr_flat_parry_rev 0 (the unflip,
#   kept as an option); by default:
#   rev       the knight's sword (held in two hands, then flipped), the judicator's, the spear (given flipped): the guard
#             keeps the grip flipped (drawn flip 1.00) and takes the reverse-grip guard (share 1.00), parries the probe;
#             the drawn hands' most turn a frame into it and out of it (vr_flat_melee_2h_report) at most 20 degrees (the
#             unflip's printed beside it); the knight's sword's free hand stays on its handle through it and after.
#   polehand  the flipped spear's other hand: vr_flat_revgrip_pole_off_up 4 moves it 3 u or more, _off_pitch 0 turns it.
# Prints "flatparry2: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
W=C:/OHWorkspace/qvr-agents/$AGENT
S=$W/scratch/flatparry2
mkdir -p "$S"
ST="vr_flatplay_status"
BASE="developer 1;map vrtesthall;wait30;god;notarget;vr_flatplay_debug 1;vr_debug_2h_grip 1;vr_parry_stamina 0;setpos 0 300 25 0 90 0;wait5"
FILTER='^fp2|^flatparry|parry probe|parry flip:|blade grip:|parry guard:|2h grip:|two-handed stance: the|^flat2h: drawn|admelee|ERROR|CRASH'
# (the drawn hands' most turn a frame in a block's vr_flat_melee_2h_report: main, off)
turns() { block "$1" "$2" | grep -a "^flat2h: drawn" | head -1 | grep -ao '[0-9.]* deg' | grep -ao '^[0-9.]*' | paste -sd' '; }
stance() { block "$1" "$2" | grep -a -A1 "two-handed stance: the off hand .* along" | head -2 | paste -sd' '; }
along() { stance "$1" "$2" | grep -ao 'hand -\?[0-9.]* u along' | grep -ao -- '-\?[0-9.]*' | head -1; }
run() { # name script
  bash $KIT/run.sh $AGENT -Games '-game ad -game quakevr' -ExtraArgs '-nomapindex -noaddons -flatscreenmode' -Script "$2" \
    -QuitOn "^fp2 done" -Timeout 240 -Filter "$FILTER" $3 > "$S/$1.log" 2>&1
}
block() { awk -v m="fp2: $2" '$0==m || $0==m" "{on=1;next} /^fp2:|^fp2 done/{on=0} on' "$S/$1.log"; }
flip() { block flip "$2" | grep -a "parry flip: main" | grep -ao 'drawn flip [0-9.]*' | grep -ao '[0-9.]*$' | head -1; }

# 1. The flipped grip's guard: the knight's sword (impulse 163) and the judicator's (vr_ad_test 172).
# (flatparry3: the unflip is vr_flat_parry_rev 0's.)
SC="$BASE;tune vr_flat_parry_rev 0"
for w in "sword impulse 163" "judge vr_ad_test 172"; do
  set -- $w
  n=$1; shift
  # (The flip: vr_revgrip_flip_time 0.18 s, 13 frames; the guard 0.05 s and the unflip 0.1 s, 8 frames: 30 is plenty.)
  SC="$SC;$*;wait60;impulse 43;wait40;echo fp2: $n flipped;$ST;vr_flat_melee_2h_report;echo fp2: $n guard;+offhandattack;wait30;vr_flat_melee_2h_report;impulse 249;wait3;$ST"
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
  echo "  $n: vr_flat_parry_rev 0, the unflip's most turn a frame into the guard (main off): $(turns flip "$n guard") degrees"
done

# 1b. (flatparry3) The reverse-grip guard (vr_flat_parry_rev 1, the default): the knight's sword held in two hands then
# flipped, the judicator's flipped, the spear given flipped (vr_test_give_flags 11520: kind 13 and QVR_WPNFLAG_REVGRIP).
SC="$BASE"
for w in "sword impulse 163;wait60;impulse 43" "judge vr_ad_test 172;wait60;impulse 43" "spear vr_test_give_wid 13;vr_test_give_flags 11520;impulse 149"; do
  n=${w%% *}
  # (vr_flat_parry_rev_in_time / _out_time 0.2 s, 15 frames: 30 is plenty.)
  SC="$SC;${w#* };wait60;echo fp2: $n rflipped;$ST;vr_flat_melee_2h_report;echo fp2: $n rguard;+offhandattack;wait30;vr_flat_melee_2h_report"
  SC="$SC;impulse 249;wait3;$ST;echo fp2: $n rafter;-offhandattack;wait30;vr_flat_melee_2h_report;$ST;wait2"
done
run rev "$SC;echo fp2 done;toggleconsole;quit"
check $(grep -aq "^fp2 done" "$S/rev.log" && echo 1 || echo 0) "rev run reached its end"
for n in sword judge spear; do
  b=$(block rev "$n rguard" | grep -a "parry flip: main hand drawn" | grep -ao 'drawn flip [0-9.]*' | grep -ao '[0-9.]*$' | head -1)
  r=$(block rev "$n rguard" | grep -a "reverse-grip guard share" | grep -ao 'share [0-9.]*' | grep -ao '[0-9.]*$' | head -1)
  g=$(block rev "$n rguard" | grep -a "parry guard: main" | grep -ao 'share [0-9.]*' | grep -ao '[0-9.]*$' | head -1)
  check $([ "$b" = 1.00 ] && [ "$r" = 1.00 ] && [ "$g" = 1.00 ] && echo 1 || echo 0) "$n: the guard keeps the grip flipped (drawn flip ${b:-?}), the reverse-grip guard (share ${r:-?}, guard ${g:-?})"
  check $(block rev "$n rguard" | grep -aq "parry probe.*PARRIES" && echo 1 || echo 0) "$n: the reverse-grip guard parries"
  ti=$(turns rev "$n rguard"); to=$(turns rev "$n rafter")
  check $([ -n "$ti" ] && awk -v a="${ti% *}" -v b="${ti#* }" 'BEGIN{exit !(a <= 20 && b <= 20)}' && echo 1 || echo 0) "$n: into it, the drawn hands' most turn a frame (main off) ${ti:-?} degrees (20 at most)"
  check $([ -n "$to" ] && awk -v a="${to% *}" -v b="${to#* }" -v n=$n 'BEGIN{exit !(a <= 20 && (b <= 20 || n != "sword"))}' && echo 1 || echo 0) "$n: out of it, ${to:-?} degrees (20 at most; the off hand's for the sword, on the handle)"
done
for p in rguard rafter; do
  st=$(stance rev "sword $p")
  check $(grep -aq "holds its grip" <<<"$st" && awk -v a="$(along rev "sword $p")" 'BEGIN{exit !(a != "" && a <= 0 && a >= -10)}' && echo 1 || echo 0) "sword $p: the free hand on the flipped sword's handle ($(along rev "sword $p") u along; ${st##*; })"
done

# 1c. (flatparry3, item 4) The flipped spear's other hand: vr_flat_revgrip_pole_off_up / _off_pitch move and turn it.
offs() { block polehand "$1" | grep -a -A1 "two-handed stance: the off hand drawn" | head -2 | paste -sd" " | grep -ao -- '-\?[0-9.]* -\?[0-9.]* -\?[0-9.]* off\|angles -\?[0-9.]* -\?[0-9.]* -\?[0-9.]*' | grep -ao -- '-\?[0-9.]*' | paste -sd' '; }
run polehand "$BASE;vr_test_give_wid 13;vr_test_give_flags 11520;impulse 149;wait60;echo fp2: before;$ST;tune vr_flat_revgrip_pole_off_up 4;tune vr_flat_revgrip_pole_off_pitch 0;wait20;echo fp2: tuned;$ST;tune_reset vr_flat_revgrip_pole_off_up;tune_reset vr_flat_revgrip_pole_off_pitch;wait2;echo fp2 done;toggleconsole;quit"
o1=$(offs before); o2=$(offs tuned)
mv=$(awk -v a="$o1" -v b="$o2" 'BEGIN{n=split(a,x," "); m=split(b,y," "); if(n<6||m<6){print -1; exit} d=0; for(i=1;i<=3;i++) d+=(x[i]-y[i])^2; t=0; for(i=4;i<=6;i++){e=x[i]-y[i]; while(e>180)e-=360; while(e<-180)e+=360; t+=(e<0?-e:e)} printf "%.1f %.1f", sqrt(d), t}')
check $(awk -v m="$mv" 'BEGIN{split(m,v," "); exit !(v[1] >= 3 && v[2] >= 15)}' && echo 1 || echo 0) "polehand: the flipped spear's other hand moved / turned by the tunables (${mv:-?}: units, degrees; 4 up, pitch -40 -> 0)"

# 2. The free hand in the guard, per weapon (each its own run: its screenshot).
for w in "sword impulse 163" "fury vr_ad_test 171" "judge vr_ad_test 172" "hammer vr_ad_test 173" "mace vr_ad_test 174"; do
  set -- $w
  n=$1; shift
  # (The guard 0.05 s, the blade 0.12 s more and vr_twohand's take 0.2 s: 60 frames are plenty.)
  run $n "$BASE;$*;wait60;echo fp2: $n stance;$ST;echo fp2: $n guard;+offhandattack;wait60;$ST;screenshot;wait2;-offhandattack;wait30;echo fp2 done;toggleconsole;quit" "-Clean -Out fp2_$n.png"
  # (flatparry3, item 8: not parrying, both hands on the handle: the free hand at or below the holding hand.)
  st=$(stance $n "$n stance"); a=$(along $n "$n stance")
  check $(grep -aq "holds its grip" <<<"$st" && awk -v a="$a" 'BEGIN{exit !(a != "" && a <= 0 && a >= -12)}' && echo 1 || echo 0) "$n: not parrying, the free hand on the handle (${a:-?} u along from the holding hand; ${st##*; })"
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
