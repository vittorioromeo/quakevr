#!/bin/bash
# Dawn of the Machine (MG3): press every wall button of a map, headless, and fail on a crash (a player's report:
# "Repeatedly crashed after pressing the button in The Matriarch of Decay", map6; ROUND22.md, "The Matriarch of Decay").
#   bash mg3_buttons_test.sh <agent> [map] [buttons]     (default map6 and 32 buttons: map6 has 32)
# Each button: vr_mg3_test 200+k (the player presses it as a hand would, or shoots a shot button), then about two
# seconds for its targets (delays, doors, spawned monsters and items) to run. Needs the owned rerelease MG3 data.
# Logs scratch/mg3buttons_<map>.log. Exit status 0: every button pressed, no crash, no error, no trigger linked as a
# solid (the engine's "Trigger in clipping list", a developer warning since the fix, a fatal error before), the map
# still running at the end.
NAME=${1:?agent}; MAP=${2:-map6}; N=${3:-32}
KIT="C:/OHWorkspace/qvr-kit"; T="C:/OHWorkspace/qvr-agents/$NAME"; mkdir -p "$T/scratch"
LOG="$T/scratch/mg3buttons_$MAP.log"
S="developer 1;vr_campaign_native mg3;skill 1;map $MAP;wait120;god;notarget"
k=0
while [ $k -lt $N ]; do S="$S;vr_mg3_test $((200 + k));wait150"; k=$((k + 1)); done
S="$S;echo mg3btn: done;toggleconsole;quit"
BADRE="clipping list|ENGINE|TIMEOUT|Host_Error|SV_Error|runaway|PR_ExecuteProgram"
bash "$KIT/run.sh" "$NAME" -ExtraArgs "-nomapindex -noaddons" -Timeout ${TMO:-600} \
    -Filter "mg3btn|$BADRE|exit=|access violation|#0[0-9] " -Script "$S" > "$LOG" 2>&1
PRESSED=$(grep -c "mg3btn: press " "$LOG")
BAD=0
grep -Eq "$BADRE" "$LOG" && BAD=1
grep -q "mg3btn: done" "$LOG" || BAD=1
[ "$PRESSED" -eq "$N" ] || BAD=1
if [ $BAD -eq 0 ]; then echo "mg3buttons: PASS $MAP: $PRESSED buttons pressed, no crash"
else echo "mg3buttons: FAIL $MAP: $PRESSED/$N pressed; last: $(grep "mg3btn: press" "$LOG" | tail -1)"; grep -E "$BADRE|#0[0-9] " "$LOG" | head -12; fi
exit $BAD
