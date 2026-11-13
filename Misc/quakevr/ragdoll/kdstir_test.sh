#!/bin/bash
# kdstir_test.sh <agent> -- a living knockdown's concussed stirring (vr_knockdown_stir, _rate; vr_box3d_ragdoll.cpp
# stirFrame; his note 2026-11-13: shoved down they lay almost static, read as dead). Headless, two runs on vrfiringrange:
#   on    a grunt shoved down (two hands, sure, 8 s down): within 5 s it has stirred (a slow head turn, a hand's twitch,
#         an arm or a leg drawn in) and a move turned its joint by a few degrees at least (subtle, not a struggle);
#         got up (vr_knockdown_test 1) it stirs no more.
#   kill  shoved down the same way and killed lying there (vr_test_kill 1): its corpse never stirs (no move after it).
#   off   vr_knockdown_stir 0: shoved down the same way, no move at all.
# Reads vr_knockdown_test 9's stircheck: line (moves so far, stirring now) and vr_knockdown_debug's stirred: lines.
AGENT=${1:-takeslight}
source "$(dirname "$0")/../qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
F="stircheck:|stirred:|kdstir:"
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
# The stircheck: line after each mark: "moves stirring".
after() { awk -v m="kdstir: $2" '$0 ~ m {f=1; next} f && /^stircheck:/ {for(i=1;i<=NF;i++){split($i,a,"="); if(a[1]=="moves") mv=a[2]; if(a[1]=="stirring") st=a[2]; if(a[1]=="down") dn=a[2]} print mv, st, dn; exit}' "$1"; }
read dm ds dd <<<"$(after "$S/kdstir_on.log" down)"
reach=$(awk '/kdstir: down/{exit} /stirred:/{for(i=1;i<=NF;i++) if($i=="turned") {v=$(i+1)+0; if(v>m) m=v}} END{print m+0}' "$S/kdstir_on.log")
check $([ -n "$dm" ] && [ "$dm" -ge 1 ] && awk -v r="$reach" 'BEGIN{exit !(r >= 4 && r <= 60)}' && echo 1 || echo 0) \
    "down 5 s: it stirred ($dm moves; a joint turned $reach deg at most: a few degrees, not a struggle)"
read um us ud <<<"$(after "$S/kdstir_on.log" up)"
check $([ "$us" = 0 ] && [ "$ud" = 0 ] && echo 1 || echo 0) "got up: not stirring (living ones down $ud, stirring $us)"
read km ks kd <<<"$(after "$S/kdstir_kill.log" killed)"
read lm ls ld <<<"$(after "$S/kdstir_kill.log" later)"
check $([ -n "$km" ] && [ "$kd" = 0 ] && [ "$ks" = 0 ] && [ "$lm" = "$km" ] && [ "$ls" = 0 ] && echo 1 || echo 0) \
    "killed while down: its corpse never stirs (living ones down $kd; moves $km at the kill, $lm 3 s later; stirring $ks, $ls)"
read om os od <<<"$(after "$S/kdstir_off.log" down)"
check $([ "$om" = 0 ] && echo 1 || echo 0) "vr_knockdown_stir 0: down 5 s, no move ($om)"
verdict; exit $fail
