#!/bin/bash
# magholster_test.sh <agent> [case...] -- the magnetic holsters' checks (ROUND22.md, "Magnetic holsters"), e1m1, the
# shotgun in the right hip holster (slot 3) bound by the off hand's stick click there. Prints the QC's "magholster:"
# lines and vr_magholster_info; with no case, all of them:
#   time   drawn, tossed: flies home after vr_magholster_return_time (2 s), "home in holster 3"; no "fell on" (it hurts
#          nobody on the way)
#   walk   drawn, dropped, the player walks away: flies home past vr_magholster_return_distance (2 m); then unbound (a
#          second press at the holster), dropped and walked away from again: no flight
#   catch  tossed, flying home over 2 s: the main hand closes on it near the end: "caught on its way home"
#   save   bound, saved, unbound, loaded: bound again; tossed, a level change at once: home before e1m2, bound there
#   busy   the axe put into holster 3 while the bound shotgun is out: it waits (no flight after 1 s); the axe taken out:
#          it flies home at once (style Both, the chain half see-through; a screenshot from behind: mag_busy.png)
#   shots  the lanyard on the holstered and the held gun (the stub at the holster), the tether to the thrown gun, the
#          flight home: scratch/magholster_shots.png (vrfiringrange)
AGENT=$1; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-time walk catch save busy shots}
P="map e1m1;wait60;developer 1;god;notarget;"
BIND="vr_mock_hand_to off holster 3;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;"
DROP="vr_mock_hand_glide 0.4;vr_mock_hand_to main by 20 0 15;wait90;vr_mock_hand_glide 0;-grabright;vr_mock_button main grip 0;wait10;"
TOSS="vr_mock_hand_to main by 0 30 10;wait5;-grabright;vr_mock_button main grip 0;wait10;"
INFO="vr_magholster_info;"
WALK="vr_mock_stick off 0 1;wait200;vr_mock_stick off 0 0;wait60;"
BACK="vr_mock_camera 0.35 1.75 1.3 22 0;"
F="magholster|exit=|ERROR|CRASH|fell on|throw hit"
for c in $CASES; do
    EXTRA=""
    case $c in
    time) S="$P vr_magholster_return_time 2;vr_magholster_return_distance 0;$BIND$INFO$DRAW$TOSS${INFO}wait200;$INFO" ;;
    walk) S="$P vr_magholster_return_time 0;vr_magholster_return_distance 2;$BIND$DRAW$DROP$WALK$INFO$BIND$INFO$DRAW$DROP$WALK$INFO" ;;
    catch) S="$P vr_magholster_return_time 1;vr_magholster_return_distance 0;vr_magholster_flight_time 2;$BIND$DRAW${TOSS}wait180;${INFO}vr_mock_hand_to main weapon 0.5;wait2;vr_mock_hand_to main weapon 0.5;wait2;vr_mock_hand_to main weapon 0.5;+grabright;vr_mock_button main grip 1;wait2;vr_mock_hand_to main weapon 0.5;wait2;vr_mock_hand_to main weapon 0.5;wait30;$INFO" ;;
    save) S="$P $BIND${INFO}save magtest;wait5;$BIND${INFO}load magtest;wait60;$INFO$DRAW${TOSS}wait5;changelevel e1m2;wait90;$INFO" ;;
    busy) S="$P vr_magholster_return_time 1;vr_magholster_return_distance 0;vr_magholster_style 2;vr_magholster_chain_alpha 0.5;$BIND${DRAW}vr_mock_hand_glide 0.3;vr_mock_hand_to main by 25 0 15;wait60;vr_mock_hand_glide 0;vr_mock_hand_to off holster 2;wait5;+graboff;vr_mock_button off grip 1;wait15;vr_mock_hand_to off holster 3;wait10;-graboff;vr_mock_button off grip 0;wait10;vr_mock_hand off -0.3 1.0 -0.1 0 0 0;wait5;${TOSS}wait150;$INFO${BACK}wait5;screenshot;vr_mock_camera off;vr_mock_hand_to off holster 3;wait5;+graboff;vr_mock_button off grip 1;wait60;$INFO"
          EXTRA="-Clean -Out mag_busy.png" ;;
    shots) S="map vrfiringrange;wait60;developer 1;god;notarget;vr_magholster_style 0;vr_magholster_return_time 30;vr_magholster_return_distance 0;vr_magholster_flight_time 1.5;${BIND}vr_mock_hand off -0.3 1.0 -0.1 0 0 0;vr_mock_camera 0.55 1.0 -0.05 20 95;wait40;screenshot;${DRAW}vr_mock_camera 0.4 1.25 0.3 15 0;vr_mock_hand main 0.42 1.15 -0.25 50 0 0;wait50;screenshot;vr_mock_camera off;vr_mock_hand main 0.2 1.3 -0.3 70 0 0;wait10;vr_mock_hand_to main by 0 25 12;wait4;-grabright;vr_mock_button main grip 0;vr_magholster_style 1;wait100;${BACK}wait20;${INFO}screenshot;vr_magholster_style 2;vr_magholster_return;wait85;${INFO}screenshot;"
           EXTRA="-Clean -Out magholster_shots.png" ;;
    *) echo "unknown case $c (time walk catch save busy shots)"; exit 2 ;;
    esac
    echo "== $c"
    bash $KIT/run.sh $AGENT $EXTRA -Script "${S}toggleconsole;quit" -Filter "$F|composed"
done
