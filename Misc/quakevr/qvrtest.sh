#!/bin/bash
# qvrtest.sh -- the shell tests' library (qvrtest.py's verdict grammar; docs/vr-port/TESTING.md, "Writing a test").
# Sourced by a test script under Misc/quakevr after it reads its agent:
#   AGENT=$1
#   source "$(dirname "$0")/qvrtest.sh"            # (from a subfolder: "$(dirname "$0")/../qvrtest.sh")
#   check $(grep -q "^foo: ok" <<<"$log" && echo 1 || echo 0) "foo is ok"   # `<name>: PASS foo is ok` (or FAIL)
#   verdict; exit $fail                             # `<name>: N passed, M failed`
# <name> is QT_NAME (default: the script's name without .sh and _test). KIT and TREE are set if they are not.
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=${TREE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -W)}
if [ -z "$QT_NAME" ]; then QT_NAME=$(basename "$0" .sh); QT_NAME=${QT_NAME%_test}; QT_NAME=${QT_NAME%_tests}; fi
fail=0
qt_passed=0
qt_failed=0
# check <1|0> <what>: one check's line; a FAIL sets fail=1 (the script's exit status).
check() { if [ "$1" = "1" ]; then echo "$QT_NAME: PASS $2"; qt_passed=$((qt_passed + 1)); else echo "$QT_NAME: FAIL $2"; qt_failed=$((qt_failed + 1)); fail=1; fi; }
# verdict: the test's last line.
verdict() { echo "$QT_NAME: $qt_passed passed, $qt_failed failed"; }
# expect_verdict <output> <tag> <what>: one check that the output's `<tag>: N passed, M failed` line has no failure.
expect_verdict() { local v; v=$(grep -aE "$2: [0-9]+ passed, [0-9]+ failed" <<<"$1" | tail -1); check $([ -n "$v" ] && grep -q ", 0 failed" <<<"$v" && echo 1 || echo 0) "$3 (${v:-no verdict line})"; }
