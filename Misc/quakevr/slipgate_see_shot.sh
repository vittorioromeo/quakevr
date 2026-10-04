#!/usr/bin/env bash
# What the view through a slipgate actually draws, from far away and from near.
# Needs the portal scene's own image read back in renderPortal (the portalShotMap / vr_portals_shot diagnostic, taken
# out at the end of this round: it is the tool that showed the portal scene itself renders dark, not the selection).
# Usage: bash Misc/quakevr/slipgate_see_shot.sh [map] [trigger]
set -u
MAP=${1:-start}
TRIG=${2:-1}
W="C:/OHWorkspace/qvr-agents/slipgate-see"
W=$(cygpath -u "$W")
S="setvr vr_move 0; setvr vr_teleport 0; setvr vr_eyeshot_mode 0; setvr vr_tonemap_auto 0; setvr vr_tonemap 0; setvr vr_foveated 0; setvr vr_gadget_text 0; setvr vr_hand_debug 0; setvr vr_hand_visualisation 0; setvr vr_hand_occlusion 0; setvr vr_body 0; setvr vr_arms 0; setvr vr_legs 0; setvr vr_mirrors 0; setvr vr_menu_render 0; setvr vr_hud 0; setvr vr_targeting 0; setvr vr_target 0; setvr vr_target_range 0; setvr vr_target_lock 0; setvr vr_target_helper 0; setvr vr_target_helper_haptic 0; setvr vr_target_helper_sound 0; setvr vr_target_helper_tacton 0; setvr vr_hand_sound 0; setvr vr_hand_solid 0; setvr vr_hand_nudge 0; setvr vr_hand_nudge_sound 0; setvr vr_hand_nudge_haptic 0; setvr vr_hand_nudge_tacton 0; setvr vr_handcal_roll 0; setvr vr_handcal_yaw 0; setvr vr_handcal_pitch 0; setvr vr_handcal_x 0; setvr vr_handcal_y 0; setvr vr_handcal_z 0; setvr vr_gunangle 39.5; setvr vr_gunyaw 4; setvr vr_move 0; setvr vr_teleport 0; setvr vr_eyeshot_mode 0; setvr vr_tonemap_auto 0; setvr vr_tonemap 0; setvr vr_foveated 0; setvr vr_gadget_text 0; setvr vr_hand_debug 0; setvr vr_hand_visualisation 0; setvr vr_hand_occlusion 0; setvr vr_body 0; setvr vr_arms 0; setvr vr_legs 0; setvr vr_mirrors 0; setvr vr_menu_render 0; setvr vr_hud 0; setvr vr_targeting 0; setvr vr_target 0; setvr vr_target_range 0; setvr vr_target_lock 0; setvr vr_target_helper 0; setvr vr_target_helper_haptic 0; setvr vr_target_helper_sound 0; setvr vr_target_helper_tacton 0; setvr vr_hand_sound 0; setvr vr_hand_solid 0; setvr vr_hand_nudge 0; setvr vr_hand_nudge_sound 0; setvr vr_hand_nudge_haptic 0; setvr vr_hand_nudge_tacton 0; setvr vr_handcal_roll 0; setvr vr_handcal_yaw 0; setvr vr_handcal_pitch 0; setvr vr_handcal_x 0; setvr vr_handcal_y 0; setvr vr_handcal_z 0; setvr vr_gunangle 39.5; setvr vr_gunyaw 4; "
bash C:/OHWorkspace/qvr-kit/run.sh slipgate-see -Timeout 240 -Filter "VR portal shot|^ |mean |chosen|side [0-9] " -Script "\
map $MAP; wait1; setvr vr_portals_view 1; setvr vr_portals $TRIG; setvr vr_portals_debug 1; setvr vr_portals_info 1; \
setpos 232 400 24 0 90 0; wait4; vr_portals_shot; wait4; \
setpos 232 1300 24 0 90 0; wait4; vr_portals_shot; wait4; \
toggleconsole; quit"
