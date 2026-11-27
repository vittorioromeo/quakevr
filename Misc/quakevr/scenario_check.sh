#!/bin/bash
# scenario_check.sh <agent> -- every checklist Test scene (quakevr/scenarios/*.txt) read and started headless, in
# Arcane Dimensions' game folders (its ad_ maps found), with a toolgun entry selected as in his game:
#   vr_scenario validate   each file's map, entries, settings and commands (SCNOK/SCNERR), each toolgun entry found
#                          while selected (SCNSEL), every checklist tag's file (SCNSUM)
#   vr_scenario runall     each scene started and held 3 s of game time: none "not started", everything placed
#                          (SCNPLACED), still there (SCNKEPT), the given weapons still in hand (SCNHANDS), SCNRUN OK;
#                          SCNRUN SKIP (not a failure) when a scene names an add-on's entry this base lacks (Dwell, AD);
#                          an mp scene's local game and bots (SCNMP; SCNBOT: each still in, a passive one unmoved, its
#                          weapon held). The game runs -loopbackonly (an mp scene opens no socket anyway).
# Ends on runall's SCNRUNSUM line (the game quits itself); prints PASS or FAIL and exits 1 on any failure.
# (ROUND22.md, "Checklist Tests that didn't start": the toolgun's selected entry broke every scene naming it.)
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
SEL=${SCENARIO_CHECK_SELECT:-Nailgun}
OUT=$(bash $KIT/run.sh $AGENT -ExtraArgs "-loopbackonly" -Games "-game hipnotic -game rogue -game ad -game quakevr" \
    -Script "vr_toolgun_select $SEL;vr_scenario validate;vr_scenario runall quit" -QuitOn "^SCNRUNSUM" -Timeout 900 \
    -Filter "^SCN|not started|vr_scenario: |rror|CRASH|TIMEOUT|exit=" 2>&1 | grep -v "^$")
LOG=C:/OHWorkspace/qvr-agents/$AGENT/scratch/scenario_check.log
echo "$OUT" > $LOG # (every line: SCNPLACED, SCNKEPT, SCNHANDS per scene)
echo "$OUT" | grep -v "^SCNOK|\|^SCNRUN|.*|OK|\|^SCNHANDS|.*|HELD|\|^SCNPLACED|\|^SCNKEPT|\|^SCNMP|\|^SCNBOT|\|loading\|put back\|^The crucial error\|^exit=0"
fail=0
# A Map Library package scene (quoth_lpqsp1, quoth_metmon, quoth_ne_marb: `package` line) whose package isn't installed
# here says so (SCNERR, not started, SCNRUN FAIL, by design: it never downloads) and is not a failure, as a missing add-on's
# SKIP: its lines are left out and the validate/runall counts may hold exactly that many (beta6, 2026-11-27).
PKG="[0-9]|the Map Library package " # (its line's start: run.sh wraps the long lines)
NPKG=$(echo "$OUT" | grep -c "^SCNRUN|.*|FAIL|.*$PKG") # (one SCNRUN line per scene; no sort: Windows' sort.exe can come first on PATH)
REST=$(echo "$OUT" | grep -v -- "$PKG")
[ "$NPKG" -gt 0 ] && echo "scenario_check: $NPKG Map Library package scene(s) not run: package not installed (not a failure)"
if echo "$REST" | grep -q "^SCNERR|\|not started\|^SCNRUN|.*|FAIL|\|^SCNHANDS|.*|LOST|\|ENGINE\|TIMEOUT"; then fail=1; fi
if ! echo "$OUT" | grep -q "^SCNSUM|.*|$NPKG with errors|.*|0 missing"; then fail=1; fi
if ! echo "$OUT" | grep -q "^SCNSEL|.*|0 not"; then fail=1; fi
if ! echo "$OUT" | grep -q "^SCNRUNSUM|.*|$NPKG failed"; then fail=1; fi
echo "$OUT" | grep "^SCNSUM\|^SCNSEL\|^SCNRUNSUM"
if [ $fail = 0 ]; then echo "scenario_check: PASS"; else echo "scenario_check: FAIL"; fi
exit $fail
