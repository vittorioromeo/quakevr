#!/bin/bash
# saveload_test.sh <agent> -- shoves still make active ragdolls after a save is loaded (ROUND22.md, "No active ragdolls
# after a load"). e1m1, god, vr_knockdown_chance 100: the nearest standing monster shoved two-handed (vr_knockdown_test 22)
# in each phase: fresh (the map from the menu), load (saved, killed, the save loaded: the respawn's and the quickload's
# path), restart (killed, `restart`: the death's respawn), changelevel (e1m2), crossload (e1m1's save loaded from e1m2).
# Each phase also prints its nearest monster's knockdown chance handle (vr_knockdown_test 10's `chance handle`; 0: the
# field lost, so no shove, leg grab or lift ragdolls it). PASS when every phase's shove found a monster and staggered or
# knocked it down, and no handle is 0.
AGENT=${1:?}
KIT=C:/OHWorkspace/qvr-kit
LOG=C:/OHWorkspace/qvr-agents/$AGENT/scratch/saveload_test_$$.log
SHOVE="vr_knockdown_test 10;vr_knockdown_test 22;wait60"
S="map e1m1;developer 1;god;wait30;vr_knockdown_chance 100"
S="$S;echo PHASE fresh;$SHOVE"
S="$S;save ragload_a;wait10;kill;wait30;load ragload_a;wait60;god;echo PHASE load;$SHOVE"
S="$S;kill;wait30;restart;wait60;god;echo PHASE restart;$SHOVE"
S="$S;changelevel e1m2;wait60;god;echo PHASE changelevel;$SHOVE"
S="$S;load ragload_a;wait60;god;echo PHASE crossload;$SHOVE"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "PHASE|knockdown test|bash hit|whynot: .*chance handle|rror|CRASH" -Timeout 400 > "$LOG" 2>&1
grep -E "PHASE|knockdown test|bash hit|chance handle|rror|CRASH" "$LOG"
awk '
/PHASE / { p = $2; phases[++n] = p; next }
/two-handed shove of/ { found[p] = 1 }
/bash hit:/ && (/staggered/ || /knocked down/) { hit[p] = 1 }
/chance handle 0 / { lost[p] = 1 }
END {
    bad = 0
    for (i = 1; i <= n; i++) {
        q = phases[i]
        ok = found[q] && hit[q] && !lost[q]
        printf "%s: %s (shove %d, stagger %d, handle lost %d)\n", q, ok ? "ok" : "FAIL", found[q], hit[q], lost[q]
        if (!ok) bad = 1
    }
    if (n != 5) { print "FAIL: " n " of 5 phases ran"; bad = 1 }
    print bad ? "FAIL" : "PASS"
}' "$LOG"
