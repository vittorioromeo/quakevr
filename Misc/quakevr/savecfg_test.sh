#!/bin/bash
# savecfg_test.sh <agent> -- the menu's Save Default (vr_propose_defaults) saves the config and tunables.cfg to disk
# right away, as quit does (Host_WriteConfiguration), headless, no map: a setting and a tuning constant changed,
# vr_propose_defaults, then the files on disk read back in the same run (vr_config_test hasline): ironwail.cfg and
# tunables.cfg hold both. The run is killed on the verdict line, never quits (as a crash would). (run.sh puts the
# config back and empties tunables.cfg itself.)
# Ends on the verdict line "savecfg_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
FAIL=0
PASS=0
GAIN=2.$((RANDOM % 900 + 100)) # a new value each run: the files hold this run's save, not an older one's
WIDTH=$((RANDOM % 8 + 17))

OUT=$(bash $KIT/run.sh $AGENT -Script "vr_hull_width $WIDTH;tune vr_throw_gain_lo $GAIN;vr_propose_defaults;vr_config_test hasline ironwail.cfg vr_hull_width $WIDTH;vr_config_test hasline tunables.cfg tune vr_throw_gain_lo $GAIN;vr_config_test done" \
    -Filter "^Saved|settings were not saved|^Wrote|^config_test|rror|CRASH" -QuitOn "^config_test: [0-9]+ passed" -Timeout 120 2>&1 | grep -v "^$")
echo "$OUT" | grep -v "PASS"
if echo "$OUT" | grep -q "^Saved your settings to disk: ironwail.cfg and tunables.cfg"; then
    PASS=$((PASS + 1))
else
    echo "savecfg_test: FAIL no confirmation naming both files"; FAIL=$((FAIL + 1))
fi
if echo "$OUT" | grep -q "^config_test: 4 passed, 0 failed"; then # (each file read, each line found)
    PASS=$((PASS + 1))
else
    echo "savecfg_test: FAIL the files on disk do not hold the changes before quit"; FAIL=$((FAIL + 1))
fi
echo "savecfg_test: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
