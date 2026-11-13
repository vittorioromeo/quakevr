#!/bin/bash
# kdstir_test.sh <agent> -- a living knockdown's concussed stirring (vr_knockdown_stir and its tunables; vr_box3d_ragdoll.cpp
# stirFrame; his notes 2026-11-13: shoved down they lay almost static, read as dead; then "can't see any difference": at
# the shipped 1.75-3.5 s down one small move came at most). Headless, on vrfiringrange, a grunt shoved down (two hands, sure):
#   on    8 s down: within 5 s it has stirred from as it landed (the first move within 0.5 s of its lying), several moves
#         (6 or more), its joints turned 20 degrees or more at their most and on average 60% or more of the turns asked
#         (not lost in the floor, its stops or the fall's fading drive); got up (vr_knockdown_test 1) it stirs no more.
#   kill  shoved down the same way and killed lying there (vr_test_kill 1): its corpse never stirs (no move after it).
#   off   vr_knockdown_stir 0: shoved down the same way, no move at all.
#   time  6 knockdowns of the shipped length band's middle (2.5 s, fixed: the same timer either way), vr_knockdown_stir 1
#         and 0: each knockdown's get-up due at the same time and on its feet no later (0.3 s at most; the rise is the
#         physics': a pose a little off) and on average no later (0.1 s); 3 or more moves in each knockdown stirring.
# Reads vr_knockdown_test 9's stircheck: line and vr_knockdown_debug's knockdown: lines.
AGENT=${1:-takeslight}
source "$(dirname "$0")/../qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
F="stircheck:|stirs:|stirred:|kdstir:|fell: down|get-up due after|its get-up after"
BASE="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 60;vr_knockdown_chance 100;vr_knockdown_time_min 8;vr_knockdown_time_max 8;vr_knockdown_debug 1"
ON="$BASE;impulse 241;wait40;vr_knockdown_test 22;wait360;echo kdstir: down;vr_knockdown_test 9"
ON="$ON;vr_knockdown_test 1;wait288;echo kdstir: up;vr_knockdown_test 9"
ON="$ON;echo kdstir: end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$ON" -Filter "$F" -QuitOn "kdstir: end" -Timeout 300 > "$S/kdstir_on.log" 2>&1
# Killed while down (vr_test_kill 1: the live monster nearest, the one lying at his feet): a moment later and 3 s on.
KILL="$BASE;impulse 241;wait40;vr_knockdown_test 22;wait216;vr_knockdown_test 9;vr_test_kill 1;wait36;echo kdstir: killed;vr_knockdown_test 9;wait216;echo kdstir: later;vr_knockdown_test 9;echo kdstir: end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$KILL" -Filter "$F" -QuitOn "kdstir: end" -Timeout 300 > "$S/kdstir_kill.log" 2>&1
OFF="$BASE;vr_knockdown_stir 0;impulse 241;wait40;vr_knockdown_test 22;wait360;echo kdstir: down;vr_knockdown_test 9;echo kdstir: end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$OFF" -Filter "$F" -QuitOn "kdstir: end" -Timeout 300 > "$S/kdstir_off.log" 2>&1
# Six knockdowns of 2.5 s (each shove 500 frames, ~7 s, after the last: up and steady again), stirring and not.
SIX="vr_knockdown_test 22;wait500;vr_knockdown_test 22;wait500;vr_knockdown_test 22;wait500;vr_knockdown_test 22;wait500;vr_knockdown_test 22;wait500;vr_knockdown_test 22;wait500"
TIME="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 60;vr_knockdown_chance 100;vr_knockdown_time_min 2.5;vr_knockdown_time_max 2.5;vr_knockdown_debug 1;developer 1"
bash $KIT/run.sh $AGENT -Script "$TIME;impulse 241;wait40;$SIX;echo kdstir: end;toggleconsole;quit" -Filter "$F" -QuitOn "kdstir: end" -Timeout 400 > "$S/kdstir_time1.log" 2>&1
bash $KIT/run.sh $AGENT -Script "$TIME;vr_knockdown_stir 0;impulse 241;wait40;$SIX;echo kdstir: end;toggleconsole;quit" -Filter "$F" -QuitOn "kdstir: end" -Timeout 400 > "$S/kdstir_time0.log" 2>&1
# The stircheck: line after each mark: "moves stirring down done reached achieved first".
after() { awk -v m="kdstir: $2" '$0 ~ m {f=1; next} f && /^stircheck:/ {for(i=1;i<=NF;i++){split($i,a,"="); v[a[1]]=a[2]} print v["moves"], v["stirring"], v["down"], v["done"], v["reached"], v["achieved"], v["first"]; exit}' "$1"; }
read dm ds dd dn dr da df <<<"$(after "$S/kdstir_on.log" down)"
check $([ -n "$dm" ] && [ "$dm" -ge 6 ] && awk -v r="$dr" -v a="$da" -v f="$df" 'BEGIN{exit !(r >= 20 && a >= 0.6 && f >= 0 && f <= 0.5)}' && echo 1 || echo 0) \
    "down 5 s: it stirred from as it lay (first move $df s after), $dm moves ($dn done), a joint turned $dr deg at most, $da of the turns asked on average"
read um us ud un ur ua uf <<<"$(after "$S/kdstir_on.log" up)"
check $([ "$us" = 0 ] && [ "$ud" = 0 ] && echo 1 || echo 0) "got up: not stirring (living ones down $ud, stirring $us)"
read km ks kd kn kr ka kf <<<"$(after "$S/kdstir_kill.log" killed)"
read lm ls ld ln lr la lf <<<"$(after "$S/kdstir_kill.log" later)"
check $([ -n "$km" ] && [ "$kd" = 0 ] && [ "$ks" = 0 ] && [ "$lm" = "$km" ] && [ "$ls" = 0 ] && echo 1 || echo 0) \
    "killed while down: its corpse never stirs (living ones down $kd; moves $km at the kill, $lm 3 s later; stirring $ks, $ls)"
read om os od on or oa of <<<"$(after "$S/kdstir_off.log" down)"
check $([ "$om" = 0 ] && echo 1 || echo 0) "vr_knockdown_stir 0: down 5 s, no move ($om)"
# Per knockdown: "due afoot moves" (moves: stirs: lines after its fall).
times() { awk '/fell: down/{n++; m[n]=0} / stirs: /{m[n]++} /get-up due after/{for(i=1;i<=NF;i++) if($i=="after") d[n]=$(i+1)} /its get-up after/{for(i=1;i<=NF;i++) if($i=="after") u[n]=$(i+1)} END{for(i=1;i<=n;i++) printf "%s %s %s\n", d[i], u[i], m[i]}' "$1"; }
times "$S/kdstir_time1.log" > "$S/kdstir_time1.txt"
times "$S/kdstir_time0.log" > "$S/kdstir_time0.txt"
cmp=$(paste -d' ' "$S/kdstir_time1.txt" "$S/kdstir_time0.txt" | awk '{n++; if($1!=$4) due++; d=$2-$5; s+=d; if(d>w) w=d; if($3<3) few++; if($6>0) off++; m=m" "$3} END{printf "%d %d %.2f %.2f %d %d%s", n, due+0, n?s/n:0, w+0, few+0, off+0, m}')
read n due mean worst few off moves <<<"$cmp"
check $([ "$n" = 6 ] && [ "$due" = 0 ] && [ "$few" = 0 ] && [ "$off" = 0 ] && awk -v m="$mean" -v w="$worst" 'BEGIN{exit !(m <= 0.1 && w <= 0.3)}' && echo 1 || echo 0) \
    "2.5 s knockdowns, stirring against not ($n): get-up due at the same time (differing $due), on its feet later by $mean s on average, $worst s at most; moves each:$moves (fewer than 3: $few; with it off: $off)"
verdict; exit $fail
