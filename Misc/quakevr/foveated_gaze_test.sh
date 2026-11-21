#!/bin/bash
# foveated_gaze_test.sh <agent> -- eye-tracked foveation by default (vr_foveated_gaze 1; vr_foveated.cpp fovealCentre,
# gazeStatus; ROUND25.md "comfort2"). The mock headset's gaze 20 degrees right, 5 up (vr_mock_gaze), in e1m1:
# 1. its eye tracker on (vr_mock_eye_tracking 1, as shipped): `vr_foveated_gaze_test` (the rates round a gaze, then the
# live centre follows the mock's gaze; the menu line says the eyes); 2. the runtime offering none
# (vr_mock_eye_tracking 0), the next frames, nothing restarted: the lens centre, the gaze ignored, the line says so;
# 3. the tracker back: the eyes again. Each step waits the two frames the mock's next frame state needs.
# Ends "foveated_gaze: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
T="vr_foveated_gaze_test"
S="map e1m1;vr_doubletap_test ready;vr_foveated_gaze 1;vr_mock_gaze \"20 5\";wait2;$T;"
S="${S}vr_mock_eye_tracking 0;wait2;$T;vr_mock_eye_tracking 1;wait2;$T;"
OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Timeout 240 -Filter "^vr_foveated_gaze_test|rror|CRASH" 2>&1)
grep -E "CRASH|rror|FAIL|status" <<<"$OUT"
v() { grep -aE "^$T: [0-9]+ passed, [0-9]+ failed" <<<"$OUT" | sed -n "${1}p"; }
ok() { [ -n "$(v $1)" ] && grep -q ", 0 failed" <<<"$(v $1)" && echo 1 || echo 0; }
st() { grep -a "^$T: eye tracker" <<<"$OUT" | sed -n "${1}p"; }
check $(ok 1) "an eye tracker: the centre follows the gaze, the line says the eyes ($(v 1); $(st 1))"
check $([ "$(ok 2)" = 1 ] && grep -q "tracker 0" <<<"$(st 2)" && echo 1 || echo 0) "none offered, live: the lens centre, the line says so ($(v 2); $(st 2))"
check $([ "$(ok 3)" = 1 ] && grep -q "tracker 1" <<<"$(st 3)" && echo 1 || echo 0) "the tracker back: the eyes again ($(v 3); $(st 3))"
verdict; exit $fail
