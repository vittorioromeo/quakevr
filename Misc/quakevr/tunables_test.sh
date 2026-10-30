#!/bin/bash
# tunables_test.sh <agent> -- live tuning's round trip (Quake/vr/vr_tunables.cpp; a development build), headless, no map:
#   run 1: three constants set (tune; a Tunables page row; a text), every reader checked (C++, the QC's cvar() and
#     cvar_hget's stand-in), set to its default another way (not changed), tunables.cfg written and checked, the
#     values put back and the file read again; quit writes it (only the changed ones);
#   tune_promote.py --check on it: the three would be promoted into vr_tunables.inc (a copy; nothing edited);
#   run 2 (a new start): the three read from tunables.cfg, tune_reset <name> and tune_reset; quit removes the file.
# Ends on the verdict line "tunables_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CFG=$TREE/quakevr/tunables.cfg
FAIL=0
PASS=0

OUT1=$(bash $KIT/run.sh $AGENT -Script "vr_tunables_test set;vr_tunables_test write;vr_tunables_test checkfile;vr_tunables_test reload;vr_tunables_test done;toggleconsole;quit" \
    -Filter "^tunables_test:|rror|CRASH" -QuitOn "^tunables_test: [0-9]+ passed" -Timeout 120 2>&1 | grep -v "^$")
echo "$OUT1" | grep -v "PASS"
echo "$OUT1" | grep -q "^tunables_test: [1-9][0-9]* passed, 0 failed" && PASS=$((PASS + 1)) || FAIL=$((FAIL + 1))

if [ -f "$CFG" ] && grep -q 'tune vr_throw_gain_lo "2.25"' "$CFG"; then
    PASS=$((PASS + 1))
else
    echo "tunables_test: FAIL quit did not write $CFG"; FAIL=$((FAIL + 1))
fi
PROMOTE=$(python $TREE/Misc/quakevr/tune_promote.py "$CFG" --check)
echo "$PROMOTE" | tail -1
echo "$PROMOTE" | grep -q "^tune_promote: 3 promoted, 0 unchanged, 0 unknown" && PASS=$((PASS + 1)) || FAIL=$((FAIL + 1))

OUT2=$(bash $KIT/run.sh $AGENT -Script "vr_tunables_test checkloaded;vr_tunables_test reset;vr_tunables_test done;toggleconsole;quit" \
    -Filter "^tunables_test:|rror|CRASH" -QuitOn "^tunables_test: [0-9]+ passed" -Timeout 120 2>&1 | grep -v "^$")
echo "$OUT2" | grep -v "PASS"
echo "$OUT2" | grep -q "^tunables_test: [1-9][0-9]* passed, 0 failed" && PASS=$((PASS + 1)) || FAIL=$((FAIL + 1))
if [ -f "$CFG" ]; then
    echo "tunables_test: FAIL $CFG still there after tune_reset and quit"; FAIL=$((FAIL + 1))
else
    PASS=$((PASS + 1))
fi
echo "tunables_test: $PASS passed, $FAIL failed (stages)"
[ $FAIL -eq 0 ]
