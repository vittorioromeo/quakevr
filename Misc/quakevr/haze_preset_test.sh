#!/bin/bash
# haze_preset_test.sh <agent> -- Heat Haze Detail by graphics preset (his notes 2026-10-26): each preset (0 Off .. 4
# Ultra) sets vr_heat_haze_half (half size below High, full size from High) and is then recognised as itself; and the
# config migration (vr_cfg_version 205): a config at the old default 1 keeps it on a preset below High and takes 0 on
# High or its own mix. Prints "hazepreset: N passed, M failed"; the run quits itself after its last step.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
T=C:/OHWorkspace/qvr-agents/$AGENT/scratch; mkdir -p "$T"
S="map e1m1;wait10"
for p in 0 1 2 3 4; do
    S="$S;vr_graphics_preset $p;wait2;echo hazepreset preset $p;vr_heat_haze_half;vr_graphics_preset_status"
done
# migration: Medium at the old 1, High at the old 1, High with one other setting changed (its own mix) at the old 1
S="$S;vr_graphics_preset 2;wait2;vr_heat_haze_half 1;vr_cfg_version 204;vr_migrate_config;echo hazepreset migrate medium;vr_heat_haze_half"
S="$S;vr_graphics_preset 3;wait2;vr_heat_haze_half 1;vr_cfg_version 204;vr_migrate_config;echo hazepreset migrate high;vr_heat_haze_half"
S="$S;vr_graphics_preset 3;wait2;vr_heat_haze_half 1;vr_heat_haze 0.5;vr_cfg_version 204;vr_migrate_config;echo hazepreset migrate custom;vr_heat_haze_half"
S="$S;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "^hazepreset|vr_heat_haze_half|^graphics preset:|rror" -Timeout 120 > "$T/hazepreset.log" 2>&1
python "C:/OHWorkspace/qvr-agents/$AGENT/Misc/quakevr/haze_preset_check.py" "$T/hazepreset.log"
