#!/bin/bash
# blend_autoexport_test.sh <agent> [--idle] -- Blender's export on save and the game's model watch (MODELS_IN_BLENDER.md,
# "A weapon's source .blend"; ROUND24.md, "Blender: export on save, the game reads it again"), headless, on a copy of
# the shotgun's source .blend in <tree>/scratch/ba_autoexport (inside the repository: its quakevr/progs is the game's):
#   1. a renamed frame saved: the add-on's export refuses, writes nothing (blend_autoexport_test.py refuse);
#   2. the game on e1m1 (vr_model_watch 1, the default) and the copy saved as it is: the add-on exports (the shipped
#      files byte for byte, new times) and the game reads v_shot.mdl again by itself (its watch's line);
#   3. (--idle) a minute of real time on e1m1 with nothing saved: no notification, nothing looked at, no model read
#      again (vr_model_watch_status). Real time (-RealTime) because the watch asks the disk every half second of real
#      time: a fast run's minute of game time is a few seconds of it.
# Prints PASS/FAIL per check and "blend_autoexport: N passed, M failed"; exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
TREE=C:/OHWorkspace/qvr-agents/$AGENT
BLENDER=${BLENDER:-"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"}
LOG=${QVR_LOG:-$KIT/bases/$AGENT/qbase/qconsole.log}
T=$TREE/scratch/ba_autoexport
SRC=$TREE/Misc/quakevr/src_models/blend/v_shot
fresh_copy() { mkdir -p "$T"; cp "$SRC/v_shot.blend" "$SRC/v_shot_skin0.png" "$SRC/v_shot_relief.png" "$T/"; }
has() { tr -d '\r\n' < "$2" | grep -qE "$1" && echo 1 || echo 0; } # (run.sh's -Filter wraps long lines)

# 1. A failed check writes nothing.
fresh_copy
"$BLENDER" -b --factory-startup "$T/v_shot.blend" --python "$TREE/Misc/quakevr/blend_autoexport_test.py" -- refuse \
    > "$T/refuse.out" 2>&1
grep "blend_autoexport_save:" "$T/refuse.out"
expect_verdict "$(cat "$T/refuse.out")" blend_autoexport_save "a refused export wrote nothing"

# 2. The game watching, the copy saved: exported, read again by the game.
fresh_copy
TOKEN=ba_ready_$$
bash "$KIT/run.sh" "$AGENT" -Script "map e1m1;wait10;vr_model_watch_status;echo $TOKEN" \
    -QuitOn "vr_model_watch: changed on disk, read again:.*v_shot\.mdl" -Timeout 240 \
    -Filter "vr_model_watch|vr_model_reload|exit=" > "$T/game.out" 2>&1 &
GAME=$!
ready=0
for i in $(seq 1 900); do
    if grep -q "$TOKEN" "$LOG" 2>/dev/null; then ready=1; break; fi
    sleep 0.2
done
check $ready "the game loaded e1m1 and its watch listed the models"
"$BLENDER" -b --factory-startup "$T/v_shot.blend" --python "$TREE/Misc/quakevr/blend_autoexport_test.py" -- ok \
    > "$T/save.out" 2>&1
grep "blend_autoexport_save:" "$T/save.out"
expect_verdict "$(cat "$T/save.out")" blend_autoexport_save "the save exported the shotgun"
wait $GAME
cat "$T/game.out"
check $(has "vr_model_watch: changed on disk, read again:.*v_shot\.mdl" "$T/game.out") "the game read v_shot.mdl again by itself"
check $(has "vr_model_reload: progs/v_shot.mdl: " "$T/game.out") "as vr_model_reload reads it"
check $(git -C "$TREE" diff --quiet -- quakevr/progs quakevr/data/weapons.txt && echo 1 || echo 0) \
    "the export wrote the shipped files byte for byte"

# 3. Idle: a minute, nothing changes.
if [ "$2" = "--idle" ]; then
    bash "$KIT/run.sh" "$AGENT" -RealTime -Script "map e1m1;wait4320;vr_model_watch_status;toggleconsole;quit" \
        -Timeout 300 -Filter "vr_model_watch|exit=" > "$T/idle.out" 2>&1
    cat "$T/idle.out"
    check $(has "by the system.* 0 notifications, [0-9]+ cycles, 0 looks, 0 changes, 0 reloads" "$T/idle.out") \
        "idle: no notification, no file looked at, nothing read again"
    check $(grep -q "changed on disk" "$T/idle.out" && echo 0 || echo 1) "idle: no reload line"
fi
verdict
exit $fail
