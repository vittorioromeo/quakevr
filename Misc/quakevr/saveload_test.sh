#!/bin/bash
# saveload_test.sh <agent> [flat|vr|both] -- a Test scene through a save and a load (vr_pickup_test 16,
# QC/vr_saveload_test.qc; ROUND25.md, "Save and load after a Test scene"): the waterdrips scene (vrtesthall's pool, two
# armed grunts, a shotgun in the main hand), everything near soaked, saved as qvr_saveload.sav and loaded. The QC
# checks the hands (the off hand still empty: no holster drawn by the load's grip) and the armed grunts (their guns at
# their hands, no other kind's spell); this script checks that what was wet before the save (vr_drips_info's "entity
# N: wet" lines, not in the pool) is wet after the load. Default both: flatscreen (-flatscreenmode) and the VR mock.
# Ends: `saveload_test: <p> passed, <f> failed`.
AGENT="$1"; MODE="${2:-both}"
KIT="C:/OHWorkspace/qvr-kit"
PASS=0; FAIL=0
one() {
    local label="$1" extra="$2"
    local log
    log=$(bash $KIT/run.sh $AGENT $extra -Script "vr_scenario waterdrips; vr_pickup_test 16" \
        -QuitOn "sltest: [0-9]+ passed" -Timeout 240 -Filter "sltest|^wet: coat|^  entity|^  your|^  hand|^  held|^  holstered|exit=" 2>&1 | tr -d '\r')
    echo "$log" | grep -E "sltest: (FAIL|[0-9]+ passed)" | sed "s/^/$label: /" | cut -c1-200
    local p f
    p=$(echo "$log" | grep -oE "sltest: [0-9]+ passed" | grep -oE "[0-9]+" | tail -1)
    f=$(echo "$log" | grep -oE "[0-9]+ failed" | grep -oE "[0-9]+" | tail -1)
    if [ -z "$p" ]; then
        echo "$label: FAIL no verdict (timed out?)"; FAIL=$((FAIL + 1)); return
    fi
    PASS=$((PASS + p)); FAIL=$((FAIL + f))
    # The wet things out of the pool before the save, each still wet after the load.
    local before after n=0 lost=0
    before=$(echo "$log" | sed -n '/sltest: wet before/,/sltest: wet after/p' | grep -E "^  entity [0-9]+: wet" | grep -v "(in it)" | grep -oE "entity [0-9]+" | grep -oE "[0-9]+")
    after=$(echo "$log" | sed -n '/sltest: wet after/,$p' | grep -E "^  entity [0-9]+: wet (0\.[0-9]*[1-9]|1)" | grep -oE "entity [0-9]+" | grep -oE "[0-9]+")
    for e in $before; do
        n=$((n + 1))
        echo "$after" | grep -qx "$e" || lost=$((lost + 1))
    done
    if [ "$n" -gt 0 ] && [ "$lost" -eq 0 ]; then
        echo "$label: PASS $n wet things kept through the load"; PASS=$((PASS + 1))
    else
        echo "$label: FAIL $lost of $n wet things dry after the load"; FAIL=$((FAIL + 1))
    fi
    # The shine (the wet coat, derived from the wetness): each of the five the info lists, shining before, shines after.
    local cb ca k bad=0 shining=0
    cb=$(echo "$log" | sed -n '/sltest: wet before/,/sltest: wet after/p' | grep -E "^wet: coat" | grep -oE "[0-9]+\.[0-9]+" | head -5)
    ca=$(echo "$log" | sed -n '/sltest: wet after/,$p' | grep -E "^wet: coat" | grep -oE "[0-9]+\.[0-9]+" | head -5)
    for k in 1 2 3 4 5; do
        local b a
        b=$(echo "$cb" | sed -n "${k}p"); a=$(echo "$ca" | sed -n "${k}p")
        if awk "BEGIN{exit !(${b:-0} > 0.01)}"; then
            shining=$((shining + 1))
            awk "BEGIN{exit !(${a:-0} > 0.01)}" || bad=$((bad + 1))
        fi
    done
    if [ "$shining" -gt 0 ] && [ "$bad" -eq 0 ]; then
        echo "$label: PASS $shining shining things (wet coat) still shine after the load"; PASS=$((PASS + 1))
    else
        echo "$label: FAIL $bad of $shining shining things lost their shine after the load"; FAIL=$((FAIL + 1))
    fi
}
[ "$MODE" = "vr" ] || one flat "-ExtraArgs -flatscreenmode"
[ "$MODE" = "flat" ] || one vr ""
echo "saveload_test: $PASS passed, $FAIL failed"
