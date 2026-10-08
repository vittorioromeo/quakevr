#!/bin/bash
# parry_pose_test.sh <agent> [frames] [kinds...] -- parried monsters are never drawn squashed (ROUND21.md, "Parried monsters
# drawn squashed"). Each kind (vr_test_spawn: 1 ogre, 5 knight, 6 hell knight, 7 dog, 9 fiend, 15 phantom swordsman,
# 17 overlord, 33 death knight) is put ahead of you on vrcalibration and attacks you for real (god, the stealth meter off,
# the crowbar held across: every blow parried, vr_parry_stamina 0) while vr_debug_pose_check 1 watches how it is drawn.
# Prints per kind the staggers and the "posecheck:" findings; exits 1 on a FAIL: a body held squashed between two poses
# over 0.2 s, a pose out of its model's range or a broken blend, or no parry at all for a kind that swings.
# (Before the fix: an ogre 20 squashes of about 0.5 s in 24 parries, an overlord 7, a hell knight 4, a death knight 4.)
AGENT=${1:?agent}; FRAMES=${2:-2400}; shift 2 2>/dev/null
KINDS=${*:-1 5 6 7 9 15 17 33}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
W=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$W/scratch"
fail=0
for k in $KINDS; do
    S="developer 1;vr_fixed_frames 1;vr_fixed_frames_rate 90;map vrcalibration;wait60;god;vr_parry_stamina 0;vr_stealth_meter 0"
    S="$S;vr_weapon_grip_mode 1;impulse 167;wait10;vr_mock_hand main 0.15 1.35 -0.35 0 90 0;vr_debug_pose_check 1"
    S="$S;vr_test_spawn $k;impulse 241;wait$FRAMES;toggleconsole;quit"
    bash $KIT/run.sh $AGENT -Script "$S" -Filter "parry interrupt|posecheck|rror|ENGINE" -Timeout 900 > "$W/scratch/parrypose_$k.log" 2>&1 &
done
wait
for k in $KINDS; do
    log="$W/scratch/parrypose_$k.log"
    staggers=$(grep -c "staggered" "$log")
    long=$(grep -c "too long" "$log")
    broken=$(grep -cE "out of range|blend not in|unevenly" "$log")
    verdict=ok
    if [ "$long" -gt 0 ] || [ "$broken" -gt 0 ] || grep -qE "ENGINE (ERROR|CRASH)|TIMEOUT" "$log"; then verdict=FAIL; fi
    case " 1 5 6 17 33 " in *" $k "*) [ "$staggers" -eq 0 ] && verdict="FAIL (no parry)";; esac
    [ "$verdict" = ok ] || fail=1
    echo "parrypose: kind $k: $staggers parries, $long held squashed, $broken broken poses: $verdict"
    grep -h "in all" "$log" | head -3
done
exit $fail
