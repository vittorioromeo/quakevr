#!/bin/bash
# doubletap_test.sh <agent> -- run by double-tapping the moving stick (vr_move_doubletap; vr_input.cpp doubleTapStep;
# ROUND25.md "comfort"). 1. `vr_doubletap_test`: its steps fed pushes frame by frame (two quick pushes the same way run,
# let go ends it, a thumb's dip doesn't; a slow first push, a slow gap, another way, never let go, too weak: no run).
# 2. The live game with the mock left stick (Default Speed: Walk, the speed button up), once in game (`vr_doubletap_test
# ready` holds the script until then): a long push walks; after a gap longer than the window, push-let go-push quickly
# runs (faster asked); the stick let go ends it; with the setting off the same taps walk. The other waits are counted
# frames the steps need (72 a second; the window 0.3 s = 21.6 frames). Ends "doubletap: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
ST="vr_mock_stick off 0 1"; LET="vr_mock_stick off 0 0"
TAPS="$ST;wait6;$LET;wait6;$ST;wait12"
S="vr_doubletap_test;map e1m1;vr_doubletap_test ready;god;notarget;cl_alwaysrun 0;vr_sprint_toggle 0;vr_move_doubletap 1;"
S="${S}$ST;wait40;vr_doubletap_test state walk;$LET;wait40;"
S="${S}$TAPS;vr_doubletap_test state taps;$LET;wait2;vr_doubletap_test state letgo;wait40;"
S="${S}vr_move_doubletap 0;$TAPS;vr_doubletap_test state off;$LET;"
OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Timeout 240 -Filter "^vr_doubletap_test|^doubletap state|rror|CRASH" 2>&1)
grep -E "FAIL|CRASH|rror|state ready" <<<"$OUT"
expect_verdict "$OUT" "vr_doubletap_test" "the steps (vr_doubletap_test)"
st() { grep "^doubletap state $1:" <<<"$OUT" | head -1; }
field() { st "$1" | awk -v n="$2" '{print $n}'; }
W=$(field walk 9)
check $([ "$(field walk 5)" = 0 ] && echo 1 || echo 0) "a long push walks: $(st walk)"
check $(awk -v w="$W" -v t="$(field taps 9)" -v r="$(field taps 5)" 'BEGIN{print (r==1 && t>w*1.2) ? 1 : 0}') "push, let go, push quickly runs: $(st taps)"
check $([ "$(field letgo 5)" = 0 ] && echo 1 || echo 0) "let go ends it: $(st letgo)"
check $(awk -v w="$W" -v o="$(field off 9)" -v r="$(field off 5)" 'BEGIN{print (r==0 && o<w*1.05) ? 1 : 0}') "the setting off, the same taps walk: $(st off)"
verdict; exit $fail
