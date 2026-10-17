#!/bin/bash
# Dawn of the Machine (MG3): The Sacrifice's (map8) Rune of Sacrifice comes only after the eight sacrifices, headless
# (a player's report: "the Rune of Sacrifice spawns without fulfilling any preconditions"; ROUND22.md, "The Sacrifice").
#   bash mg3_rune_test.sh <agent>
# The player stands in the altar room's trigger (trigger_once "rune_trigger", upstream's SPAWNFLAG_TRIGGER_FIRST: a
# trigger only once the sacrifice counter has used it) for about twelve seconds: the rune must stay hidden. Then the
# eight victims are gibbed (vr_mg3_test 24), and standing there brings the lightning, the altar's sacrifice and the rune.
# Needs the owned rerelease MG3 data. Logs scratch/mg3rune.log. Exit status 0: hidden before, there after, no error.
NAME=${1:?agent}
KIT="C:/OHWorkspace/qvr-kit"; T="C:/OHWorkspace/qvr-agents/$NAME"; mkdir -p "$T/scratch"
LOG="$T/scratch/mg3rune.log"
P="setpos 256 300 120"
S="developer 1;vr_campaign_native mg3;skill 1;map map8;wait120;god;notarget;vr_mg3_test 23;wait5;echo mg3rune: stage before"
S="$S;$P;wait900;vr_mg3_test 23;wait5;echo mg3rune: stage sacrifices;vr_mg3_test 24;wait30;$P;wait900;vr_mg3_test 23"
S="$S;wait5;echo mg3rune: done;toggleconsole;quit"
BADRE="clipping list|ENGINE|TIMEOUT|Host_Error|SV_Error|runaway|PR_ExecuteProgram"
bash "$KIT/run.sh" "$NAME" -ExtraArgs "-nomapindex -noaddons" -Timeout ${TMO:-300} \
    -Filter "mg3rune|$BADRE|exit=|access violation|#0[0-9] " -Script "$S" > "$LOG" 2>&1
BEFORE=$(sed -n '/stage before/,/stage sacrifices/p' "$LOG" | grep -c "sigil rune there")
AFTER=$(sed -n '/stage sacrifices/,/mg3rune: done/p' "$LOG" | grep -c "sigil rune there")
MADE=$(grep -o "mg3rune: [0-9]* sacrifices made" "$LOG" | head -1)
BAD=0
grep -Eq "$BADRE" "$LOG" && BAD=1
grep -q "mg3rune: done" "$LOG" || BAD=1
[ "$BEFORE" -eq 0 ] || BAD=1
[ "$AFTER" -ge 1 ] || BAD=1
if [ $BAD -eq 0 ]; then echo "mg3rune: PASS map8: rune hidden in its room before the sacrifices, there after ($MADE)"
else echo "mg3rune: FAIL map8: rune there before $BEFORE, after $AFTER ($MADE)"; grep -E "mg3rune|$BADRE|#0[0-9] " "$LOG" | head -14; fi
exit $BAD
