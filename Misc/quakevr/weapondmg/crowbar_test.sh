#!/usr/bin/env bash
# Weapon Damage: a synthetic crowbar swing (motion_synth.py slash_horizontal_rtl --weapon crowbar --distance 0.85
# --name wd_crowbar, in quakevr/motions/synth) played at the firing range's dummy at the settings given; prints the
# dummy's readout ("Dummy: <n> damage - melee: Crowbar, ...").
# Usage: bash crowbar_test.sh <worktree name> ["<cvar> <value>;..."]   e.g. "vr_crowbar_damage 40"
kit=C:/OHWorkspace/qvr-kit
name=${1:?worktree name}
s="map vrfiringrange;wait60;developer 1;${2:-};vr_motion_play synth/wd_crowbar;wait300;toggleconsole;quit"
bash $kit/run.sh "$name" -Script "$s" -Filter "^Dummy: |rror" 2>&1 | grep -v slots
