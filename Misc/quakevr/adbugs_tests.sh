#!/bin/bash
# adbugs_tests.sh <agent> [stealth|guns|armagon|range|all] -- the 2026-10-14 playtest's AD and expansion regressions
# (ROUND22.md, "AD stealth, AD soldiers' drops, Armagon's torso, SoA relief, the firing range's laser cannons"), headless:
#   stealth  vr_stealth_test 1's scenes on an AD rocket soldier (the debug spawner's 211) on e1m1 under the AD campaign
#            (15 scenes; 7, a map's trigger, needs AD's own player: pass on AD's start map, n/a here), and the same
#            soldier 300 units ahead of him on AD's start, him dark (vr_stealth_light_force 10): Idle after 7 s
#   guns     vr_ad_test 71: each armed AD soldier's drop (AD's own single-use guns); vr_ad_test 90: the grenade launcher
#            fires our grenade (model, size, physics, fuse), clear of its muzzle
#   armagon  hipend: vr_test_monsters 5 (the hit probe) on the Armagon: its torso's heights hit (precise hits on and off)
#   range    vrfiringrange: the two laser cannons after 5 s (vr_pickup_test 5): on their pads, at rest
AGENT=${1:?worktree name}
WHAT=${2:-all}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
run() { bash $KIT/run.sh $AGENT -Script "$1;wait5;toggleconsole;quit" -Filter "$2" -Timeout 400 2>&1 | grep -v "^$\|^exit=0"; }
if [ "$WHAT" = stealth ] || [ "$WHAT" = all ]; then
    echo "== stealth (AD rocket soldier, e1m1)"
    run "skill 1;vr_campaign_native ad;wait30;map e1m1;wait60;god;developer 1;vr_test_spawn_dist 150;wait2;vr_test_spawn 211;impulse 241;wait30;vr_stealth_test 1;wait9000" "stealthtest: [a-z_ ():]+ (PASS|FAIL)|done|rror|NULL"
    echo "== stealth (dark, 300 ahead, AD's start)"
    run "skill 1;vr_campaign_native ad;wait30;map start;wait60;god;developer 1;vr_test_spawn_dist 300;wait2;vr_stealth_light_force 10;vr_test_spawn 211;impulse 241;wait420;vr_ad_mtest 2" "admtest: stealth|rror|NULL"
fi
if [ "$WHAT" = guns ] || [ "$WHAT" = all ]; then
    echo "== guns (drops by class)"
    run "skill 1;vr_campaign_native ad;wait30;map start;wait60;god;notarget;developer 1;vr_ad_test 71" "adgun drop|rror"
    echo "== guns (their shots: ours where we have the kind; clear of the muzzle)"
    run "skill 1;vr_campaign_native ad;wait30;map start;wait60;god;notarget;developer 1;vr_mock_hand main 0.25 1.20 -0.40 70 0 0;wait5;vr_ad_test 90;wait1100" "adgun shots: .*(PASS|FAIL)|adgun shots: [0-9]+ passed|rror"
fi
if [ "$WHAT" = armagon ] || [ "$WHAT" = all ]; then
    echo "== armagon (precise, then boxes)"
    run "skill 1;vr_campaign_native hipnotic;wait30;map hipend;wait60;god;notarget;setpos 923 -676 152;wait10;vr_test_monsters 5;wait5;vr_hit_precise 0;wait2;vr_test_monsters 5;wait5;vr_hit_precise 1" "hitprobe|rror"
fi
if [ "$WHAT" = range ] || [ "$WHAT" = all ]; then
    echo "== range (the laser cannons, weapon 12, pads at 152 -920 and 152 -952, z 33 over a table at 21)"
    run "skill 1;map vrfiringrange;wait5;setpos 138 -757 41;wait300;vr_pickup_test 5" "pickuptest: lying 12 |rror"
fi
