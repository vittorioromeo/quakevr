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
#   TB_COLLECT_STUBS=1  (a suite sets it) the collect pass's log-reading tools are no-ops: its outputs are empty and
#               thrown away, and each grep/sed/python a case cost seconds on a loaded machine. Safe: a run whose
#               arguments came out different without them (built from a file or an earlier run's output with one of
#               these) is still played, alone, in the replay (TB_STATS lists those); awk, seq, printf still work.
TB_HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TB_KIT=${KIT:-C:/OHWorkspace/qvr-kit}

tb_collect_stubs()
{
    grep() { return 0; }; sed() { return 0; }; cut() { return 0; }; tr() { return 0; }; head() { return 0; }
    tail() { return 0; }; wc() { return 0; }; paste() { return 0; }; sort() { return 0; }; uniq() { return 0; }
    python() { return 0; }; py() { return 0; }
    # TB_COLLECT_STUBS=2: awk and cat too (a suite whose checks are awk calls between its runs: the flat suites).
    # Equally safe: a run whose arguments an awk or cat built comes out different and is played alone in the replay.
    [ "$TB_COLLECT_STUBS" = 2 ] && { awk() { return 0; }; cat() { return 0; }; }
}

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
    (TB_PASS=collect; [ -n "$TB_COLLECT_STUBS" ] && [ "$TB_COLLECT_STUBS" != 0 ] && tb_collect_stubs; "$fn" "$@") > /dev/null 2>&1
    local t1=$SECONDS
    bash "$TB_HERE/run_batch.sh" "$TB_AGENT" "$TB_DIR"
    [ -n "$TB_STATS" ] && { cat "$TB_DIR/stats.json"; echo " collect $((t1 - t0)) s, batch $((SECONDS - t1)) s"; } >&2
    # (The index in memory for the replay: a run's arguments (joined by \x1f) -> its collected cases. tb_run_sh mostly
    # runs in a $(...) subshell, which inherits it; what it hands out goes to the used file.)
    unset TB_IDX; declare -gA TB_IDX
    local ik count args key
    read -r count < "$TB_DIR/count"
    for ((ik = 1; ik <= count; ik++)); do
        mapfile -d '' -t args < "$TB_DIR/case_$ik.args"
        printf -v key '%s\x1f' "${args[@]}"
        TB_IDX[$key]+="$ik "
    done
    echo 0 > "$TB_DIR/count"
    : > "$TB_DIR/solos"
    TB_PASS=replay
    local t2=$SECONDS rc
    "$fn" "$@"
    rc=$?
    # (TB_STATS: the replay's time and its runs played alone, not from the batch: a script built from an earlier run's
    # output, a screenshot run, or a case whose arguments changed; each is a game of its own after the batch.)
    [ -n "$TB_STATS" ] && { echo " replay $((SECONDS - t2)) s, $(wc -l < "$TB_DIR/solos") run(s) alone:"; cut -c1-150 "$TB_DIR/solos"; } >&2
    return $rc
}

# tb_run_sh <run.sh arguments after the agent>: in place of "bash $KIT/run.sh $AGENT <arguments>".
tb_run_sh()
{
    # (Bash builtins wherever they do: a process started under Git bash costs 0.1-1 s on a loaded machine, and the two
    # passes of a 100-case suite started ~8 a case: minutes, 2026-11-22. Now none.)
    local n h k u out
    case "$TB_PASS" in
    collect)
        read -r n < "$TB_DIR/count"; n=$((n + 1)); echo $n > "$TB_DIR/count"
        printf '%s\0' "$@" > "$TB_DIR/case_$n.args"
        return 0 ;;
    replay)
        # The first collected run with the same arguments not handed out yet (by arguments, not by position: a run
        # retried a different number of times than in the first pass leaves the others where they were).
        printf -v h '%s\x1f' "$@"
        for k in ${TB_IDX[$h]}; do
            while read -r u; do [ "$u" = "$k" ] && continue 2; done < "$TB_DIR/used"
            echo "$k" >> "$TB_DIR/used"
            if [ -s "$TB_DIR/out_$k.txt" ]; then IFS= read -r -d '' out < "$TB_DIR/out_$k.txt"; printf '%s' "$out"; return 0; fi
            break
        done
        echo "$*" >> "$TB_DIR/solos"
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
