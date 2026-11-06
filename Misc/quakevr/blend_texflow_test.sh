#!/bin/bash
# blend_texflow_test.sh <agent> -- the texturing workflow (MODELS_IN_BLENDER.md, "5. Texturing"; texflow.py), headless:
#   1. the game on e1m1 (vr_model_watch 1) while Blender (blend_texflow_test.py) runs every texturing tool on copies of
#      the shotgun's and the soldier's source .blends in <tree>/scratch/texflow: Paint in Blender, Palette Preview, UV
#      Wires, Fill Material, Bake Starter Shading, Ctrl+Z's pixels, the UV layout PNG, Edit Skin Externally (the PNG
#      changed outside: read back, the .blend saved, exported); the game reads v_shot.mdl and soldier.mdl again by
#      itself;
#   2. the armour's third skin (skin index 2) the same way, without the game;
#   3. vr_edit_skin finds the source and the skin (-n: what it would run).
# The exports write the repository's quakevr/progs (the game's folder): the files they touch are copied first and put
# back after (byte for byte). Before/after images of the fill and the bake: scratch/texflow/<model>/out/*_fill.png,
# *_bake.png. Prints PASS/FAIL per check and "blend_texflow: N passed, M failed"; exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
TREE=C:/OHWorkspace/qvr-agents/$AGENT
BLENDER=${BLENDER:-"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"}
LOG=${QVR_LOG:-$KIT/bases/$AGENT/qbase/qconsole.log}
T=$TREE/scratch/texflow
PROGS=$TREE/quakevr/progs
SAVED=$T/saved_progs
mkdir -p "$SAVED"
for f in "$PROGS"/*v_shot* "$PROGS"/soldier* "$PROGS"/vrgrunt* "$PROGS"/armor* "$TREE/quakevr/data/weapons.txt"; do
    cp -p "$f" "$SAVED/"
done
put_back() {
    for f in "$SAVED"/*; do
        b=$(basename "$f")
        d=$PROGS/$b
        [ "$b" = weapons.txt ] && d=$TREE/quakevr/data/weapons.txt
        cmp -s "$f" "$d" || cp -p "$f" "$d"
    done
}
blender_half() { # <model> <skin>
    mkdir -p "$T/$1"
    cp "$TREE/Misc/quakevr/src_models/blend/$1"/* "$T/$1/"
    "$BLENDER" -b --factory-startup "$T/$1/$1.blend" --python "$TREE/Misc/quakevr/blend_texflow_test.py" -- \
        "$T/$1/out" "$2" > "$T/$1.out" 2>&1
    grep -a "blend_texflow: " "$T/$1.out" | sed "s/^blend_texflow: /  $1 /"
    expect_verdict "$(cat "$T/$1.out")" blend_texflow "$1 skin $2: the texturing tools"
}
has() { tr -d '\r\n' < "$2" | grep -qE "$1" && echo 1 || echo 0; }

# 1. The game watching; the shotgun and the soldier textured and exported.
TOKEN=tf_ready_$$
bash "$KIT/run.sh" "$AGENT" -Script "map e1m1;wait10;echo $TOKEN" \
    -QuitOn "vr_model_watch: changed on disk, read again:.*soldier\.mdl" -Timeout 400 \
    -Filter "vr_model_watch: changed|exit=" > "$T/game.out" 2>&1 &
GAME=$!
ready=0
for i in $(seq 1 900); do
    if grep -q "$TOKEN" "$LOG" 2>/dev/null; then ready=1; break; fi
    sleep 0.2
done
check $ready "the game loaded e1m1"
blender_half v_shot 0
blender_half soldier 0
wait $GAME
cat "$T/game.out"
check $(has "changed on disk, read again:.*v_shot\.mdl" "$T/game.out") "the game read v_shot.mdl again by itself"
check $(has "changed on disk, read again:.*soldier\.mdl" "$T/game.out") "the game read soldier.mdl again by itself"

# 2. A model's other skin.
blender_half armor 2
put_back
check $(git -C "$TREE" diff --quiet -- quakevr/progs quakevr/data/weapons.txt && echo 1 || echo 0) \
    "the shipped files put back byte for byte"

# 3. vr_edit_skin.
out=$(bash "$KIT/run.sh" "$AGENT" -Script "map e1m1;wait10;vr_edit_skin armor 2 -n;vr_edit_skin v_shot -n;echo tf_done;toggleconsole;quit" \
    -QuitOn "tf_done" -Timeout 120 -Filter "vr_edit_skin|edit_skin_from|tf_done" 2>&1 | tr -d '\r\n')
check $(grep -q "armor.blend\" --python-expr \"import quakevr_models as q; q.edit_skin_from_game(2)" <<<"$out" && echo 1 || echo 0) \
    "vr_edit_skin armor 2: opens armor.blend and edits skin 2"
check $(grep -q "v_shot skin -1: would open .*v_shot.blend" <<<"$out" && echo 1 || echo 0) \
    "vr_edit_skin v_shot: opens v_shot.blend, the panel's skin"
verdict
exit $fail
