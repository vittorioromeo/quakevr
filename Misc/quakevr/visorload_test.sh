#!/bin/bash
# visorload_test.sh <agent> -- the headset's centre popup through map loads (ROUND25.md "visorcrash"), VR mock, fast mode.
# The popup (vr_visor.cpp renderHeadset, from VR_End2D) steps the visor's update() also while a map loads: a console print
# redraws the screen. It used to read r_viewleaf there, still the freed old world's leaf (the access violation at
# vr_visor.cpp:230 of 11-22, ~98 s into scenario_check). Three map loads (a reload, another map, back), then
# vr_center_panel_info's counts:
#   loading updates  > 0: the popup did run while the maps loaded (the path is covered)
#   stale view leafs = 0: r_viewleaf never pointed outside the current world there (Host_ClearMemory clears it)
# Prints PASS/FAIL lines and "visorload_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
mkdir -p "$S"
LOG=$S/visorload_test.log
SCRIPT="map vrtesthall; wait30; map vrtesthall; wait30; map e1m1; wait30; map vrtesthall; wait30; vr_center_panel_info; echo visorload_test: end"
bash $KIT/run.sh $AGENT -ExtraArgs "-loopbackonly" -QuitOn "visorload_test: end" -Timeout 300 \
    -Filter "center popup|visorload|CRASH|Exception" -Script "$SCRIPT; toggleconsole; quit" > "$LOG" 2>&1
PASS=0
FAIL=0
LINE=$(tr -d '\r\n' < "$LOG" | grep -o "loading updates [0-9]*, stale view leafs [0-9]*" | tail -1) # (the console wraps it)
LOADING=$(echo "$LINE" | sed -n 's/.*loading updates \([0-9]*\).*/\1/p')
STALE=$(echo "$LINE" | sed -n 's/.*stale view leafs \([0-9]*\).*/\1/p')
if grep -q "CRASH\|Exception" "$LOG"; then
    echo "FAIL crash: $(grep -m1 "Exception" "$LOG")"; FAIL=$((FAIL + 1))
fi
if [ -n "$LOADING" ] && [ "$LOADING" -gt 0 ]; then
    echo "PASS loading updates $LOADING"; PASS=$((PASS + 1))
else
    echo "FAIL loading updates '${LOADING}' (the popup never ran while a map loaded)"; FAIL=$((FAIL + 1))
fi
if [ "$STALE" = "0" ]; then
    echo "PASS stale view leafs 0"; PASS=$((PASS + 1))
else
    echo "FAIL stale view leafs '${STALE}'"; FAIL=$((FAIL + 1))
fi
echo "visorload_test: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
