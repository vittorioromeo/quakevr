#!/bin/bash
# gunlooks_kick.sh <agent> [log]: vr_kickcancel_info on the guns gunlooks.py wrote, its fit lines into a log for
# gunlooks_check.py --kick (no new moving parts). Ends when the last gun's info has printed (its verdict line).
AGENT=$1
LOG=${2:-C:/OHWorkspace/qvr-agents/$AGENT/scratch/gunlooks_kick.log}
KIT=C:/OHWorkspace/qvr-kit
I="vr_kickcancel_info"
S="map e1m1;wait10;$I progs/v_shot.mdl;$I progs/vr_pumpbody_on_v_shot.mdl;$I progs/vr_pump_on_v_shot.mdl;$I progs/v_nail.mdl;$I progs/v_lava.mdl;echo gunlooks_kick: done;toggleconsole;quit"
bash $KIT/run.sh "$AGENT" -Script "$S" -Filter "moving parts|part v|gunlooks_kick|rror|ENGINE" -QuitOn "gunlooks_kick: done" -Timeout 120 > "$LOG" 2>&1
grep -E "moving parts|gunlooks_kick" "$LOG" | cut -c1-110
