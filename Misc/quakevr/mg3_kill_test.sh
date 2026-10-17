#!/bin/bash
# Dawn of the Machine (MG3): kill chosen monsters of a map by rocket blasts, headless, and fail on a crash (a player's
# report: The Sacrifice, map8, "The crash happens when two vores are killed"; ROUND22.md, "The Sacrifice").
#   bash mg3_kill_test.sh <agent> [map] [monster k...]     (default map8 55 56: its two vores on the pedestals, whose
#                                                            deaths bring two item_spikes and the crushers)
# Each monster: vr_mg3_test 300+k (the k-th "monster_" entity, killed by a blast at its middle: its gore, limb cuts and
# ragdoll, then a trace in the same frame), then about two seconds for what its death sets off. Needs the owned
# rerelease MG3 data. Logs scratch/mg3kill_<map>.log. Exit status 0: every monster killed, no crash, no error, no
# trigger linked as a solid (the engine's "Trigger in clipping list", a developer warning since 1.1.0, a fatal error
# before), the map still running at the end.
NAME=${1:?agent}; MAP=${2:-map8}; shift; shift
KS=${*:-55 56}
KIT="C:/OHWorkspace/qvr-kit"; T="C:/OHWorkspace/qvr-agents/$NAME"; mkdir -p "$T/scratch"
LOG="$T/scratch/mg3kill_$MAP.log"
S="developer 1;vr_campaign_native mg3;skill 1;map $MAP;wait120;god;notarget"
N=0
for k in $KS; do S="$S;vr_mg3_test $((300 + k));wait150"; N=$((N + 1)); done
S="$S;echo mg3kill: done;toggleconsole;quit"
BADRE="clipping list|ENGINE|TIMEOUT|Host_Error|SV_Error|runaway|PR_ExecuteProgram"
bash "$KIT/run.sh" "$NAME" -ExtraArgs "-nomapindex -noaddons" -Timeout ${TMO:-300} \
    -Filter "mg3kill|$BADRE|exit=|access violation|#0[0-9] " -Script "$S" > "$LOG" 2>&1
DEAD=$(grep "mg3kill: killed " "$LOG" | grep -vc "health [1-9]")
BAD=0
grep -Eq "$BADRE" "$LOG" && BAD=1
grep -q "mg3kill: done" "$LOG" || BAD=1
[ "$DEAD" -eq "$N" ] || BAD=1
if [ $BAD -eq 0 ]; then echo "mg3kill: PASS $MAP: $DEAD/$N monsters killed, no crash"
else echo "mg3kill: FAIL $MAP: $DEAD/$N killed"; grep -E "mg3kill: kill |$BADRE|#0[0-9] " "$LOG" | head -12; fi
exit $BAD
