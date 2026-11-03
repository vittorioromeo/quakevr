#!/bin/bash
# gibtable_test.sh <agent>: what gibs a grunt, a table of blows and props (vr_prop_impact_test 7; ROUND23.md, "Gibbing:
# overkill by blows and props"), on vrarena's flat floor, no sneak attack bonus (vr_stealth_sneak 1). Ends on "gibtable: N passed, M failed". GTS="cmd;" runs cmd first. GTF=regex: the lines kept
# (all rows by default).
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
S="sv_random_seed 7;vr_stealth_sneak 1;${GTS}map vrarena;wait30;god 1;notarget 1;+forward;wait90;-forward;wait10;vr_prop_impact_test 7"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "${GTF:-gibtable|host_error|PROG}" -QuitOn "gibtable: \d+ passed" -Timeout 240
