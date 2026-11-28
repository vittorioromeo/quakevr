#!/bin/bash
# flatpad_test.sh <agent> -- a gamepad in flat-screen play (ROUND26 "padflat"; FLATPLAY.md "A gamepad"), -flatscreenmode:
# - vr_flat_pad_test: the default layout (every common action on a button, no button on two actions, nor a flat key on
#   fire / jump / the alt layer / the menu), the alt layer (Back held: X is next ammo, let go in either order), an older
#   config filled once, Reset, the Controller Bindings screen's and reminder panel's names for the pad's flat actions;
# - vr_bindsetup_test: the screen's swaps and reset (a pad's now the flat keys' buttons);
# - vr_menu_pad_test: the menus' gamepad cursor (the stick moves it, steered onto B's label A clicks it open, the d-pad
#   steps and hides it), a stage a frame.
# Screenshot: scratch/flatpad/bindings.png (the Controller Bindings screen, an Xbox pad, the new defaults).
# Ends "flatpad_test: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=flatpad_test
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatpad
mkdir -p "$S"
SCRIPT="developer 1; map vrtesthall; wait30; vr_flat_pad_test; vr_bindsetup_test; vr_menu_pad_test"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$SCRIPT" -QuitOn "^menu_pad_test: [0-9]+ passed" -Timeout 120 \
    -Filter "^(flat_pad_test|bindsetup_test|menu_pad_test|Quake VR: [0-9]+ gamepad)" > "$S/run.log" 2>&1
L=$(cat "$S/run.log")
has() { grep -qE "$1" <<<"$L" && echo 1 || echo 0; }
check $(has "^flat_pad_test: [1-9][0-9]* passed, 0 failed") "vr_flat_pad_test: the layout, the alt layer, filling, reset, the screen's names"
check $(has "^bindsetup_test: [1-9][0-9]* passed, 0 failed") "vr_bindsetup_test: swaps and reset (a pad's flat keys)"
check $(has "^menu_pad_test: [1-9][0-9]* passed, 0 failed") "vr_menu_pad_test: the stick's cursor, A clicks, the d-pad steps"
grep -E "FAIL" "$S/run.log" | head -12
SCRIPT="developer 1; map vrtesthall; wait30; menu_bindsetup xboxone; wait20; screenshot; echo flatpad_shot_end; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$SCRIPT" -QuitOn "^flatpad_shot_end" -Timeout 120 \
    -Filter "^(flatpad)" -Out "$S/bindings.png" > "$S/run_shot.log" 2>&1
check $([ -s "$S/bindings.png" ] && echo 1 || echo 0) "the bindings screen's screenshot"
verdict
exit $fail
