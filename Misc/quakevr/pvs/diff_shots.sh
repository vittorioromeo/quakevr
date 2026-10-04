#!/bin/bash
# Diff the 10 sweep shots (name order = take order): 0..4 = vr_pvs_selfleaf 0 at yaw -60,-30,0,30,60;
# 5..9 = the same with 1. Prints off-vs-on per yaw, and each flag's own yaw stability.
D="C:/OHWorkspace/qvr-kit/bases/pvs-selfbit/qbase/quakevr/screenshots"
cd "$D" || exit 1
mapfile -t F < <(ls -1 *.png | sort)
P=C:/OHWorkspace/qvr-agents/pvs-selfbit/Misc/quakevr/pvs/pngdiff.py
Y=(-60 -30 0 30 60)
for i in 0 1 2 3 4; do echo "yaw ${Y[$i]}: off-vs-on"; python "$P" "${F[$i]}" "${F[$i+5]}"; done
for i in 1 2 3 4; do echo "off yaw ${Y[0]} vs ${Y[$i]}"; python "$P" "${F[0]}" "${F[$i]}"; echo "on  yaw ${Y[0]} vs ${Y[$i]}"; python "$P" "${F[5]}" "${F[$i+5]}"; done
