#!/bin/bash
# axe_stick_rates.sh <agent> <out dir> [extra cmds] -- the thrown axe stick rates (ROUND21.md, "Thrown axes: the blade
# decides"): 60 synthetic hand throws (vr_test_axe 4 overhand, 5 sidearm, 6 sloppy, 8 upright, 9 knife-style, 10
# spear-like, 11 flat, 12 handle first) at vrfiringrange's target wall, each kind one run, vr_debug_axestick 1; prints
# each kind's stick rate and why the others bounced. The cfgs go to the agent's quakevr folder (axerate_<mode>.cfg).
AGENT=$1; OUT=$2; EXTRA=$3; HERE=$(cd "$(dirname "$0")" && pwd)
KIT=${KIT:-C:/OHWorkspace/qvr-kit}; G=$(cd "$HERE/../../quakevr" && pwd)
mkdir -p "$OUT"
for m in 4 5 6 8 9 10 11 12; do
    python "$HERE/axe_stick_rates.py" gen $m 60 $((m * 7 + 3)) "$G/axerate_$m.cfg"
    bash $KIT/run.sh $AGENT -Timeout 900 -Script "map vrfiringrange;wait60;god;notarget;developer 0;vr_debug_axestick 1;$EXTRA;exec axerate_$m.cfg;toggleconsole;quit" -Filter "AXETHROW|AXEDONE|axestick:" > "$OUT/axerate_$m.txt" 2>&1
    echo "mode $m: $(python "$HERE/axe_stick_rates.py" parse "$OUT/axerate_$m.txt" | tr '\n' ' ')"
done
