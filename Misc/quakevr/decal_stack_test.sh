#!/usr/bin/env bash
# decal_stack_test.sh <agent> [kit] - pools piled on top of each other and next to each other never leave holes in
# the older ones (1.0.1: gibs' pools piled up left square, grid-cell holes in those under them).
#
# vrfiringrange's start, looking at the floor 96 units ahead: 8 pools stacked at one spot (vr_decal_stress 8 112 stack), shot
# once spread (and again, as a control: the scene's own flicker); then 128 smaller pools clustered round the spot and
# 16 more stacked on it, shot again once spread. The marks multiply the texture, so more marks only darken a pixel:
# decal_stack_check.py fails on pixels lighter after the new pools (an older mark gone under them).
set -u
agent="${1:?usage: decal_stack_test.sh <agent> [kit]}"
kit="${2:-C:/OHWorkspace/qvr-kit}"
here="$(dirname "$0")"
shots="$kit/bases/$agent/qbase/quakevr/screenshots"
script="map vrfiringrange;wait60;r_drawviewmodel 0;vr_mock_look 35 0;wait30;vr_decal_stress 1 112 stack;wait900"
script="$script;cl_screenshotname screenshots/stack_before;screenshot;wait60"
script="$script;cl_screenshotname screenshots/stack_control;screenshot;wait60"
script="$script;vr_decal_stress 64 24 cluster;wait2;vr_decal_stress 64 24 cluster;wait900"
script="$script;vr_decal_count;cl_screenshotname screenshots/stack_after;screenshot;wait60;toggleconsole;quit"
bash "$kit/run.sh" "$agent" -Clean -Script "$script" -Filter "decals:|world decal grid|stress" -Out decal_stack.png || exit 2
python "$here/decal_stack_check.py" "$shots/stack_before0000.png" "$shots/stack_after0000.png" "$shots/stack_control0000.png"
