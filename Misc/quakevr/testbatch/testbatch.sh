#!/bin/bash
# testbatch.sh -- sourced by a bash test suite: its game runs played as batches (many cases in one game, the cases split
# between parallel copies; testbatch.py; docs/vr-port/TESTING.md, "Batched test cases"). The suite is written as before,
# its body in a function, its runs through tb_run_sh:
#   source "$TREE/Misc/quakevr/testbatch/testbatch.sh"
#   main() {
#       log=$(tb_run_sh -Script "map e1m1;wait60;...;toggleconsole;quit" -Filter "^foo:" 2>&1)   # was: bash $KIT/run.sh $AGENT ...
#       ...the checks on $log, as before...
#   }
#   tb_main "$AGENT" main "$@"
# tb_main runs the function twice: first to collect its runs (tb_run_sh prints nothing; that pass's output is thrown
# away), then the batches (run_batch.sh), then again for real: each tb_run_sh prints what run.sh would have printed for
# that run alone. A run whose arguments differ the second time (a script built from an earlier run's output) or that
# can't be batched (-Out, -Clean, -RealTime, -Visible, -Full, a screenshot) runs alone then, as before.
#   TB_SLOW=1   every run by run.sh, one after another, as before the batches (a suite's reference results)
#   TB_FIXED=0  real-time frames in the batches (default: vr_fixed_frames 1, unpaced: the same game time a frame)
#   TB_JOBS=N   copies of the game at most (default 8; the kit's free slots decide)
# The batch's cases, outputs and stats.json: <tree>/scratch/testbatch/<suite>/.
TB_HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TB_KIT=${KIT:-C:/OHWorkspace/qvr-kit}

tb_main()
{
    TB_AGENT=$1
    local fn=$2
    shift 2
    if [ "$TB_SLOW" = "1" ]; then
        TB_PASS=slow
        "$fn" "$@"
        return $?
    fi
    local suite
    suite=$(basename "$(dirname "$0")")_$(basename "$0" | sed 's/\.[a-z]*$//')
    TB_DIR="$TB_HERE/../../../scratch/testbatch/$suite"
    mkdir -p "$TB_DIR/solo"
    local f
    for f in "$TB_DIR"/case_*.args "$TB_DIR"/out_*.txt; do [ -e "$f" ] && : > "$f"; done
    echo 0 > "$TB_DIR/count"
    : > "$TB_DIR/index"
    : > "$TB_DIR/used"
    local t0=$SECONDS
    (TB_PASS=collect; "$fn" "$@") > /dev/null 2>&1
    local t1=$SECONDS
    bash "$TB_HERE/run_batch.sh" "$TB_AGENT" "$TB_DIR"
    [ -n "$TB_STATS" ] && { cat "$TB_DIR/stats.json"; echo " collect $((t1 - t0)) s, batch $((SECONDS - t1)) s"; } >&2
    echo 0 > "$TB_DIR/count"
    TB_PASS=replay
    "$fn" "$@"
}

# tb_run_sh <run.sh arguments after the agent>: in place of "bash $KIT/run.sh $AGENT <arguments>".
tb_run_sh()
{
    local n
    case "$TB_PASS" in
    collect)
        n=$(( $(cat "$TB_DIR/count") + 1 )); echo $n > "$TB_DIR/count"
        printf '%s\0' "$@" > "$TB_DIR/case_$n.args"
        echo "$(printf '%s\0' "$@" | md5sum | cut -c1-32) $n" >> "$TB_DIR/index"
        return 0 ;;
    replay)
        # The first collected run with the same arguments not handed out yet (by arguments, not by position: a run
        # retried a different number of times than in the first pass leaves the others where they were).
        local h k
        h=$(printf '%s\0' "$@" | md5sum | cut -c1-32)
        for k in $(grep "^$h " "$TB_DIR/index" | cut -d' ' -f2); do
            grep -qx "$k" "$TB_DIR/used" && continue
            echo "$k" >> "$TB_DIR/used"
            if [ -s "$TB_DIR/out_$k.txt" ]; then cat "$TB_DIR/out_$k.txt"; return 0; fi
            break
        done
        printf '%s\0' "$@" > "$TB_DIR/solo/case_1.args"
        if python "$TB_HERE/testbatch.py" batchable "$TB_DIR/solo" 1; then
            : > "$TB_DIR/solo/out_1.txt"
            bash "$TB_HERE/run_batch.sh" "$TB_AGENT" "$TB_DIR/solo"
            if [ -s "$TB_DIR/solo/out_1.txt" ]; then cat "$TB_DIR/solo/out_1.txt"; return 0; fi
        fi
        bash "$TB_KIT/run.sh" "$TB_AGENT" "$@" ;;
    *)
        bash "$TB_KIT/run.sh" "$TB_AGENT" "$@" ;;
    esac
}
