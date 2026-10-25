#!/bin/bash
# bodytrack_elbow_test.sh <agent> [capture.csv] -- body tracking's elbow fit (vr_bodytrack.cpp, vr_avatar.cpp solveArm;
# ROUND22.md, "Elbow fit: no hand joints through Virtual Desktop"), headless on the mock:
#   1. the synthetic body as Virtual Desktop sends it (no hand joints, only the head flagged tracked; Meta-like arms):
#      the right hand before the face, the elbow flared out, its height swept from below the shoulder to above it; each
#      step the right arm's fit weight 1 and its elbow within 1 cm of the tracked one;
#   2. with a capture (a vr_bodytrack_dump CSV in the game folder, e.g. his bodytrack_dump_real.csv): replayed
#      (vr_bodytrack_replay) once through; the arms' tracking on throughout and the elbows within 1 cm on average.
# Prints PASS or FAIL per check; exits 1 on any failure.
AGENT="$1"
CAPTURE="$2"
KIT=C:/OHWorkspace/qvr-kit
FAIL=0

S="map e1m1;wait60;vr_bodytrack_synth 1;vr_bodytrack_arms 1;vr_bodytrack_arm_fit 1;vr_bodytrack_synth_elbows 1"
S="$S;vr_mock_hand main 0.04 1.62 -0.24 0 70 0;vr_mock_hand off -0.22 1.0 -0.15 70 0 0;wait60"
for up in -0.6 -0.3 0 0.3 0.6 1.0 1.5; do
    S="$S;vr_bodytrack_synth_elbows_up $up;wait90;vr_bodytrack_stats reset;wait30;vr_bodytrack_stats up$up"
done
if [ -n "$CAPTURE" ]; then
    S="$S;vr_bodytrack_synth 0;vr_bodytrack_replay $CAPTURE;wait200;vr_bodytrack_stats reset;wait1200;vr_bodytrack_stats replay"
fi
S="$S;toggleconsole;quit"
OUT=$(bash "$KIT/run.sh" "$AGENT" -Filter "btstats|replay|rror" -Script "$S" | tr -d '\r' | sed -e ':a' -e 'N' -e '$!ba' -e 's/\n\([^b]\)/ \1/g')

check() {
    # $1 label, $2 condition result (0 pass), $3 detail
    if [ "$2" = 0 ]; then echo "PASS $1: $3"; else echo "FAIL $1: $3"; FAIL=1; fi
}
for up in -0.6 -0.3 0 0.3 0.6 1.0 1.5; do
    OFF=$(echo "$OUT" | grep "btstats up$up: elbow off" | sed 's/.* R \([0-9.]*\) .*/\1/')
    W=$(echo "$OUT" | grep "btstats up$up: fit R w" | sed 's/.*fit R w \([0-9.]*\).*/\1/')
    awk -v o="$OFF" -v w="$W" 'BEGIN { exit !(o != "" && o + 0 <= 1.0 && w + 0 >= 0.99) }'
    check "face, elbow up $up" $? "right elbow off ${OFF:-?} cm, fit weight ${W:-?}"
done
if [ -n "$CAPTURE" ]; then
    LINE=$(echo "$OUT" | grep "btstats replay: elbow off")
    L=$(echo "$LINE" | sed 's/.*off L \([0-9.]*\) .*/\1/')
    R=$(echo "$LINE" | sed 's/.* R \([0-9.]*\) .*/\1/')
    G=$(echo "$OUT" | grep -c "btstats replay: gate [LR] on 100%")
    awk -v l="$L" -v r="$R" -v g="$G" 'BEGIN { exit !(l != "" && l + 0 <= 1.0 && r + 0 <= 1.0 && g == 2) }'
    check "replay $CAPTURE" $? "elbows off L ${L:-?} R ${R:-?} cm, arms on throughout: $G of 2"
fi
exit $FAIL
