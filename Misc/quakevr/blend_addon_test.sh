#!/bin/bash
# blend_addon_test.sh <agent> -- the one Quake VR Blender add-on and the game's side of the workflow
# (docs/vr-port/BLENDER_WORKFLOW.md), headless:
#   1. the game (vr_blender_test): vr_edit_in_blender finds each model's source (a weapon, a hand-made gun, a derived
#      model, a part, a prop, the hand's and a body build's .blends; the bare-forearm body imported), the main hand's
#      weapon, Blender in Program Files; the request
#      file Show in Game writes is run (show, reload) and other lines refused; vr_model_show shows and hides;
#   2. Blender (blend_addon_test.py, the hand then the body): the add-on registers whole (the hand's tools in it, one
#      panel), takes the old hand add-on's entry out of the preferences, finds the kind; a .blend holding the imported
#      hand / body exports on save into copies (scratch/ba_addon_<kind>/progs) as shipped but the md5mesh's
#      commandline line; Show in Game's request written.
# The weapon's and a prop's export on save: blend_autoexport_test.sh (BA_MODEL=vr_crate1 for a prop).
# Prints PASS/FAIL per check and "blend_addon: N passed, M failed"; exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
TREE=C:/OHWorkspace/qvr-agents/$AGENT
BLENDER=${BLENDER:-"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"}

# 1. The game.
out=$(bash "$KIT/run.sh" "$AGENT" -Script "map e1m1;wait60;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait10;vr_blender_test;toggleconsole;quit" \
    -QuitOn "vr_blender_test: [0-9]+ passed" -Timeout 180 -Filter "vr_blender_test|exit=" 2>&1)
echo "$out" | grep "vr_blender_test: \(FAIL\|[0-9]\)"
expect_verdict "$out" vr_blender_test "the game's side: edit in Blender, the request file, vr_model_show"

# 2. Blender: the hand and the body. (The normal map bake records what it writes in generated.json: put back.)
cp "$TREE/Misc/quakevr/generated.json" "$TREE/scratch/ba_addon_generated.json"
for kind in hand body; do
    if [ $kind = hand ]; then pat='hand_rig*'; else pat='vrbody*'; fi
    W=$TREE/scratch/ba_addon_$kind
    mkdir -p "$W/progs"
    cp "$TREE"/quakevr/progs/$pat "$W/progs/"
    "$BLENDER" -b --factory-startup --python "$TREE/Misc/quakevr/blend_addon_test.py" -- $kind "$W" > "$W/out.txt" 2>&1
    grep "blend_addon_$kind: \(FAIL\|[0-9]\)" "$W/out.txt"
    expect_verdict "$(cat "$W/out.txt")" blend_addon_$kind "the $kind's .blend exported on save through the one add-on"
done
cp "$TREE/scratch/ba_addon_generated.json" "$TREE/Misc/quakevr/generated.json"
verdict
exit $fail
