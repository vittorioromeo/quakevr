#!/bin/bash
# scenario_check.sh <agent> -- every checklist Test scene (quakevr/scenarios/*.txt) read and started headless, in
# Arcane Dimensions' game folders (its ad_ maps found), with a toolgun entry selected as in his game:
#   vr_scenario validate   each file's map, entries, settings and commands (SCNOK/SCNERR), each toolgun entry found
#                          while selected (SCNSEL), every checklist tag's file (SCNSUM)
#   vr_scenario runall     each scene started and held 3 s of game time: none "not started", everything placed
#                          (SCNPLACED), still there (SCNKEPT), the given weapons still in hand (SCNHANDS), SCNRUN OK
# Ends on runall's SCNRUNSUM line (the game quits itself); prints PASS or FAIL and exits 1 on any failure.
# (ROUND22.md, "Checklist Tests that didn't start": the toolgun's selected entry broke every scene naming it.)
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
SEL=${SCENARIO_CHECK_SELECT:-Nailgun}
OUT=$(bash $KIT/run.sh $AGENT -Games "-game hipnotic -game rogue -game ad -game quakevr" \
    -Script "vr_toolgun_select $SEL;vr_scenario validate;vr_scenario runall quit" -QuitOn "^SCNRUNSUM" -Timeout 900 \
    -Filter "^SCN|not started|vr_scenario: |rror|CRASH|TIMEOUT|exit=" 2>&1 | grep -v "^$")
LOG=C:/OHWorkspace/qvr-agents/$AGENT/scratch/scenario_check.log
echo "$OUT" > $LOG # (every line: SCNPLACED, SCNKEPT, SCNHANDS per scene)
echo "$OUT" | grep -v "^SCNOK|\|^SCNRUN|.*|OK|\|^SCNHANDS|.*|HELD|\|^SCNPLACED|\|^SCNKEPT|\|loading \|put back\|^The crucial error\|^exit=0"
fail=0
if echo "$OUT" | grep -q "^SCNERR|\|not started\|^SCNRUN|.*|FAIL|\|^SCNHANDS|.*|LOST|\|ENGINE\|TIMEOUT"; then fail=1; fi
if ! echo "$OUT" | grep -q "^SCNSUM|.*|0 with errors|.*|0 missing"; then fail=1; fi
if ! echo "$OUT" | grep -q "^SCNSEL|.*|0 not"; then fail=1; fi
if ! echo "$OUT" | grep -q "^SCNRUNSUM|.*|0 failed"; then fail=1; fi
echo "$OUT" | grep "^SCNSUM\|^SCNSEL\|^SCNRUNSUM"
if [ $fail = 0 ]; then echo "scenario_check: PASS"; else echo "scenario_check: FAIL"; fi
exit $fail
