#!/bin/bash
# fgbusy_test.sh <agent> -- one busy-hand check for every grab (ROUND23, "One busy-hand check"): the hand slot's test
# (vr_pickup_test 7) and the force grab's (vr_pickup_test 8: VR_Hand_Free's exceptions; a busy off hand locks on to
# nothing, a free one locks on). On e1m1, the off hand aimed level. Ends on the second test's verdict line.
NAME="$1"; KIT=C:/OHWorkspace/qvr-kit
bash $KIT/run.sh "$NAME" -Script "map e1m1;wait15;god;developer 1;vr_mock_hand off -0.25 1.2 -0.3 70 0 0;wait2;vr_pickup_test 7;wait2;vr_pickup_test 8;wait600;toggleconsole;quit" \
    -Filter "(handslot|fgbusy) test: (FAIL|[0-9]+ passed)" -QuitOn "fgbusy test: [0-9]+ passed" -Timeout 120 2>&1 | grep "test:"
