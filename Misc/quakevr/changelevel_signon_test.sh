#!/bin/bash
# changelevel_signon_test.sh <agent> -- a changelevel in the frames of the client's signon (ROUND24.md, "The changelevel
# error"): `map e1m1; wait3; changelevel e1m2` ended in "Host_Error: Illegible server message 39". The new level took
# the client's "begin" for the old one (queued when the changelevel ran), sent it datagrams before its serverinfo, and
# the client's cleared state could not read svc_quakevr (39). Now a signon reply read while the level's serverinfo is
# still unsent is ignored (host_cmd.c SV_SignonReplyStale).
# One game: `map e1m1; wait<N>; changelevel e1m2` for N = 0..6 and 10 (each frame of the first map's signon, and after
# it), each waiting (vr_waitprint) for the client to sign on to e1m2 ("Client signed on: e1m2", developer 1).
# PASS: every changelevel signed on to e1m2, no Host_Error, the run quit by its own script (exit 0).
AGENT=${1:-flatwall}; shift
source "$(dirname "$0")/qvrtest.sh"
WAITS="0 1 2 3 4 5 6 10"
S="developer 1"
n=0
for w in $WAITS; do
    W=""; [ "$w" != "0" ] && W="wait$w;"
    S="$S;map e1m1;${W}changelevel e1m2;vr_waitprint 3000 \"Client signed on: e1m2\";echo changelevel_signon: case wait$w over"
    n=$((n + 1))
done
S="$S;toggleconsole;quit"
log=$(bash "$KIT/run.sh" "$AGENT" -Timeout 300 -Script "$S" \
    -Filter "Client signed on|Host_Error|Illegible|case wait|ignored: sent before|timed out|exit=" "$@")
grep -aE "Host_Error|Illegible|ignored: sent before|timed out" <<<"$log" | head -10
for w in $WAITS; do
    # the case: an e1m2 sign-on and no Host_Error between the case before's "over" line and its own
    ok=$(awk -v w="wait$w" '/Client signed on: e1m2/ { s = 1 } /Host_Error/ { e = 1 } $0 ~ ("case " w " over") { print (s && !e) ? 1 : 0; f = 1; exit } /case wait[0-9]+ over/ { s = 0; e = 0 } END { if (!f) print 0 }' <<<"$log")
    check "$ok" "map e1m1; wait$w; changelevel e1m2: signed on to e1m2, no Host_Error"
done
check $(grep -qa "Host_Error" <<<"$log" && echo 0 || echo 1) "no Host_Error"
check $(grep -qa "exit=0" <<<"$log" && echo 1 || echo 0) "the game quit by its script (exit 0)"
verdict; exit $fail
