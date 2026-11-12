#!/bin/bash
# flatswing_test.sh <agent> -- flat-screen play: the axe's overhead swing (vr_flat_swing) lands its blow once, at a wall and
# on a monster, at a short strike too (ROUND24.md, "The swing's wall blow and the raised swing"). Headless, fast mode;
# each run ends on its own last line (-QuitOn); prints "flatswing: N passed, M failed".
#   wall075, wall14  from 10 standing places off vrtesthall's wall (y 640: 612 to 626), the swing at the wall with
#                    vr_flat_swing_strike 0.075 (his) and 0.14 (the old default): exactly one wall blow from each. (Before
#                    the stop on the swing's own path, vr_flat_swing_path_stop: 4 and 2 places with none.)
#   pathoff          the control: vr_flat_swing_path_stop 0 at 0.075 misses at some place (the test sees the bug).
#   knight           the swing raised behind the head (vr_flat_swing_raise_fwd -12, strike 0.075) at a fresh knight 20 to
#                    28 units ahead, 8 times, and the default swing 3 times: never two blows on the knight in a swing,
#                    and at least 9 of the 11 land (random: a knight now and then is not where it spawned, or past the default swing's reach at 28); damage printed. (Its windup landed a weak blow, x0.5, that knocked the knight out of the strike's way: 11
#                    to 24 damage; vr_flat_swing_wind_strikes 0 since.)
# Each wait is the swing's own frames (0.73 s at 0.075, 0.79 at 0.14: 53 and 57 frames; 60 counted) or a settle.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatswing
mkdir -p "$S"
FLAT="-ExtraArgs -flatscreenmode"
YS="612 614 616 618 619 620 621 622 624 626"

# wallrun <tag> <strike> [extra]: the 10 places' swings, one run; then "<y>:<wall blows>" per place in $S/<tag>.txt.
wallrun() { local tag=$1 st=$2 ex=$3 sc y
    sc="map vrtesthall; god; notarget; developer 1; wait5; $ex tune vr_flat_swing_strike $st; impulse 152; wait10"
    for y in $YS; do sc="$sc; setpos 0 $y 25 0 90 0; wait30; echo flatswing: at $y; vr_flat_punch main; wait60"; done
    sc="$sc; tune_reset vr_flat_swing_strike; tune_reset vr_flat_swing_path_stop; echo flatswing: $tag done"
    bash $KIT/run.sh $AGENT $FLAT -Script "$sc" -QuitOn "^flatswing: $tag done" -Timeout 180 -Filter "^(melee event|flatswing)" > "$S/$tag.log" 2>&1
    awk '/^flatswing: at/{y=$3; n[y]=0; o[++k]=y} /^melee event: .* a wall/{n[y]++} END{for(i=1;i<=k;i++) printf "%s:%d ", o[i], n[o[i]]}' "$S/$tag.log" > "$S/$tag.txt"; }
ones() { tr ' ' '\n' < "$S/$1.txt" | grep -c ":1$"; }

wallrun wall075 0.075
check $([ "$(ones wall075)" = 10 ] && echo 1 || echo 0) "wall075: strike 0.075, one wall blow from each of 10 places ($(cat $S/wall075.txt))"
wallrun wall14 0.14
check $([ "$(ones wall14)" = 10 ] && echo 1 || echo 0) "wall14: strike 0.14, one wall blow from each of 10 places ($(cat $S/wall14.txt))"
wallrun pathoff 0.075 "tune vr_flat_swing_path_stop 0;"
check $([ "$(ones pathoff)" -lt 10 ] && echo 1 || echo 0) "pathoff: the control, the old stop towards the shoulder misses somewhere ($(cat $S/pathoff.txt))"

# The knight (vr_test_spawn 5) on vrtesthall's open floor, which has no monsters of its own: one knight at a time, fresh
# each swing; vr_test_hurt 1 reads it after (1 damage of its own; nothing when the swing gibbed it), vr_test_kill 1 then
# takes it away. (On e1m1 a knight the swing left alive stayed for the next, which then struck both or read the old one.)
# (Each swing 64 units further east, x -320 to 320: away from the last one's corpse and gibs.)
sc="sv_random_seed 1; map vrtesthall; god; notarget; developer 1; wait5; vr_test_spawn 5"
X=-320
trial() { sc="$sc; setpos $X 300 25 0 90 0; wait5; vr_test_spawn_dist $1; impulse 152; wait10; impulse 241; wait8; echo flatswing: swing $2 at $1; vr_flat_punch main; wait60; vr_test_hurt 1; wait5; vr_test_kill 1; wait10"; X=$((X + 64)); }
sc="$sc; tune vr_flat_swing_raise_fwd -12; tune vr_flat_swing_strike 0.075"
for d in 20 22 24 24 24 26 26 28; do trial $d raised; done
sc="$sc; tune_reset vr_flat_swing_raise_fwd; tune_reset vr_flat_swing_strike"
for d in 20 24 28; do trial $d default; done
sc="$sc; echo flatswing: knight done"
bash $KIT/run.sh $AGENT $FLAT -Script "$sc" -QuitOn "^flatswing: knight done" -Timeout 180 -Filter "^(melee event|flatswing|test hurt)" > "$S/knight.log" 2>&1
# Per swing: its blows on the knight (gibs are their own targets) and the damage: all 75 when the blow gibbed it (its
# gibs struck after it: the hurt read is then another monster), else 75 less the health read (less the read's own 1).
awk '/^flatswing: swing/{w=++k; s[w]=$3" "$5; n[w]=0; d[w]=-1; a[w]="?"} /^melee event: .*monster_knight *$/{n[w]++}
     /^melee event: .* a gib *$/{g[w]=1}
     /^test hurt: monster_knight/{a[w]=$(NF-2); for(i=1;i<=NF;i++) if($i=="->") d[w]=75-$(i-1)}
     /^test hurt: monster_(army|dog)/{d[w]=75; a[w]="dead"}
     END{for(w=1;w<=k;w++){if(g[w]){d[w]=75; a[w]="gibbed"} printf "%s:%d:%d:%s\n", s[w], n[w], d[w], a[w]}}' "$S/knight.log" > "$S/knight.txt"
echo "  (swing, distance: knight blows: damage: the knight's distance after)"
sed 's/^/  /' "$S/knight.txt"
double=$(awk -F: '$2>1' "$S/knight.txt" | wc -l)
landed=$(awk -F: '$2>=1' "$S/knight.txt" | wc -l)
check $([ "$double" = 0 ] && echo 1 || echo 0) "knight: never two blows on the knight in one swing ($double swings did)"
check $([ "$landed" -ge 9 ] && echo 1 || echo 0) "knight: $landed of 11 swings landed on it (8 raised behind the head at 0.075, 3 default; the damage is printed, flatmelee checks it)"

verdict; exit $fail
