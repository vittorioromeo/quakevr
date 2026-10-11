#!/bin/bash
# ad_monsters_test.sh <agent> [choice ...] -- Arcane Dimensions' monsters with Quake VR's treatment (ARCANE_DIMENSIONS.md
# section 17; QC vr_ad_monsters.qc, vr_ad_monsters_test.qc): for each AD monster (the debug spawner's choice, 200..), in
# an AD map (MAP, default start: the hub, room ahead) through the AD campaign, headless:
#   spawn    one ahead: its data (vr_ad_mtest 2: registered, head zone, head gib, corpse, knockdown, rigged)
#   down     knocked down (vr_knockdown_test 0: a ragdoll), hit there (2: still down), got up (1: standing, solid 3)
#   downkill knocked down and killed there (3): its death at once, the ragdoll stays
#   decap    a sword's killing slash at its head (vr_decap_test 1): beheaded, headless ragdoll
#   limbs    a fresh one per blow (vr_limb_test 4 5 6 7 9 10: slash, fist, shotgun, bolt, explosion, gibbed)
#   corpse   killed whole (vr_ad_mtest 3), left to lie (a corpse), gibbed (4): AD_MT_Report counts it gone
# CASES picks some (default all). Lines: admtest/limbtest/decap/knockdown, errors.
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
MAP=${MAP:-start}
CHOICES=${*:-200}
CASES=${CASES:-spawn down downkill decap limbs corpse}
PRE="skill 1;vr_campaign_native ad;wait30;map $MAP;wait60;god;notarget;developer 1;vr_decap_pop_roll 0;vr_test_spawn_dist 90;vr_knockdown_test 20;wait2"
FILTER="${FILTER:-^admtest|^limbtest: .*(on monster|no monster)|^decap|beheaded|^knockdown: (knocked|.*gets up)|rror|CRASH|SV_Error|Host_Error}"
run() { bash $KIT/run.sh $AGENT -Script "$1;wait5;toggleconsole;quit" -Filter "$FILTER" -Timeout 300 2>&1 | grep -v "^$\|^exit=0"; }
for c in $CHOICES; do
    SP="vr_test_spawn $c;impulse 241;wait60"
    for k in $CASES; do
        echo "== $c $k"
        case $k in
        spawn) run "$PRE;$SP;vr_ad_mtest 2" ;;
        down) run "$PRE;vr_knockdown_debug 1;$SP;vr_knockdown_test 0;wait30;vr_ad_mtest 2;vr_knockdown_test 2;wait5;vr_ad_mtest 2;vr_knockdown_test 1;wait200;vr_ad_mtest 2" ;;
        downkill) run "$PRE;$SP;vr_knockdown_test 0;wait30;vr_knockdown_test 3;wait90;vr_ad_mtest 2" ;;
        decap) run "$PRE;vr_debug_ragdoll 1;$SP;vr_decap_test 1;wait90;vr_ad_mtest 2" ;;
        limbs)
            S="$PRE;vr_test_spawn $c"; yaw=0
            for t in 4 5 6 7 9 10; do S="$S;vr_mock_look 0 $yaw;wait3;impulse 241;wait60;vr_limb_test $t;wait40"; yaw=$((yaw + 50)); done
            run "$S" ;;
        corpse) run "$PRE;$SP;vr_ad_mtest 3;wait300;vr_ad_mtest 2;wait2;vr_ad_mtest 4;wait30;vr_ad_mtest 1" ;;
        esac
    done
done
