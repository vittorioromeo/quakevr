#!/bin/bash
# The severed heads' flies (ROUND21.md, "Flies round a severed head"): gibs 30 enforcers on vrfiringrange
# (vr_test_spawn_dead 2), then bursts every gib and head with two blasts, and counts the flies channels heard
# (snd_show 2) at the spawn spot before and after. Fixed: "after 0".
# Usage: bash Misc/quakevr/flies/flies_test.sh <agent name> [runs]
name=${1:?agent name}; runs=${2:-3}
here=$(cd "$(dirname "$0")" && pwd)
cp "$here/flies_test.cfg" "C:/OHWorkspace/qvr-agents/$name/quakevr/flies_test.cfg"
S="map vrfiringrange;wait30;exec flies_test.cfg;wait5;g10;g10;g10;wait300;setpos 116 -556 56 0 180 0;wait10;echo PHASE_BEFORE;snd_show 2;wait10;snd_show 0;vr_physics_blast 116 -556 40 600;wait30;vr_physics_blast 116 -556 40 600;wait60;setpos 116 -556 56 0 180 0;wait30;echo PHASE_AFTER;snd_show 2;wait10;snd_show 0;echo PHASE_END;toggleconsole;quit"
for ((i = 0; i < runs; i++)); do
bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Sound -Script "$S" -Filter "head:|flys|PHASE_|rror" -Timeout 120 2>&1 | awk '
/head: flies at/ {starts++}
/head: flies stopped/ {stops++}
/PHASE_/ {phase=$1}
/flys.wav/ {split($0,a,"ENT:"); split(a[2],b,"|"); key=phase" "b[1]+0; if(!(key in seen)){seen[key]=1; n[phase]++}}
/rror/ {print}
END {printf "flies started %d, stopped %d; channels heard before the blasts %d, after %d\n", starts, stops, n["PHASE_BEFORE"], n["PHASE_AFTER"]}'
done
