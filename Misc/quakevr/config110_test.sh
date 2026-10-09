#!/bin/bash
# config110_test.sh <agent> -- config version 110 (ROUND21.md, "The author's settings of the afternoon of 2026-10-09 are
# the defaults"), headless: a config of version 109 holding every old default takes the new ones; a setting the player
# changed (vr_stealth_graze 50, gl_texture_anisotropy 4) keeps its value. Expected: NEW lines show the new defaults,
# KEPT lines 50 and 4, the first line the shipped defaults at start (anisotropy 16, liquids 0.3 0.9 0.6 0.9, maxfps 250).
AGENT=${1:-defaults}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
OLD="vr_bullettime_tap_gesture 0;vr_bullettime_tap_angle 10;vr_bullettime_tap_butt_depth 4;vr_bullettime_tap_depth 6.5"
OLD="$OLD;vr_bullettime_tap_double_speed 0.4;vr_bullettime_tap_double_window 0.4;vr_bullettime_tap_height 0.75"
OLD="$OLD;vr_bullettime_tap_margin 1;vr_bullettime_tap_width 1;vr_foegrab_break 35;vr_foegrab_drag 10"
OLD="$OLD;vr_foegrab_drag_speed 200;vr_foegrab_leniency 1.5;vr_stealth_light_bright 80;vr_stealth_light_dark 20"
OLD="$OLD;vr_stealth_lose_time 20;vr_stealth_meter_decay 0.2;vr_stealth_meter_time 1;vr_stealth_noise_blasts 1"
OLD="$OLD;vr_stealth_noise_guns 1;vr_stealth_noise_props 1200;vr_stealth_noise_wall 0.5"
P="echo VALUES: gl_texture_anisotropy;gl_texture_anisotropy;vr_bullettime_tap_gesture;vr_bullettime_tap_angle"
P="$P;vr_bullettime_tap_butt_depth;vr_bullettime_tap_depth;vr_bullettime_tap_double_speed;vr_bullettime_tap_double_window"
P="$P;vr_bullettime_tap_height;vr_bullettime_tap_margin;vr_bullettime_tap_width;vr_foegrab_break;vr_foegrab_drag"
P="$P;vr_foegrab_drag_speed;vr_foegrab_leniency;vr_stealth_graze;vr_stealth_light_bright;vr_stealth_light_dark"
P="$P;vr_stealth_lose_time;vr_stealth_meter_decay;vr_stealth_meter_time;vr_stealth_noise_blasts;vr_stealth_noise_guns"
P="$P;vr_stealth_noise_props;vr_stealth_noise_wall;vr_cfg_version"
S="wait5;echo === START;gl_texture_anisotropy;r_wateralpha;r_lavaalpha;r_slimealpha;r_telealpha;host_maxfps"
S="$S;echo === NEW;vr_cfg_version 109;$OLD;vr_stealth_graze 64;gl_texture_anisotropy 8;vr_migrate_config;$P"
S="$S;echo === KEPT;vr_cfg_version 109;$OLD;vr_stealth_graze 50;gl_texture_anisotropy 4;vr_migrate_config"
S="$S;vr_stealth_graze;gl_texture_anisotropy;vr_foegrab_drag;echo === END;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|\" is \"" "$@"
