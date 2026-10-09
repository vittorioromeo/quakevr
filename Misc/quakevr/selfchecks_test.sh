#!/bin/bash
# selfchecks_test.sh <agent> -- the engine's own PASS/FAIL self-checks, headless on e1m1 (fresh map):
#   vr_hull_cachetest     the monster hull cache past its old 12-size limit: every compiled tree revisited, the same
#                         (vr_hull.cpp)
#   vr_physics_mtbench    Box3D's step with 1..N workers: the same world hash with every worker count (vr_box3d.cpp)
# Prints a PASS or FAIL line for each; exits 1 on a failure. (Debug-menu-only aids until 2026-10-09:
# TECHDEBT_2026-10-09.md, 5.)
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
OUT=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait30;vr_hull_cachetest;vr_physics_mtbench 200 300;toggleconsole;quit" \
    -Filter "^vr_hull_cachetest:|^vr_physics_mtbench: [a-zA-Z]|rror|CRASH" -Timeout 600 2>&1 | grep -v "^$" | grep -v "^exit=0")
echo "$OUT"
fail=0
if echo "$OUT" | grep -q "^vr_hull_cachetest: PASS"; then echo "PASS hull cache"; else echo "FAIL hull cache"; fail=1; fi
if echo "$OUT" | grep -q "^vr_physics_mtbench: the same with every worker count"; then echo "PASS physics workers"; else
    echo "FAIL physics workers"; fail=1; fi
exit $fail
