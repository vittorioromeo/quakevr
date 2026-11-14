#!/bin/bash
# autosavenow_test.sh <agent> -- vr_autosave_now (Debug > Tests > Saves; ROUND25.md, "flat1115b"): the game's autosave made on
# demand. Fast mode; two runs, each quits by its own script (sv_autosave 0 in the test runs: the game makes none itself, so
# the save Continue finds is this one's).
#   a. single player, e1m1: the command names autosave/e1m1, the save is written, and Continue offers it, newest;
#   b. co-op (maxplayers 2, coop 1): refused, nothing saved.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
QT_NAME=autosavenow
export QVR_AUTHOR_WAIT_ALWAYS=1
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
log=$HERE/scratch/autosavenow_a.log
bash $KIT/run.sh $AGENT -Script "map e1m1; wait10; vr_autosave_now; wait10; vr_continue_info; toggleconsole; quit" -Filter "^vr_autosave_now|^Continue:|^Saving|^Autosave|^Can't" -Timeout 60 > "$log" 2>&1
grep -a "^vr_autosave_now\|^Continue:\|^Saving" "$log" | cut -c1-160 | sed 's/^/  /'
check $(grep -aq "^vr_autosave_now: autosave/e1m1" "$log" && echo 1 || echo 0) "a. single player: the command makes autosave/e1m1 (the game's name)"
check $(grep -aq "^Saving game to .*autosave/e1m1" "$log" && echo 1 || echo 0) "a. the save is written"
check $(grep -a "^Continue:" "$log" | grep -q "autosave/e1m1" && grep -a "^Continue:" "$log" | grep -qi "0 min\|just now\|seconds\|1 min" && echo 1 || echo 0) "a. Continue offers it, just made"
log=$HERE/scratch/autosavenow_b.log
bash $KIT/run.sh $AGENT -Script "maxplayers 2; coop 1; map e1m1; wait10; vr_autosave_now; wait5; disconnect; coop 0; maxplayers 1; toggleconsole; quit" -Filter "^vr_autosave_now|^Saving" -Timeout 60 > "$log" 2>&1
grep -a "^vr_autosave_now\|^Saving" "$log" | cut -c1-160 | sed 's/^/  /'
check $(grep -aq "^vr_autosave_now: none in a multiplayer game" "$log" && ! grep -aq "^Saving" "$log" && echo 1 || echo 0) "b. co-op: refused, nothing saved"
verdict; exit $fail
