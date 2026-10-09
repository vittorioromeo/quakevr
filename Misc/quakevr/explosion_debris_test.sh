#!/bin/bash
# explosion_debris_test.sh <agent> -- an explosion's chunks as the server's Box3D props (vr_explosiondebris.cpp;
# ROUND21.md, "Explosion debris as Box3D bodies"), headless:
#   1. the cap and the life: ten test explosions of 12 with Maximum Active Chunks 40 leave 40, the oldest retired
#      (80); 5.5 s later none (lives 2-4 s);
#   2. multiplayer (maxplayers 4): vr_explosion_debris_mp_max 10 caps them at 10, -1 as single player (40), 0 none;
#   3. at rest they sleep: three dropped on vrtesthall's floor rest (asleep, still) within 2 s;
#   4. no tunnelling: at 1900 units/s into vrtesthall's south panel and west panel and down onto the table, and into
#      vrclimb's lift (a kinematic body) side-on, each stays on its near side;
#   5. riding: one on vrclimb's lift rises with it (8 units/s), one on its plat goes down with it (40 units);
#   6. a blast throws resting chunks; chunks thrown into a health box leave it where it was (20 g against 6 kg);
#   7. vr_physics_blast (the tests' explosion, a console command): its explosion is sent and makes its 12 chunks (until
#      2026-10-09 written to the datagram before the frame cleared it: no explosion, no chunks).
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
run() { bash $KIT/run.sh $AGENT -Script "$1;toggleconsole;quit" -Filter "^explosiondebris:|^chunk [0-9]|^ *[0-9]+:maps/b_bh|^vr_physics_spawn|vr_explosion_debris_launch: chunk" 2>&1 | tr -d '\r'; }
# The n-th stats line's field `name` (live, evicted, ...).
stat() { echo "$1" | grep "^explosiondebris:" | sed -n "${2}p" | grep -o "$3=[0-9]*" | head -1 | cut -d= -f2; }
# Chunk `num`'s line in the n-th listing: "x y z speed state".
chunkAt() { echo "$1" | awk -v n="$2" -v c="chunk $3:" 'index($0, c) == 1 { k[c]++; if (k[c] == n) print $4, $5, $6, $8, $9 }'; }
# The k-th chunk launched (its entity number).
nth() { echo "$1" | grep -o "vr_explosion_debris_launch: chunk [0-9]*" | sed -n "${2}p" | awk '{ print $3 }'; }
SET="vr_explosion_debris 1;vr_explosion_debris_count 12;vr_explosion_debris_life_min 2;vr_explosion_debris_life_max 4;vr_explosion_debris_max 40;vr_explosion_debris_mp_max 24"
T="vr_explosion_debris_test;wait2"

# 1. The cap and the life.
log=$(run "$SET;map e1m1;wait30;$T;$T;$T;$T;$T;$T;$T;$T;$T;$T;vr_explosion_debris_stats;wait396;vr_explosion_debris_stats")
echo "  single player: live $(stat "$log" 1 live), evicted $(stat "$log" 1 evicted); 5.5 s later live $(stat "$log" 2 live)"
check "$([ "$(stat "$log" 1 live)" = 40 ] && [ "$(stat "$log" 1 evicted)" = 80 ] && echo 1)" "Maximum Active Chunks: 40 left of 120, the oldest 80 retired"
check "$([ "$(stat "$log" 2 live)" = 0 ] && echo 1)" "their lives over (2-4 s): none left 5.5 s later"

# 7. vr_physics_blast's explosion (sent from a console command: the next frame's broadcast).
log=$(run "$SET;map e1m1;wait30;vr_physics_blast 500 -255 50 1;wait3;vr_explosion_debris_stats")
check "$([ "$(stat "$log" 1 live)" = 12 ] && echo 1)" "vr_physics_blast: its explosion makes its chunks ($(stat "$log" 1 live) of 12)"

# 2. Multiplayer's cap.
log=$(run "$SET;disconnect;maxplayers 4;map e1m1;wait30;vr_explosion_debris_mp_max 10;$T;$T;$T;vr_explosion_debris_stats;vr_explosion_debris_mp_max -1;$T;$T;$T;$T;vr_explosion_debris_stats;vr_explosion_debris_mp_max 0;wait2;vr_explosion_debris_stats")
echo "  multiplayer: mp_max 10: $(stat "$log" 1 live), -1: $(stat "$log" 2 live), 0: $(stat "$log" 3 live)"
check "$([ "$(stat "$log" 1 live)" = 10 ] && [ "$(stat "$log" 2 live)" = 40 ] && [ "$(stat "$log" 3 live)" = 0 ] && echo 1)" "multiplayer: Most in Multiplayer 10 caps at 10, -1 as single player (40), 0 none"

# 3, 4 (the world) and 6. vrtesthall: the health box spawned first (its entity printed), then the chunks.
L="vr_explosion_debris_launch"
log=$(run "$SET;map vrtesthall;wait30;vr_physics_spawn item_health 96;wait72;entities;$L -200 -200 20 0 0 0 2 20;$L -200 -150 20 150 0 0 2 20;$L -250 -100 20 -300 0 100 3 20;$L 0 -440 48 0 -1900 0 2 20;$L -384 384 140 0 0 -1900 2 20;$L -560 200 48 -1900 0 0 1.4 20;$L 2 -250 10 0 330 0 3.2 20;$L -2 -250 18 0 1900 0 3.2 20;wait144;vr_explosion_debris_list;entities;vr_physics_blast -180 -180 8 120;wait10;vr_explosion_debris_list")
rest=0; for k in 1 2 3; do c=$(nth "$log" $k); read x y z s st <<< "$(chunkAt "$log" 1 $c)"; [ "$st" = resting ] && [ "$s" = 0.0 ] && rest=$((rest + 1)); done
check "$([ $rest = 3 ] && echo 1)" "three dropped on the floor rest (asleep, still) within 2 s ($rest of 3)"
read x y z s st <<< "$(chunkAt "$log" 1 $(nth "$log" 4))"; echo "  south panel (face y -496): y $y"
check "$(awk -v y="$y" 'BEGIN { print (y != "" && y > -496) ? 1 : 0 }')" "1900 units/s into the south panel: stays in front of it"
read x y z s st <<< "$(chunkAt "$log" 1 $(nth "$log" 5))"; echo "  the table (top z 32): $x $y $z"
check "$(awk -v x="$x" -v y="$y" -v z="$z" 'BEGIN { over = x > -448 && x < -320 && y > 352 && y < 416; print (z != "" && (!over || z > 32)) ? 1 : 0 }')" "1900 units/s down onto the table: on or above its top"
read x y z s st <<< "$(chunkAt "$log" 1 $(nth "$log" 6))"; echo "  west panel (face x -624): x $x"
check "$(awk -v x="$x" 'BEGIN { print (x != "" && x > -624) ? 1 : 0 }')" "1900 units/s into the west panel: stays in front of it"
# (the box by the number vr_physics_spawn gave it, in the two entity listings: "edict 60" was it until the map's
# entities changed, 2026-10-09)
hb=$(echo "$log" | grep -o "^vr_physics_spawn: [0-9]* item_health" | head -1 | awk '{ print $2 }')
box1=$(echo "$log" | grep -E "^ *$hb:maps/b_bh" | sed -n 1p | cut -d'[' -f1); box2=$(echo "$log" | grep -E "^ *$hb:maps/b_bh" | sed -n 2p | cut -d'[' -f1); echo "  health box $hb: $box1 / $box2"
check "$([ -n "$box1" ] && [ "$box1" = "$box2" ] && echo 1)" "chunks thrown into a health box (330 and 1900 units/s) leave it where it was"
moved=0; for k in 1 2; do c=$(nth "$log" $k); read x y z s st <<< "$(chunkAt "$log" 2 $c)"; awk -v s="$s" 'BEGIN { exit !(s > 50) }' && moved=$((moved + 1)); done
check "$([ $moved = 2 ] && echo 1)" "a blast beside resting chunks throws them ($moved of 2 faster than 50 units/s)"

# 4 (a kinematic body) and 5. vrclimb: the lift (func_train, x -300..-236, top z 40) and the plat (top z 48, down 40).
log=$(run "$SET;map vrclimb;wait30;$L -268 88 60 0 0 0 2 30;$L -130 120 20 -1900 0 0 2 30;wait72;vr_explosion_debris_list;setpos -318 88 24 0 0 0;wait720;vr_explosion_debris_list")
c=$(nth "$log" 1); read x1 y z1 s st <<< "$(chunkAt "$log" 1 $c)"; read x y z2 s st <<< "$(chunkAt "$log" 2 $c)"; echo "  on the lift: z $z1 -> $z2 in 10 s"
check "$(awk -v a="$z1" -v b="$z2" 'BEGIN { print (a != "" && b - a > 60) ? 1 : 0 }')" "one on the lift rises with it (8 units/s)"
read x y z s st <<< "$(chunkAt "$log" 2 $(nth "$log" 2))"; echo "  into the lift's side (x -236): x $x"
check "$(awk -v x="$x" 'BEGIN { print (x != "" && x > -236) ? 1 : 0 }')" "1900 units/s into the lift's side: stays out of it"
log=$(run "$SET;map vrclimb;wait30;$L -168 72 70 0 0 0 2 30;wait144;vr_explosion_debris_list;setpos -223 72 24 0 0 0;wait576;vr_explosion_debris_list")
c=$(nth "$log" 1); read x y z1 s st <<< "$(chunkAt "$log" 1 $c)"; read x y z2 s st2 <<< "$(chunkAt "$log" 2 $c)"; echo "  on the plat: z $z1 -> $z2 ($st2)"
check "$(awk -v a="$z1" -v b="$z2" 'BEGIN { print (a != "" && a - b > 36 && a - b < 44) ? 1 : 0 }')" "one on the plat goes down with it (40 units)"
exit $fail
