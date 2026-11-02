#!/bin/bash
# tunables_test.sh <agent> -- live tuning's round trip (Quake/vr/vr_tunables.cpp; a development build), headless, no map:
#   run 1: three constants set (tune; a Tunables page row; a text), every reader checked (C++, the QC's cvar() and
#     cvar_hget's stand-in), set to its default another way (not changed), tunables.cfg written and checked, the
#     values put back and the file read again; quit writes it (only the changed ones);
#     run 1 first checks that the baseline config's stale lines (older than sparse configs) set no constant;
#   tune_promote.py --check on it: the three would be promoted into vr_tunables.inc (a copy; nothing edited);
#     --check --quiet: exit 2 and the three lines;
#   run 2 (a new start, QVR_KEEP_TUNABLES=1: run.ps1 keeps run 1's file): the three read from tunables.cfg,
#     tune_reset <name> and tune_reset; quit removes the file. Then it is left empty (an empty file reads as none);
#     --check --quiet on that and on no file: exit 0, nothing printed;
#     and Debug > Tunables' submenus (vr_tunables_tree): every constant on one, reached from the Tunables page;
#   run 3: a setting that became a constant: a sparse config's line taken, kept in tunables.cfg at quit, with one console
#     line; --check --quiet sees it.
# Ends on the verdict line "tunables_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CFG=$TREE/quakevr/tunables.cfg
FAIL=0
PASS=0

OUT1=$(QVR_KEEP_TUNABLES= bash $KIT/run.sh $AGENT -Script "vr_tunables_test oldconfig;vr_tunables_test set;vr_tunables_test write;vr_tunables_test checkfile;vr_tunables_test reload;vr_tunables_test done;toggleconsole;quit" \
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
# quiet_check <file> <wanted exit> <wanted lines>
quiet_check() {
    QUIET=$(python $TREE/Misc/quakevr/tune_promote.py "$1" --check --quiet)
    CODE=$?
    LINES=$(printf '%s' "$QUIET" | grep -c "")
    if [ "$CODE" = "$2" ] && [ "$LINES" = "$3" ]; then
        PASS=$((PASS + 1))
    else
        echo "tunables_test: FAIL --check --quiet on $1: exit $CODE, $LINES lines (want $2, $3)"; FAIL=$((FAIL + 1))
    fi
}
quiet_check "$CFG" 2 3

OUT2=$(QVR_KEEP_TUNABLES=1 bash $KIT/run.sh $AGENT -Script "vr_tunables_tree;vr_tunables_test checkloaded;vr_tunables_test reset;vr_tunables_test done;toggleconsole;quit" \
    -Filter "^tunables_test:|^tunables tree:|rror|CRASH" -QuitOn "^tunables_test: [0-9]+ passed" -Timeout 120 2>&1 | grep -v "^$")
echo "$OUT2" | grep -v "PASS"
echo "$OUT2" | grep -q "^tunables_test: [1-9][0-9]* passed, 0 failed" && PASS=$((PASS + 1)) || FAIL=$((FAIL + 1))
echo "$OUT2" | grep "^tunables tree:"
echo "$OUT2" | grep -q "^tunables tree: .*: PASS" && PASS=$((PASS + 1)) || FAIL=$((FAIL + 1))
if [ -f "$CFG" ]; then
    echo "tunables_test: FAIL $CFG still there after tune_reset and quit"; FAIL=$((FAIL + 1))
else
    PASS=$((PASS + 1))
fi
quiet_check "$CFG.none" 0 0
: > "$CFG" # left empty, as run.ps1 leaves it (an empty file reads as none)
quiet_check "$CFG" 0 0

OUT3=$(QVR_KEEP_TUNABLES= bash $KIT/run.sh $AGENT -Script "vr_tunables_test configline;vr_tunables_test done;toggleconsole;quit" \
    -Filter "^tunables_test:|tuning constant now|rror|CRASH" -QuitOn "^tunables_test: [0-9]+ passed" -Timeout 120 2>&1 | grep -v "^$")
echo "$OUT3" | grep -v "PASS"
echo "$OUT3" | grep -q "^tunables_test: [1-9][0-9]* passed, 0 failed" && PASS=$((PASS + 1)) || FAIL=$((FAIL + 1))
[ "$(echo "$OUT3" | grep -c "tuning constant now")" = "1" ] && PASS=$((PASS + 1)) || { echo "tunables_test: FAIL not one console line"; FAIL=$((FAIL + 1)); }
if grep -q 'tune vr_throw_gain_lo "2.25"' "$CFG" 2>/dev/null; then
    PASS=$((PASS + 1))
else
    echo "tunables_test: FAIL the config's value not kept in $CFG"; FAIL=$((FAIL + 1))
fi
quiet_check "$CFG" 2 2
: > "$CFG" # left empty, as run.ps1 leaves it (an empty file reads as none)
echo "tunables_test: $PASS passed, $FAIL failed (stages)"
[ $FAIL -eq 0 ]
