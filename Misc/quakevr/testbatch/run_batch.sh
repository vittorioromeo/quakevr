#!/bin/bash
# run_batch.sh <agent> <dir> -- plays the batched cases in <dir> (case_<n>.args: run.sh's arguments, NUL-separated)
# in copies of the agent's game, as testbatch.py exec does, while holding the agent's run lock (its runs go one at a
# time, as run.sh's and eval.sh's) and up to TB_JOBS (8) of the kit's run slots (slots.sh acquire_many: as many as are
# free, a reserve left for other agents). Called by testbatch.sh and testbatch.py's Batch; not by suites directly.
# The whole script is one function, read in full before it runs (a script edited while it runs is read from the old
# offset otherwise).
main() {
AGENT="$1"; DIR="$2"
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
local n
n=$(find "$DIR" -maxdepth 1 -name 'case_*.args' -size +0 | wc -l)
[ "$n" -gt 0 ] || return 0
AL="$KIT/agentlocks/$AGENT${QVR_LANE:+.lane$QVR_LANE}" # (a lane: its own lock, its copies qbase.lane<k>.<n>)
mkdir -p "$KIT/agentlocks"
local i
for i in $(seq 1 9e3); do
    mkdir "$AL" 2>/dev/null && break
    [ -d "$AL" ] && [ $(( $(date +%s) - $(stat -c %Y "$AL") )) -gt 1800 ] && rmdir "$AL" 2>/dev/null
    sleep .2
done
source "$KIT/slots.sh" "$AGENT" "testbatch $(basename "$DIR") ($n cases)"
local want=${TB_JOBS:-8}; [ "$want" -gt "$n" ] && want=$n; [ "$want" -lt 1 ] && want=1
acquire_many "$want"
trap 'release; rmdir "$AL" 2>/dev/null' EXIT
python "$HERE/testbatch.py" exec "$DIR" "$AGENT" "$NHELD"
}
main "$@"
