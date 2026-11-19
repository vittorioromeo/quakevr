#!/bin/bash
# dwell_monsters_test.sh <agent> [choice ...] -- Dwell's monsters with Quake VR's treatment (DWELL.md P2b; QC
# dwell/dw_common.qc DW_MonsterRow, the monster row AD's monsters share; the same checks as ad_monsters_test.sh, through
# vr_ad_mtest, whose frame finds Dwell's monsters too). In e1m1 with Dwell's data mounted everywhere (vr_dwell_status 1),
# headless; each monster the debug spawner's (70 Kamikaze, 71 Brute, 72 Construct, 73 the Awoken), cases by what it has:
#   spawn    one ahead: its row (vr_ad_mtest 2: registered, head zone, head gib, corpse, knockdown, rigged)
#   down     knocked down (vr_knockdown_test 0), hit there (2: still down), got up (1)           (rigged: the Brute)
#   downkill knocked down and killed there (3): its death at once, the ragdoll stays                 (the Brute)
#   decap    a sword's killing slash at its head (vr_decap_test 1): beheaded, headless ragdoll       (the Brute)
#   limbs    a fresh one per blow (vr_limb_test 4 5 6 7 9 10)                                       (the Brute)
#   corpse   killed whole (vr_ad_mtest 3), left to lie, gibbed (4)                                   (the Brute)
#   kill     killed (vr_ad_mtest 3): it bursts or breaks apart, gone       (the Kamikaze, the Construct)
# Then Misc/quakevr/dwell_monsters_check.py reads the lines: "dwell_monsters_test: N passed, M failed".
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(cd "$(dirname "$0")" && pwd)
LOG=$HERE/../../scratch/dwell_monsters_test.log
tb_body() {
MAP=${MAP:-e1m1}
CHOICES=${*:-70 71 72 73}
PRE="skill 1;map $MAP;wait60;god;notarget;developer 1;vr_decap_pop_roll 0;vr_test_spawn_dist 110;vr_knockdown_test 20;wait2"
FILTER="${FILTER:-^admtest|^limbtest: .*(on monster|no monster)|^decap|beheaded|^knockdown: (knocked|.*gets up)|rror|CRASH|SV_Error|Host_Error}"
run() { tb_run_sh -Script "$1;wait5;toggleconsole;quit" -Filter "$FILTER" -Timeout 300 2>&1 | grep -v "^$\|^exit=0"; }
for c in $CHOICES; do
    SP="vr_test_spawn $c;impulse 241;wait60"
    case $c in
    71) CASES=${CASES_71:-spawn down downkill decap limbs corpse} ;;
    70|72) CASES="spawn kill" ;;
    *) CASES="spawn" ;;
    esac
    for k in $CASES; do
        echo "== $c $k"
        case $k in
        spawn) run "$PRE;$SP;vr_ad_mtest 2" ;;
        down) run "$PRE;vr_knockdown_debug 1;$SP;vr_knockdown_test 0;wait30;vr_ad_mtest 2;vr_knockdown_test 2;wait5;vr_ad_mtest 2;vr_knockdown_test 1;wait200;vr_ad_mtest 2" ;;
        downkill) run "$PRE;$SP;vr_knockdown_test 0;wait30;vr_knockdown_test 3;wait90;vr_ad_mtest 2" ;;
        decap) run "$PRE;vr_debug_ragdoll 1;$SP;vr_decap_test 1;wait90;vr_ad_mtest 2" ;;
        limbs)
            S="$PRE;vr_test_spawn $c"; yaw=0
            for t in 4 5 6 7 9 10; do S="$S;vr_mock_look 0 $yaw;wait3;impulse 241;wait60;vr_limb_test $t;wait40"; yaw=$((yaw + 70)); done
            run "$S" ;;
        corpse) run "$PRE;$SP;vr_ad_mtest 3;wait300;vr_ad_mtest 2;wait2;vr_ad_mtest 4;wait30;vr_ad_mtest 1" ;;
        kill) run "$PRE;$SP;vr_ad_mtest 3;wait30;vr_ad_mtest 2" ;;
        esac
    done
done
}
source "$HERE/testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@" | tee "$LOG"
py -3 "$HERE/dwell_monsters_check.py" "$LOG"
