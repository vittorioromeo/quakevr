#!/bin/bash
# holsters2_test.sh <agent> [case...] -- ROUND22.md, "Magnetic holsters: land first, the tether at the holster; toolgun
# spawns". Cases: land (Land First 1 s: at rest, then home), noland (Land First 0: home in the air), rebind (bound at
# holster 3, moved into 5, bound there: only 5 bound), toolgun (vr_debug_spawn_toolgun, the reach spawn), shot (the
# tether from the hip holster to the gun in the hand: scratch/holsters2_tether.png).
AGENT=$1; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-land noland rebind toolgun}
P="developer 1;map e1m1;wait60;god;notarget;"
BIND="vr_mock_hand_to off holster 3;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;"
BIND2="vr_mock_hand_to off holster 5;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;"
TOSS="vr_mock_hand_to main by 0 30 25;wait5;-grabright;vr_mock_button main grip 0;wait10;"
INFO="vr_magholster_info;"
F="magholster|toolgun|vr_physics_spawn|exit=|ERROR|CRASH|fell on|throw hit"
for c in $CASES; do
    EXTRA=""
    case $c in
    land) S="$P vr_magholster_return_time 0.5;vr_magholster_return_distance 0;vr_magholster_land_time 1;$BIND$DRAW$TOSS${INFO}wait400;$INFO" ;;
    noland) S="$P vr_magholster_return_time 0.5;vr_magholster_return_distance 0;vr_magholster_land_time 0;$BIND$DRAW$TOSS${INFO}wait400;$INFO" ;;
    rebind) S="$P vr_magholster_return_time 0;vr_magholster_return_distance 0;$BIND$INFO${DRAW}vr_mock_hand_to main holster 5;wait10;-grabright;vr_mock_button main grip 0;wait10;vr_mock_hand main 0.3 1.0 -0.2 0 0 0;wait5;$INFO$BIND2$INFO" ;;
    toolgun) S="vr_debug_spawn_toolgun 1;$P wait5;vr_physics_spawn weapon_toolgun reach;wait5;vr_physics_spawn weapon_toolgun reach;changelevel e1m2;wait90;vr_debug_spawn_toolgun 0;" ;;
    shot) S="map vrfiringrange;wait60;developer 1;god;notarget;vr_magholster_style 1;vr_magholster_return_time 0;vr_magholster_return_distance 0;${BIND}vr_mock_hand off -0.3 1.0 -0.1 0 0 0;${DRAW}vr_mock_hand main 0.42 1.15 -0.25 50 0 0;wait30;vr_mock_camera 0.55 1.0 -0.05 20 95;wait20;${INFO}screenshot;vr_mock_camera off;"
          EXTRA="-Clean -Out holsters2_tether.png" ;;
    *) echo "unknown case $c"; exit 2 ;;
    esac
    echo "== $c"
    bash $KIT/run.sh $AGENT $EXTRA -Script "${S}toggleconsole;quit" -Filter "$F|composed"
done
