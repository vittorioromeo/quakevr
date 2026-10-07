#!/bin/bash
# contact_test.sh <agent> -- loose rounds loading by contact (QC vr_reload.qc VR_Reload_LooseFrame; the author's notes
# 2026-10-07: shells dropped into the shotgun's port and the open super shotgun's barrels, a magazine dropped into its
# well, the gun brought down onto a shell or an upright magazine lying on the floor), headless:
#   1. the collision shapes (vr_physics_shapes): each round's body against its drawn size, each gun's hull against its
#      drawn model, and how deep the gun's load point lies inside the hull (what the pass-through is for);
#   2. per gun (the off hand's): a round tossed into the opening lying right (in), one lying sideways at it (not in), a
#      round dropped from above into the opening turned up (in), the gun brought down onto a round on the floor (in);
#      the super shotgun shut refuses, open takes the pair; Load Loose Rounds off: nothing goes in.
# Test steps: vr_reload_test 10 tossed, 11 sideways at the opening, 12 on the floor, 13 dropped from above, 14 the loose rounds' report,
# 15 the super shotgun broken open (impulse 125 runs the step). Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
REP="vr_reload_test 0;impulse 125;wait2"
F="^reload: (off hand|[0-9] into|a magazine of|.*placed loose|the super shotgun open)|^  |^    "

# 1. The shapes.
log=$(for gun in 154 156 157 161; do
    G="map e1m1;wait60;developer 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse $gun;wait3;vr_test_weaponinst 7;impulse 120;wait3;give s 30;give n 100;give c 100;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
    if [ $gun = 154 ]; then GET="vr_reload_test 5;impulse 125;wait2;vr_reload_test 1;impulse 125;wait3;vr_reload_test 3;impulse 125"; else GET="vr_reload_test 6;impulse 125"; fi
    bash $KIT/run.sh $AGENT -Script "$G;$GET;wait90;vr_physics_shapes vr_ammo_shell vr_ammo_mag;toggleconsole;quit" -Filter "^  |^    " 2>&1
done)
echo "$log" | grep -E "^  [0-9]+ |body \(its hull\)|drawn  |inside the hull|outside the hull" | sort -u
check $(echo "$log" | grep -A2 "vr_ammo_shell (" | grep -q "body (its hull) 2.30 x 0.68 x 0.68" && echo 1 || echo 0) "the shell's body is its drawn size (2.30 x 0.68 x 0.68)"
check $(echo "$log" | grep -A4 "vr_ammo_mag (" | grep "Quake box" | awk '{print $3}' | awk '$1 > 4 {bad=1} END {print bad ? 0 : 1}') "the magazines' Quake boxes are their drawn size (were twice it)"

# The off hand holding gun $1 at pose $2 (pitch yaw roll), emptied (a magazine gun's magazine dropped away from it).
setup() {
    local gun=$1 pose=$2
    local S="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse $gun;wait3;vr_test_weaponinst 7;impulse 120;wait3;give s 30;give n 100;give c 100;vr_mock_hand main 0.25 1.1 -0.3 0 0 0"
    if [ $gun = 154 ] || [ $gun = 155 ]; then
        S="$S;vr_mock_hand off -0.15 1.0 -0.40 $pose;wait10;vr_reload_test 5;impulse 125;wait2"
    else
        S="$S;vr_mock_hand off -0.6 1.0 -0.40 50 0 0;wait10;vr_reload_test 6;impulse 125;wait60;vr_mock_hand off -0.15 1.0 -0.40 $pose;wait10"
    fi
    echo "$S"
}
# Lowering the off hand from 1.0 m to 0 at pose $1 (the gun brought down onto what lies under it).
lower() { awk -v pose="$1" 'BEGIN { for(i = 0; i <= 40; i++) printf "vr_mock_hand off -0.15 %.3f -0.40 %s;wait2;", 1.0 - i * 0.025, pose }'; }
# The clip and whether a magazine is in, from the last report.
last() { echo "$1" | grep "^reload: off hand" | tail -1; }
loaded() { last "$1" | grep -qE "clip [1-9][0-9]* mag 1" && echo 1 || echo 0; }

# 2. Per gun: tossed in, sideways, dropped from above (the opening turned up), the gun brought down onto one on the floor
# (the shotgun's receiver lowest, its muzzle a little down: tilted up, its stock meets the floor first).
for row in "154 shotgun 80_0_0 -70_0_180 60_0_0" "156 nailgun 80_0_0 -70_0_180 80_0_0" "157 super_nailgun 0_0_-90 0_0_90 0_0_-90" "161 thunderbolt 80_0_0 -70_0_180 80_0_0"; do
    set -- $row; gun=$1; name=${2//_/ }; toss=${3//_/ }; up=${4//_/ }; slam=${5//_/ }
    log=$(bash $KIT/run.sh $AGENT -Script "$(setup $gun "$toss");$REP;vr_reload_test 10;impulse 125;wait40;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $(loaded "$log") "$name: a round tossed into its opening lying right goes in ($(last "$log" | cut -c14-40))"
    log=$(bash $KIT/run.sh $AGENT -Script "$(setup $gun "$toss");$REP;vr_reload_test 11;impulse 125;wait40;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $(echo "$log" | grep -q "placed loose (way 1)" && [ $(loaded "$log") = 0 ] && echo 1 || echo 0) "$name: lying sideways at its opening it doesn't ($(last "$log" | cut -c14-40))"
    log=$(bash $KIT/run.sh $AGENT -Script "$(setup $gun "$up");$REP;vr_reload_test 13;impulse 125;wait60;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $(loaded "$log") "$name: dropped from above into its opening turned up ($(last "$log" | cut -c14-40))"
    log=$(bash $KIT/run.sh $AGENT -Script "$(setup $gun "$slam");$REP;vr_reload_test 12;impulse 125;wait90;vr_reload_test 14;impulse 125;$(lower "$slam")wait10;$REP;toggleconsole;quit" -Filter "$F|^reload: loose" 2>&1)
    check $(loaded "$log") "$name: brought down onto one on the floor ($(echo "$log" | grep -m1 "^reload: loose" | cut -c9-90); $(last "$log" | cut -c14-40))"
done

# The super shotgun: shut, a pair tossed at its breech is refused; broken open, tossed and dropped pairs go in (2 each).
SSG="$(setup 155 "80 0 0")"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$REP;vr_reload_test 10;impulse 125;wait40;$REP;vr_reload_test 15;impulse 125;wait20;$REP;vr_reload_test 10;impulse 125;wait40;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
reps=$(echo "$log" | grep "^reload: off hand")
# (Each test step prints the report too: the third is after the shut gun's toss.)
check $(echo "$reps" | sed -n 3p | grep -q "clip 0 " && echo 1 || echo 0) "super shotgun shut: a pair tossed at its breech stays out"
check $(echo "$reps" | tail -1 | grep -q "clip 2 " && echo 1 || echo 0) "super shotgun open: a pair tossed into its barrels goes in (2)"
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 155 "0 0 0");vr_reload_test 15;impulse 125;wait20;$REP;vr_reload_test 13;impulse 125;wait60;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(last "$log" | grep -q "clip 2 " && echo 1 || echo 0) "super shotgun open, muzzle down: a pair dropped into its barrels goes in ($(last "$log" | cut -c14-40))"

# Load Loose Rounds off: nothing goes in.
log=$(bash $KIT/run.sh $AGENT -Script "vr_reload_contact 0;$(setup 154 "80 0 0");$REP;vr_reload_test 10;impulse 125;wait40;$REP;vr_reload_contact 1;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "placed loose (way 0)" && [ $(loaded "$log") = 0 ] && echo 1 || echo 0) "Load Loose Rounds off: a tossed shell stays out"
exit $fail
