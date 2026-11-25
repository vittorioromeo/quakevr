#!/bin/bash
# flatvronly_test.sh <agent> -- the maps' VR-only setting buttons in flatscreen play (his ask 2026-11-25; ROUND26.md
# "flatmisc1125"). Every setting button of vrstart, vrtutorial and vrtesthall runs `vr_setup_option <key>` (vr_setup.cpp);
# a headset-only one (Option::vrOnly: turning, sticks, teleport, grip, gadget, flashlight side, position, world scale, HUD)
# changes nothing in flatscreen play and its screen reads "VR only", greyed. In vrtesthall: Turning (VR only) and Default
# Speed (both modes) pressed, flat then with the headset; each run ends with its own script.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
S="map vrtesthall; wait30; vr_snap_turn 45; cl_alwaysrun 1; wait2; vr_setup_option turning; vr_setup_option run; wait2; vr_snap_turn; cl_alwaysrun; vr_setup_option list; toggleconsole; quit"
F='"vr_snap_turn" is|"cl_alwaysrun" is|^vr_setup_option: flatscreen|VR only, no effect|^Turning:|^Default Speed:'
val() { grep -a "\"$2\" is" "$1" | head -1 | sed 's/^[^"]*"[^"]*" is "\([^"]*\)".*/\1/'; }
for mode in flat vr; do
  log=$HERE/scratch/flatvronly_$mode.log
  args=$([ $mode = flat ] && echo "-ExtraArgs -flatscreenmode" || echo "")
  bash $KIT/run.sh $AGENT $args -Script "$S" -Filter "$F" -Timeout 90 > "$log" 2>&1
  snap=$(val "$log" vr_snap_turn); run=$(val "$log" cl_alwaysrun)
  grey=$(grep -a "^vr_setup_option: flatscreen" "$log" | sed 's/.*last frame //' | tr -d '\r')
  echo "  $mode: vr_snap_turn 45 -> $snap, cl_alwaysrun 1 -> $run, VR-only screens greyed: $grey"
  if [ $mode = flat ]; then
    check $([ "$snap" = "45" ] && grep -aq "Turning: VR only, no effect" "$log" && echo 1 || echo 0) "flat: the Turning button (VR only) changes nothing ($snap)"
    check $([ "$run" = "0" ] && echo 1 || echo 0) "flat: the Default Speed button (both modes) still steps ($run)"
    check $([ "${grey:-0}" -gt 0 ] 2>/dev/null && echo 1 || echo 0) "flat: the VR-only buttons' screens drawn greyed ($grey)"
  else
    check $([ "$snap" = "90" ] && [ "$run" = "0" ] && echo 1 || echo 0) "headset: both buttons step as before (turning $snap, run $run)"
    check $([ "$grey" = "0" ] && echo 1 || echo 0) "headset: no screen greyed ($grey)"
  fi
done
verdict; exit $fail
