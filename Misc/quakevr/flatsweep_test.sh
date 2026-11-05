#!/bin/bash
# flatsweep_test.sh <agent> [label] [console commands] [sheet.png]: the flatscreen pose sweep (docs/vr-port/FLATPLAY.md, "The pose sweep").
# Three headless runs of vr_flat_sweep, each ending on its own line (-QuitOn `flatsweep: ... done`):
#   open: vrtesthall, flat floor and far walls: every stance, standing and crouching, hipfire and ironsight, -85 to 85 degrees;
#   wall: the same hall, the long gun with his box against the wall he faces;
#   low:  e1m1, crouched under the lowest ceiling its traces find a box fits under.
# flatsweep_check.py judges the rows against flatsweep_known.txt, which lists what fails today and the phase that fixes it:
# prints `flatsweep: N passed, K known failing, M failed`. Fast mode: game logic as ever, frames uncapped. vr_flat_bob 0: its
# breathing never settles. The rows and the pictures stay in the run's screenshots folder (the -Out line says where).
AGENT=${1:-flatsweep}; LABEL=${2:-now}; EXTRA=${3:-echo}; SHEET=${4:-}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
run() { # label, mode, map, set-up
    log=$HERE/scratch/flatsweep_$1.log
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map $3; god; notarget; r_norefresh 0; vr_flat_bob 0; viewsize 120; $4; $EXTRA; wait30; vr_flat_sweep $1 $2 5" \
        -QuitOn "flatsweep: .* (done|FAIL)" -Filter "flatsweep: |ENGINE|TIMEOUT|composed" -Timeout 500 -Out "fs_raw_$1.png" > "$log" 2>&1
    grep -a -E "flatsweep: |ENGINE|TIMEOUT" "$log" | cut -c1-160
}
run "$LABEL" open vrtesthall echo
run "${LABEL}_wall" wall vrtesthall echo
run "${LABEL}_low" low e1m1 echo
SHOTS=$(grep -a -h 'full size in' "$HERE/scratch/flatsweep_$LABEL.log" | sed 's/.*full size in //' | tr -d '\r' | tail -1)
if [ -n "$SHEET" ]; then
    powershell -NoProfile -ExecutionPolicy Bypass -File "$HERE/flatsweep_sheet.ps1" -Shots "$SHOTS" -Label "$LABEL" -Out "$SHEET"
fi
FILES=""
for l in "$LABEL" "${LABEL}_wall" "${LABEL}_low"; do
    cp "$SHOTS/flatsweep_$l.csv" "$HERE/scratch/flatsweep_$l.csv"
    FILES="$FILES $HERE/scratch/flatsweep_$l.csv"
done
python "$HERE/flatsweep_check.py" $FILES --known "$HERE/flatsweep_known.txt"
