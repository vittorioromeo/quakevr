#!/bin/bash
# selftest.sh <agent> -- the batch harness's own cases (TESTING.md "Batched test cases"): -QuitOn in a batch (the engine's
# vr_test_quiton/vr_waitquiton: the pattern's regex subset, armed from the case's start, the closing wait ended at the
# match, no closing quit), -NoDraw per case. Five cases, one batch; prints "testbatch_selftest: N passed, M failed",
# exits 1 on a failure.
AGENT=${1:-mp}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/testbatch.sh"
pass=0; fail=0
ck() { if [ "$1" = 1 ]; then pass=$((pass+1)); echo "testbatch_selftest: PASS $2"; else fail=$((fail+1)); echo "testbatch_selftest: FAIL $2"; fi; }
body() {
    # 1: ^, a class and +, matched during the closing wait6000 (~80 s of frames drawn): the case ends there, undrawn
    a=$(tb_run_sh -NoDraw -QuitOn "^tbq: [0-9]+ passed" -Filter "^tbq" -Script "map start;wait10;echo tbq: start;wait20;echo tbq: 12 passed;echo tbq: same frame;wait6000;toggleconsole;quit")
    # 2: an alternation group, case-insensitive, printed before the closing wait (armed from the case's start)
    b=$(tb_run_sh -QuitOn "tbq: (DONE|fail)" -Filter "^tbq" -Script "map start;wait5;echo tbq: done;wait30;echo tbq: mid;wait6000;toggleconsole;quit")
    # 3: no match: the closing wait runs out silently, then the rest of the script
    c=$(tb_run_sh -QuitOn "^tbq: never" -Filter "^tbq|timed out" -Script "map start;wait10;echo tbq: x;wait50;echo tbq: end;toggleconsole;quit")
    # 4: no closing quit (the agrunt cases' shape): the case runs until the match
    d=$(tb_run_sh -QuitOn "tbq: test 5 over" -Filter "^tbq" -Script "map start;wait20;echo tbq: test 5 over")
    # 5: an escaped dot, a negated class, \s and $ (echo's lines end in a space, in the log as in the game)
    e=$(tb_run_sh -QuitOn "^tbq: v1\.5 [^x]ok\s*$" -Filter "^tbq" -Script "map start;wait5;echo tbq: v1x5 yok;echo tbq: v1.5 xok;wait5;echo tbq: v1.5 yok;wait6000;toggleconsole;quit")
    [ "$TB_PASS" = collect ] && return
    ck $([[ $a == *"12 passed"* && $a == *"same frame"* && $a == *quit-on* ]] && echo 1 || echo 0) "regex ends the closing wait"
    ck $([[ $b == *"tbq: done"* && $b == *quit-on* ]] && echo 1 || echo 0) "alternation, case-insensitive, armed from the start"
    ck $([[ $c == *"tbq: end"* && $c != *"timed out"* && $c == *exit=0* ]] && echo 1 || echo 0) "no match: the wait runs out silently"
    ck $([[ $d == *"test 5 over"* && $d == *quit-on* ]] && echo 1 || echo 0) "no closing quit: runs until the match"
    ck $([[ $e == *"v1.5 yok"* && $e != *TIMEOUT* && $e == *quit-on* ]] && echo 1 || echo 0) "escapes, negated class, \\s, \$"
    echo "testbatch_selftest: $pass passed, $fail failed"
}
tb_main "$AGENT" body
exit $((fail > 0))
