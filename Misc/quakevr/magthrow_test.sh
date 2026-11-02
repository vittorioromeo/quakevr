#!/bin/bash
# magthrow_test.sh <agent>: thrown magazines hit as light props (vr_prop_impact_test 6; ROUND23.md, "Thrown magazines
# hit as light props"): a nailgun magazine, a super nailgun's and a cell thrown hard into a grunt on vrarena's flat
# floor (made about 600 ahead of the start, past its steps): about 1 damage each and its flinch or pain. Ends on "magthrow: N
# passed, M failed". MTS="cmd;" runs cmd first (e.g. "developer 1;").
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
S="sv_random_seed 7;map vrarena;wait30;god 1;notarget 1;${MTS}+forward;wait90;-forward;wait10;vr_prop_impact_test 6"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "${MTF:-magthrow}" -QuitOn "magthrow: \d+ passed" -Timeout 120
