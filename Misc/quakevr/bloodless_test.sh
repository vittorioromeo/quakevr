#!/bin/bash
# bloodless_test.sh <agent>: shoves, weapon bashes and slaps draw no blood (ROUND22.md, "Shoves, slaps and bashes draw
# no blood"): vr_smallgibs_test 23 on a grunt spawned ahead (vrtesthall): each prints its gore hits, wound events, blood
# particles and small gibs; all 0 for shove, bash and slap, while their damage is dealt and a punch (the control) bleeds.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT="$TREE/scratch/bloodless_test.txt"
mkdir -p "$TREE/scratch"
bash "$KIT/run.sh" "$AGENT" -Script "map vrtesthall;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 64;impulse 241;wait10;vr_smallgibs_test 23;wait10;toggleconsole;quit" -Filter "sgibtest: bloodless|Error|CRASH" -Timeout 200 > "$OUT"
cat "$OUT" | grep "sgibtest"
FAIL=0
for k in shove bash slap; do
    LINE=$(grep "bloodless $k: gore" "$OUT")
    DMG=$(grep "bloodless $k: 10 on" "$OUT")
    if [ -z "$LINE" ] || [ -z "$DMG" ] || ! echo "$LINE" | grep -q "gore hits 0, wound events 0, blood particles 0"; then
        echo "$k: blood (or no line)"; FAIL=1
    fi
    if ! echo "$DMG" | grep -q ", 0 small gibs" || echo "$DMG" | grep -q " 0 damage"; then
        echo "$k: small gibs, or no damage dealt"; FAIL=1
    fi
done
if grep "bloodless punch: gore" "$OUT" | grep -q "gore hits 0, wound events 0, blood particles 0"; then
    echo "punch: no blood (the counters see nothing)"; FAIL=1
fi
if [ "$FAIL" = 1 ]; then
    echo "BLOODLESS FAILED"
    exit 1
fi
echo "BLOODLESS PASSED"
