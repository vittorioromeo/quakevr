#!/bin/bash
# deadleap_test.sh <agent> [20 21 22] -- a leaping monster dies and stays dead (QC vr_deadleap_test.qc): 20 a fiend, 21 a
# dog, 22 a spawn, killed by shots, blows and a neck snap, on the ground and in mid-leap; knocked down; staggered; dying of
# a blast its own leap set off (scene 9, the note vrfiringrange_2026-11-03_02-01-47); and its corpse poked. Each run ends on
# its verdict line, `deadleap: N passed, M failed`.
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
for n in ${*:-20 21 22}; do
    echo "== vr_deadanim_test $n"
    bash "$KIT/run.sh" "$AGENT" -Script "skill 1;map vrfiringrange;wait60;god;notarget;developer 1;vr_deadanim_test $n" \
        -QuitOn "deadleap: [0-9]+ passed" -Filter "^deadleap: (FAIL|[0-9])|rror|CRASH|NULL" -Timeout 200 2>&1 | grep -v "^$\|^exit="
done
