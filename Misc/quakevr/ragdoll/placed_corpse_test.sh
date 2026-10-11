#!/bin/bash
# placed_corpse_test.sh <agent> [cases] -- the corpses a map places (misc_corpse: Dawn of the Machine, Dimension of the
# Machine; QC vr_mg_corpses.qc; ROUND22.md "Dead ogres standing"). Needs the owned MG3 data (vr_campaign_native mg3).
#   poses    start and map2b: every placed corpse lies (vr_corpse_poses: "0 not lying"; 1.0.1 had 7 ogres standing in each,
#            frames 116 and 126: $death5 and $bdeath1)
#   scenery  vr_corpse_placed 0: they are scenery again, still lying, no ragdolls
#   grab     map2b: walked up to, a placed ogre is a ragdoll; the off hand takes it, lifts it and lets go
#   walk     start: past all 58 of them; at most vr_ragdoll_max ragdolls, none removed (those left behind lie as placed)
#   gib      map2b: a blast on the placed ogre gibs it (one corpse fewer)
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-poses scenery grab walk gib}
PRE="vr_campaign_native mg3;wait5"
GO="wait30;god;notarget;developer 1;vr_knockdown_debug 1"
FILTER="vr_corpse_poses|NOT LYING|^vr_ragdoll_list|^ragdoll [0-9]+ m|held by|lies as placed again|a dead .* goes|limb taken|rror|CRASH"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "${2:-$FILTER}" -Timeout 400 2>&1 | grep -v "^$" | cut -c1-180; }
# (map2b's placed ogre: -12 280 24, turned 150; the player 40 units east of it.)
NEAR="setpos 28 280 48 0 180 0;wait30"
TAKE="vr_mock_hand_to off ragdoll near 1;wait2;vr_mock_hand_to off ragdoll near 1;+grableft;vr_mock_button off grip 1"
# (start's 58 placed corpses, 40 units beside each in turn: Misc/quakevr placed corpse origins, maps/start.bsp.)
WALK="setpos -616 672 48;wait15;setpos -648 760 48;wait15;setpos -1360 304 -16;wait15;setpos -1320 360 -16;wait15;setpos -1152 -88 -16;wait15;setpos -1064 -48 -16;wait15;setpos 712 -360 432;wait15;setpos 904 -352 432;wait15;setpos 976 -312 432;wait15;setpos 840 -144 432;wait15;setpos 864 208 496;wait15;setpos 808 144 496;wait15;setpos 664 416 112;wait15;setpos 560 712 112;wait15;setpos 640 736 112;wait15;setpos 712 680 112;wait15;setpos 840 720 496;wait15;setpos 840 544 496;wait15;setpos 600 928 112;wait15;setpos -864 -184 432;wait15;setpos -832 -344 432;wait15;setpos -832 728 496;wait15;setpos -784 640 496;wait15;setpos -864 280 496;wait15;setpos -784 384 496;wait15;setpos 864 728 112;wait15;setpos 728 992 496;wait15;setpos 440 1504 112;wait15;setpos -440 1472 112;wait15;setpos -456 1472 496;wait15;setpos 440 1488 496;wait15;setpos -648 960 496;wait15;setpos -400 1680 496;wait15;setpos -384 2168 304;wait15;setpos -352 2256 304;wait15;setpos 8 2376 240;wait15;setpos -40 2160 240;wait15;setpos -72 2272 240;wait15;setpos 40 2520 240;wait15;setpos 120 2296 240;wait15;setpos 16 1872 176;wait15;setpos 344 2008 112;wait15;setpos 416 1672 496;wait15;setpos 384 2120 304;wait15;setpos -424 1952 112;wait15;setpos -360 1768 112;wait15;setpos -336 1920 112;wait15;setpos -312 1856 112;wait15;setpos 248 1856 112;wait15;setpos 400 1760 112;wait15;setpos 456 1832 112;wait15;setpos 360 1704 112;wait15;setpos -768 968 496;wait15;setpos 840 -600 -144;wait15;setpos 912 -616 -144;wait15;setpos 864 432 496;wait15;setpos 1016 688 112;wait15;setpos -856 72 496;wait15"
for c in $CASES; do
    echo "== $c"
    case $c in
    poses)
        run "$PRE;map start;$GO;wait60;vr_corpse_poses 1;map map2b;$GO;wait60;vr_corpse_poses 1;toggleconsole;quit" ;;
    scenery)
        run "$PRE;vr_corpse_placed 0;map map2b;$GO;wait60;vr_corpse_poses 1;vr_ragdoll_list;toggleconsole;quit" ;;
    grab)
        run "$PRE;map map2b;$GO;$NEAR;vr_ragdoll_list;$TAKE;wait10;vr_ragdoll_list 1;vr_mock_hand_to off by 0 0 30;wait40;vr_ragdoll_list 1;-grableft;vr_mock_button off grip 0;wait200;vr_ragdoll_list;toggleconsole;quit" \
            | grep -v -E "^  [ 0-9][0-9] [a-z]" ;;
    walk)
        run "$PRE;map start;$GO;noclip;$WALK;wait60;vr_ragdoll_list;vr_corpse_poses 1;toggleconsole;quit" "vr_corpse_poses|^vr_ragdoll_list|lies as placed again|a dead .* goes|rror|CRASH" \
            | sort | uniq -c ;;
    gib)
        run "$PRE;map map2b;$GO;$NEAR;vr_corpse_poses 1;vr_physics_blast -12 280 40 300;wait30;vr_corpse_poses 1;toggleconsole;quit" "vr_corpse_poses|gibbed|rror|CRASH" ;;
    esac
done
