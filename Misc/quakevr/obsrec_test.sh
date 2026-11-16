#!/bin/bash
# obsrec_test.sh <agent>: clean recording (his note 2026-11-16; ROUND25.md, "Clean recording"): with OBS recording (the test
# aid vr_obsrec_as_if 1) no Ghost Hands are drawn though one would be (Ghost Hands on, the ghost held 15 cm off the drawn
# hand by vr_ghost_hands_test_offset, no fade margin), the other options default on and act; with No Ghost Hands While
# Recording off the same frames show it; not recording, nothing hidden. 30 frames each phase (the ghost is placed every
# frame). Ends on "vr_obsrec_test: N passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-takeslight}
S="map vrtesthall;wait30;god;notarget;vr_ghost_hands 1;vr_ghost_hands_fade_from 0;vr_ghost_hands_fade_range 0.1;vr_ghost_hands_test_offset 15"
S="$S;vr_obsrec_as_if 1;wait10;vr_obsrec_test start;wait30;vr_obsrec_test hidden;vr_obsrec_ghost_hands 0;wait30;vr_obsrec_test end"
S="$S;vr_obsrec_ghost_hands 1;vr_ghost_hands 0;vr_ghost_hands_test_offset 0;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_obsrec_test" -QuitOn "vr_obsrec_test: \d+ passed" -Timeout 120
