#!/bin/bash
# ad_rig_sweep.sh <agent>: every ragdoll seed table's model derived (vr_ragdoll_rig_sweep, the rig cache off) with the
# mission packs' and Arcane Dimensions' data, before any map. Passes on "N rigged, 0 failed, 0 absent". A table measured
# with a weapon in the hand that a later change made loose (vr_monstermods.cpp knownSwords) fails here: "bone hand_r got
# 0 vertices" (ROUND23.md, "AD ragdolls lost again"). Needs AD's data in the agent's base (qbase/ad: its paks copied,
# never linked); without it: SKIP.
set -u
AGENT=${1:?worker name}
K=C:/OHWorkspace/qvr-kit
if [ ! -f "$K/bases/$AGENT/qbase/ad/pak0.pak" ]; then
    echo "ad_rig_sweep: SKIP (no AD data in bases/$AGENT/qbase/ad)"
    exit 0
fi
S="developer 1; vr_ragdoll_rig_cache 0; vr_ragdoll_rig_sweep; developer 0; toggleconsole; quit"
OUT=$(bash "$K/run.sh" "$AGENT" -Games '-game hipnotic -game rogue -game ad -game quakevr' -Script "$S" \
    -QuitOn 'vr_ragdoll_rig_sweep: [0-9]+ rigged' \
    -Filter 'no ragdoll|vr_ragdoll_rig_sweep|is not the model|Host_Error|ENGINE ERROR|CRASH' -Timeout 240)
echo "$OUT" | grep -v '^$'
if echo "$OUT" | grep -qE 'vr_ragdoll_rig_sweep: [0-9]+ rigged, 0 failed, 0 absent'; then
    echo "ad_rig_sweep: passed"
else
    echo "ad_rig_sweep: FAIL"
    exit 1
fi
