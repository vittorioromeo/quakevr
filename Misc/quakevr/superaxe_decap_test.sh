#!/bin/bash
# superaxe_decap_test.sh <agent> -- a fast and a slow two-handed Super Axe slash at the neck behead (ROUND25.md "axelag"):
# vrtrailer_test.sh's scene with its swing made fast (0.16 s, vrtrailer_swing_fast.mock) and slow (0.5 s,
# vrtrailer_swing_slow.mock; both vrtrailer_swing.py --duration, the shipped Super Axe and hand settings). Each run ends on
# its own (the trailer test's quit); prints "superaxe_decap: N passed, M failed" and exits 1 on a failure.
AGENT=$1
HERE=$(dirname "$0")
TREE=C:/OHWorkspace/qvr-agents/$AGENT
pass=0; failn=0
for kind in fast slow; do
    MOCK=$TREE/Misc/quakevr/vrtrailer_swing_$kind.mock bash $HERE/vrtrailer_test.sh $AGENT > $TREE/scratch/superaxe_decap_$kind.log 2>&1
    cp $TREE/scratch/vrtrailer_test.log $TREE/scratch/superaxe_decap_${kind}_game.log # (the game's lines, kept per swing)
    cut=$(grep -m1 "PASS the swing beheaded him\|FAIL the swing beheaded him" $TREE/scratch/superaxe_decap_$kind.log)
    if echo "$cut" | grep -q "^vrtrailer: PASS"; then
        pass=$((pass + 1)); echo "superaxe_decap: PASS $kind slash: ${cut:39:110}"
    else
        why=$(awk '/STEP swing/,/STEP reset/' $TREE/scratch/superaxe_decap_${kind}_game.log | grep -a -m1 "decap: no\|agarmor\|smallgib" | cut -c1-110)
        failn=$((failn + 1)); echo "superaxe_decap: FAIL $kind slash: no beheading ($why)"
    fi
done
echo "superaxe_decap: $pass passed, $failn failed"
[ $failn -eq 0 ]
