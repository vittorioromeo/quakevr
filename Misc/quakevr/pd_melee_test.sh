#!/bin/bash
# pd_melee_test.sh <agent> [flat]: `vr_pd_test 2` (QC/vr_playerdisarm_test.qc): a gun-and-axe grunt and an axe-only one
# fight you holding the shotgun out (the mock headset, or flat-screen play with "flat"); every blow held back for your
# weapon near its way ends, landed blows hurt, rolls on your weapon knock it out. Ends on "pdtest: N passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posebake}
EXTRA=""
if [ "$2" = "flat" ]; then EXTRA="-ExtraArgs -flatscreenmode"; fi
bash $KIT/run.sh $AGENT $EXTRA -Script "map vrtesthall; developer 1; wait5; vr_pd_test 2" \
  -QuitOn "^pdtest: [0-9]+ passed" -Timeout 240 -Filter "^pdtest|PASS|FAIL"
