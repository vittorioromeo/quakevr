#!/usr/bin/env bash
# decal_stack_test.sh <agent> [kit] - pools piled on top of each other and next to each other never leave holes in
# the older ones (1.0.1: gibs' pools piled up left square, grid-cell holes in those under them).
#
# vrfiringrange's start, looking at the floor 96 units ahead: 3 pools stacked on one spot (vr_decal_stress 3 112
# stack), shot once spread (and again, as a control: a monster idling, a flicker); then 128 smaller pools and splatters
# clustered round the spot (vr_decal_stress 64 24 cluster, twice: a gib's burst), shot again once spread. The marks
# multiply the texture, so more marks only darken a pixel: decal_stack_check.py fails on pixels lighter after the new
# marks (an older mark gone under them). Exit 0 pass, 1 holes, 2 the run failed. Before the fix (the newest 64 of each
# grid bucket listed): about 1950 pixels lighter, in cell-edged patches; after: under 20.
set -u
agent="${1:?usage: decal_stack_test.sh <agent> [kit]}"
kit="${2:-C:/OHWorkspace/qvr-kit}"
here="$(dirname "$0")"
shots="$kit/bases/$agent/qbase/quakevr/screenshots"
script="nomonsters 1;map vrfiringrange;wait60;vr_mock_look 35 0;wait30;vr_decal_stress 3 112 stack;wait900"
script="$script;cl_screenshotname screenshots/stack_before;screenshot;wait60"
script="$script;cl_screenshotname screenshots/stack_control;screenshot;wait60"
script="$script;vr_decal_stress 64 24 cluster;wait2;vr_decal_stress 64 24 cluster;wait900"
script="$script;vr_decal_count;cl_screenshotname screenshots/stack_after;screenshot;wait60;nomonsters 0;toggleconsole;quit"
bash "$kit/run.sh" "$agent" -Clean -Script "$script" -Filter "decals:|world decal grid|stress" -Out decal_stack.png || exit 2
python "$here/decal_stack_check.py" "$shots/stack_before0000.png" "$shots/stack_after0000.png" "$shots/stack_control0000.png"
