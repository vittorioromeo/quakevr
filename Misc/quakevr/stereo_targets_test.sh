#!/bin/bash
# stereo_targets_test.sh <agent>: the eyes' GL objects over frames (vr_stereo_targets_test, vr_stereo.cpp), with the
# mock headset's hidden area (vr_mock_hidden_area 1). start, facing the hall, still: begun after 25 frames, then 12
# frames on: each eye's hidden area made once and not uploaded again (it changes only with the runtime's mesh or the
# eye's field of view). Ends on `stereo_targets: N passed, M failed`.
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
F='stereo_targets|ENGINE CRASH|Host_Error'
S='vr_mock_hidden_area 1;map start;wait60;god;notarget;setpos 544 420 24 0 270 0;wait25'
S="$S;vr_stereo_targets_test begin;wait12;vr_stereo_targets_test steady;vr_stereo_targets_test verdict;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Timeout 300 -QuitOn 'stereo_targets: [0-9]+ passed' -Script "$S" -Filter "$F"
