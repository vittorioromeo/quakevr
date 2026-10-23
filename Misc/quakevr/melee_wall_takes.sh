#!/bin/bash
# melee_wall_takes.sh <agent> [<sets>] [<out name>] -- the author's melee takes swung into a wall (ROUND22.md "One wall blow
# a swing"): every melee take (motions/, as eval.sh's TAKES) with its weapon, the axe takes also with the crowbar and the
# shotgun, replayed in vrmelee-recording with its dummy at the north wall at five depths (melee_wall_takes.py); per
# weapon how many replays struck the wall and how many struck it twice or more in the one swing (a doubled thunk).
#   <sets>: axe,crowbar,shotgun,sword,fists (the default: all). The run's log: <worktree>/scratch/<out name>.log
#   (default wall_takes). Compare two: python Misc/quakevr/melee_wall_takes.py report <a.log> <b.log>.
AGENT=$1; SETS=${2:-axe,crowbar,shotgun,sword,fists}; OUT=${3:-wall_takes}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TAKES=${EVAL_TAKES:-C:/OHWorkspace/quakevr-iw/quakevr/motions}
WT=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$WT/scratch"
python "$WT/Misc/quakevr/melee_wall_takes.py" gen "$TAKES" "$WT/scratch/$OUT.list" --sets "$SETS" || exit 1
bash "$KIT/run.sh" "$AGENT" -Timeout 3000 -Filter "melee event: |\.csv \| at |vr_motion_eval: [0-9]+ takes:|ENGINE ERROR|TIMEOUT" \
    -Script "vr_mock_fast 2;r_norefresh 1;vr_mock_eye_size 64;developer 1;wait;vr_motion_eval list $WT/scratch/$OUT.list out $WT/scratch/$OUT.csv quit" \
    > "$WT/scratch/$OUT.run.txt" 2>&1
grep -E "exit=|vr_motion_eval: [0-9]+ takes:|ENGINE|TIMEOUT" "$WT/scratch/$OUT.run.txt" | cut -c1-200
# (the console's own log: run.sh's lines are wrapped at the console's width)
cp "$KIT/bases/$AGENT/qbase/qconsole.log" "$WT/scratch/$OUT.log"
python "$WT/Misc/quakevr/melee_wall_takes.py" report "$WT/scratch/$OUT.log"
