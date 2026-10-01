#!/bin/bash
# Sliding off a steep box (ROUND21.md, "Sliding off steep boxes"): in vrfiringrange the long explosive box (207) is
# leant on the small one, settling about 33 degrees steep; the player is dropped on its face and printed every frame.
#   bash Misc/quakevr/boxslide/run.sh <agent> [cvars, e.g. "vr_box3d_player_slope 46"]
# Prints frame time, origin, velocity, ground; then the box. With vr_box3d_player_slope 30 he slides off onto the floor
# (z 41.11) about 0.55 s after landing on it; 46 (Quake's limit) he stands, creeping down at 7 units a second.
K=C:/OHWorkspace/qvr-kit
D=$(cd "$(dirname "$0")" && pwd); Q="$D/../../../quakevr"
cp "$D/bs_w.cfg" "$D/bs_slide.cfg" "$Q/"
echo "$2" > "$Q/bs_pre.cfg"
bash $K/run.sh "$1" -Script "exec bs_slide.cfg" -Filter "SETTLED|207 misc|vr_physics_player: [0-9]|vr_physics_inside:|rror|ENGINE" 2>&1 |
    awk '/vr_physics_player: [0-9]/{printf "%s s  at %s %s %s  vel %s %s %s  %s\n",$2,$5,$6,$7,$9,$10,$11,$12; next} {print}'
