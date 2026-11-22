#!/bin/bash
# split_join_test.sh <agent> [shots dir] [prefix] -- splitscreen phase 10 (docs/vr-port/SPLITSCREEN.md section 20): the
# join screen, headless and flatscreen. Four runs (2 seats coop, 3 deathmatch, 4 coop, 4 deathmatch), each from no game:
# Multiplayer > Splitscreen; mock pads join by A, leave by B, join by Start; the Game row by the keyboard; a joined pad's
# Start starts the game (maxplayers 4): every seat signs on in its own slot, driven by its own pad, no menu opened by
# that Start; then over the running game a seat leaves by B and joins again by A, and its Start goes back to the game.
# Pictures: <shots>/<prefix>_<n><mode>.png (the join screen, then the game in tiles). Verdict per run
# "split_join_test: N passed, M failed"; the script's own last line "split_join_test.sh: N passed, M failed".
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
SHOTS=${2:-$S}
PREFIX=${3:-split10}
mkdir -p "$S"
P=0; F=0
for RUN in 2:coop 3:dm 4:coop 4:dm; do
    N=${RUN%%:*}; MODE=${RUN##*:}
    LOG=$S/split_join_test_$N$MODE.log
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "split_join_test: [0-9]+ passed" -Timeout 240 -Clean \
        -Out "$SHOTS/${PREFIX}_$N$MODE.png" \
        -Filter "^split_join_test|^seat |^splitscreen:|seat_join|Host_Error|SV_Error|Sys_Error" \
        -Script "vr_split_join_test $N $MODE" > "$LOG" 2>&1
    echo "== $N $MODE"
    grep -a "split_join_test: FAIL\|split_join_test: [0-9]* passed\|Host_Error\|SV_Error\|Sys_Error" "$LOG"
    V=$(grep -a "split_join_test: [0-9]* passed" "$LOG" | tail -1)
    if [ -z "$V" ]; then F=$((F + 1)); echo "split_join_test: FAIL $N $MODE: no verdict (see $LOG)"; continue; fi
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
done
echo "split_join_test.sh: $P passed, $F failed"
[ $F -eq 0 ]
