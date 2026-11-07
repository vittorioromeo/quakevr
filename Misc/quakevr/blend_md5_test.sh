#!/bin/bash
# blend_md5_test.sh <agent> -- the jointed hand's and the body's source .blends (MODELS_IN_BLENDER.md, sections 3 and
# 5: Misc/quakevr/src_models/blend/hand_rig/hand_rig.blend, .../vrbody/vrbody.blend), headless:
#   1. each exported unedited (blend_export.py, as the export on save runs it) into a copy of the game's files: every
#      file byte for byte as shipped (the hand's md5 files, its 4 skins, the feminine hand left alone; the body's four
#      builds from its shape keys and its 16 skins);
#   2. the feminine hand made from the shipped hand (make_hand_feminine.derive, what the hand's export runs when the
#      hand changed) is the shipped one byte for byte;
#   3. the game on e1m1 watching its models, and a small edit saved in a copy of each .blend (scratch/bmd5, inside the
#      repository: its export writes the game's quakevr/progs): a fingertip (the feminine hand made again), the top of
#      the body's head on the basis (every build moved by as much); the game reads hand_rig and vrbody again by itself
#      (vr_model_watch). The game's files and generated.json are put back afterwards.
# Prints PASS/FAIL per check and "blend_md5: N passed, M failed"; exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
TREE=C:/OHWorkspace/qvr-agents/$AGENT
BLENDER=${BLENDER:-"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"}
T=$TREE/scratch/bmd5
PROGS=$TREE/quakevr/progs
mkdir -p "$T/backup"
has() { tr -d '\r\n' < "$2" | grep -qE "$1" && echo 1 || echo 0; } # (run.sh's -Filter wraps long lines)

# 1. Unedited: byte for byte.
for m in hand_rig vrbody; do
    mkdir -p "$T/$m/progs"
    cp "$PROGS"/$m* "$T/$m/progs/"
    "$BLENDER" -b --factory-startup "$TREE/Misc/quakevr/src_models/blend/$m/$m.blend" --python-exit-code 1 \
        --python "$TREE/Misc/quakevr/blend_export.py" -- --out "$T/$m/progs" > "$T/$m/out.txt" 2>&1
    differ=$(python "$TREE/Misc/quakevr/qvr_cmpdir.py" "$T/$m/progs" "$PROGS")
    check $(grep -q "^blend_export: ok" "$T/$m/out.txt" && [ -z "$differ" ] && echo 1 || echo 0) \
        "$m.blend exported unedited: $(grep "^blend_export: ok" "$T/$m/out.txt"), every file as shipped${differ:+ (differ: $differ)}"
done

# 2. The feminine hand.
"$BLENDER" -b --factory-startup --python "$TREE/Misc/quakevr/blend_md5_test.py" -- feminine "$T/feminine" \
    > "$T/feminine.txt" 2>&1
grep "blend_md5_feminine: \(FAIL\|[0-9]\)" "$T/feminine.txt"
expect_verdict "$(cat "$T/feminine.txt")" blend_md5_feminine "the feminine hand derived as shipped"

# 3. Edits saved while the game watches.
cp "$PROGS"/hand_rig* "$PROGS"/vrbody* "$TREE/Misc/quakevr/generated.json" "$T/backup/"
cp "$TREE/Misc/quakevr/src_models/blend/hand_rig/hand_rig.blend" "$TREE/Misc/quakevr/src_models/blend/vrbody/vrbody.blend" "$T/"
TOKEN=bmd5_ready_$$
LOG=${QVR_LOG:-$KIT/bases/$AGENT/qbase/qconsole.log}
bash "$KIT/run.sh" "$AGENT" -Script "map e1m1;wait10;vr_model_watch_status;echo $TOKEN" \
    -QuitOn "vr_model_watch: changed on disk, read again:.*vrbody" -Timeout 400 \
    -Filter "vr_model_watch|vr_model_reload|hand|exit=" > "$T/game.out" 2>&1 &
GAME=$!
ready=0
for i in $(seq 1 900); do
    if grep -q "$TOKEN" "$LOG" 2>/dev/null; then ready=1; break; fi
    sleep 0.2
done
check $ready "the game loaded e1m1 and its watch listed the models"
for m in hand body; do
    b=$([ $m = hand ] && echo hand_rig || echo vrbody)
    "$BLENDER" -b --factory-startup "$T/$b.blend" --python "$TREE/Misc/quakevr/blend_md5_test.py" -- edit_$m \
        > "$T/edit_$m.txt" 2>&1
    grep "blend_md5_edit_$m: \(FAIL\|[0-9]\)" "$T/edit_$m.txt"
    expect_verdict "$(cat "$T/edit_$m.txt")" blend_md5_edit_$m "the $m's edit saved and exported"
done
wait $GAME
grep "read again" "$T/game.out"
check $(has "vr_model_watch: changed on disk, read again:.*hand_rig" "$T/game.out") "the game read the edited hand again by itself"
check $(has "vr_model_watch: changed on disk, read again:.*vrbody" "$T/game.out") "the game read the edited body again by itself"
cp "$T/backup/"hand_rig* "$T/backup/"vrbody* "$PROGS/"
cp "$T/backup/generated.json" "$TREE/Misc/quakevr/generated.json"
check $(git -C "$TREE" diff --quiet -- quakevr/progs && echo 1 || echo 0) "the game's files put back"
verdict
exit $fail
