#!/bin/bash
# questcrash_preset_test.sh <agent> -- graphics presets picked mid-game on the Quest (his report 2026-11-26: Standalone
# -> "Quake (off)" -> Low left the Quest under 1 fps, Standalone out of reach). Under the Quest's profile
# (QVR_GLES_COMPAT=1, as -glescompat): a preset picked from Standalone keeps the headset's budget rows at Standalone's
# (the eyes' resolution, dynamic resolution, foveation, OIT, MSAA) and the must-haves hold (Low's shadowed dynamic
# lights, Medium's shadowed map lights: 0); a pick whose frames stay under vr_preset_revert_fps is put back
# (vr_preset_revert_test 500: each frame counted as 500 ms), one at 20 ms kept. On the PC (no ES profile) leaving
# Standalone still puts its rows back as they were before it. The frame counts waited are what the timing needs
# (a second's grace and 8 frames at the counted frame time). Prints "questpreset: N passed, M failed".
AGENT=${1:-quest}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
T=C:/OHWorkspace/qvr-agents/$AGENT/scratch; mkdir -p "$T"
Q="vr_render_scale;vr_xr_foveation;r_oit;vid_fsaa;vr_dynres;vr_tonemap;vr_shadow_dlights;vr_shadow_maplights;vr_normalmaps"
S="map e1m1;wait10;vr_standalone_preset 0;wait2;echo QP standalone;$Q"
S="$S;vr_graphics_preset 0;wait2;echo QP off;$Q"
S="$S;vr_graphics_preset 1;wait2;echo QP low;$Q"
S="$S;vr_graphics_preset 2;wait2;echo QP medium;$Q"
S="$S;vr_preset_revert_test 20;vr_graphics_preset 1;wait70;echo QP kept;$Q"
S="$S;vr_preset_revert_test 500;vr_graphics_preset 3;wait15;echo QP reverted;$Q"
S="$S;vr_preset_revert_test 0;echo QP end;toggleconsole;quit"
F="^QP |^\"(vr_|r_oit|vid_fsaa)|put back|budget rows|graphics preset \(Quest\)|rror"
env QVR_GLES_COMPAT=1 bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "$F" -QuitOn "^QP end" -Timeout 180 > "$T/questpreset_es.log" 2>&1
P="map e1m1;wait10;vr_standalone_preset 0;wait2;vr_graphics_preset 0;wait2;echo QP pcoff;$Q;echo QP end;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Script "$P" -Filter "$F" -QuitOn "^QP end" -Timeout 120 > "$T/questpreset_pc.log" 2>&1
python "C:/OHWorkspace/qvr-agents/$AGENT/Misc/quakevr/questcrash_preset_check.py" "$T/questpreset_es.log" "$T/questpreset_pc.log"
