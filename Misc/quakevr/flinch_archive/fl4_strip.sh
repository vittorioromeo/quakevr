#!/bin/bash
# A side-view frame strip: one grunt, one test hit ($1) along +y, mode $2, terrain $3, into scratch/$4.png; the player
# 150 units to its -x side looking +x; a shot every $6 frames (default 8, ~0.11 s), 8 shots from the hit.
T=${1:-1}; M=${2:-2}; WHERE=${3:-flat}; OUTN=${4:-strip}; EXTRA="$5"; EVERY=${6:-8}
X=-240; Y=100; Z=24
[ "$WHERE" = slope ] && X=-160 && Y=540 && Z=60
[ "$WHERE" = stairs ] && X=200 && Y=592 && Z=140
PX=$(( X - 150 ))
SHOTS="screenshot"
for k in 1 2 3 4 5 6 7; do SHOTS="$SHOTS; wait$EVERY; screenshot"; done
S="map vrtesthall; wait30; god; notarget; developer 1; vr_flinch_debug 1; vr_flinch $M; $EXTRA; setpos $PX $Y $Z 0 0 0; vr_flinch_test_x $X; vr_flinch_test_y $Y; vr_flinch_test_z $Z; vr_flinch_test_yaw 90; vr_flinch_test 20; wait90; vr_flinch_test_dist 64; vr_flinch_test $T; $SHOTS; wait5; toggleconsole; quit"
bash C:/OHWorkspace/qvr-kit/run.sh fastship -Clean -Script "$S" -Out C:/OHWorkspace/qvr-agents/fastship/scratch/$OUTN.png -Filter "flinch: [0-9]+ (result|monster_army struck)|ENGINE" -Timeout 120 | cut -c1-160
