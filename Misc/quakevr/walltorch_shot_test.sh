#!/bin/bash
# walltorch_shot_test.sh [agent] -- a wall torch on its wall shot off it (vr_walltorch_shot; ROUND22.md).
# In vrfiringrange, at the torch on the prop wall at -590 -760 75 (its face x -599), from -566 -760 40 facing it
# (its origin 24 units away, 5 under the eye). Each case reloads the map, so the same torch is on its wall every time.
# A case is: <kind> <label> [extra cvars]. Prints one CASE line per case and what the game did with it.
AGENT=${1:-torch-knockoff}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
POS="setpos -566 -760 40 0 180 0"
P="god;notarget;developer 1;sv_gravity 800;vr_debug_missiles 1;"
for c in \
    "10 pellets" \
    "1 nail" \
    "3 supernail" \
    "0 rocket" \
    "21 lightning-bolt" \
    "22 blast" \
    "20 thrown-prop" \
    "20 thrown-prop-slow vr_test_fling_speed 1" \
    "2 grenade-projectile" \
    "10 cvar-off vr_walltorch_shot 1;vr_walltorch_shot 0"; do
    set -- $c; kind=$1; label=$2; shift 2
    extra="$*"
    P="$P $extra; map vrfiringrange; wait60; echo CASE $label kind $kind; $POS; wait30; vr_test_walltorch_shot $kind; wait150;"
done
P="$P vr_walltorch_shot 1;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Timeout 900 -Filter "CASE |walltorch|test fire|missile hit|error" -Script "$P" 2>&1 | grep -v "^exit"
