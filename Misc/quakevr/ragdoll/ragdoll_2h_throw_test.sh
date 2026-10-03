#!/bin/bash
# ragdoll_2h_throw_test.sh <agent> [stagger ...] -- a ragdoll thrown forward with both hands (ROUND21.md, "Two-handed
# ragdoll throws"): a grunt's ragdoll 28 units ahead on e1m1, a limb taken in each hand (as ragdoll_test.sh twohand),
# lifted 25 units, then both hands pushed forward and up (the main hand peaking at SPEED units a frame; the off hand
# LAG frames behind on a path OSCALE as long) and let go. STAGGER (frames the off hand lets go before the main hand):
# 0 together, 1 (11 ms), 4 (44 ms), -2 (the main hand first). Prints, per throw, what struck or pushed the ragdoll
# (corpse damage, melee blows, QC knocks), the hands' throw speeds, and how far its pelvis went forward and up 10, 30
# and 150 frames after the throw (VERBOSE=1: the whole log).
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
STAGGERS=${*:-0 1 4 -2}
SPEED=${SPEED:-3}; LAG=${LAG:-2}; OSCALE=${OSCALE:-0.7}; MON=${MON:-0}
PRE="wait30;god;notarget;vr_ragdoll 1;$XPRE;developer 1;vr_debug_ragdoll 1;vr_test_spawn $MON"
DEAD="vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0"
# The hand on the limb nearest it, 3 units over its middle (twice: the mock hand reaches from where it was). The mock
# hand jumps there in a frame, a punch: blows need vr_melee_speed 99 until both hands hold it (then MELEE, 3).
TAKE() { echo "vr_mock_hand_to $1 ragdoll near 3;wait2;vr_mock_hand_to $1 ragdoll near 3;+grab$2;vr_mock_button $1 grip 1"; }
FILTER="${FILTER:-corpse: |knocked|ragdoll: (a limb|limb)|melee|blow|struck|^vr_ragdoll_list|pelvis|rror|throw: }"
# The push's speed profile (units a frame, forward; a fifth of it up), a ramp to SPEED held 4 frames.
PROFILE=$(python -c "s=$SPEED; print(' '.join('%.3f' % (s * min(1, (i + 1) / 6.0)) for i in range(10)))")
for st in $STAGGERS; do
    S="map e1m1;$PRE;vr_test_spawn_dist 28;$DEAD;wait300;vr_melee_speed 99;$(TAKE off left);wait5;$(TAKE main right);wait10;vr_melee_speed ${MELEE:-3}"
    S="$S;vr_mock_hand_to off by 0 0 25;vr_mock_hand_to main by 0 0 25;wait30;vr_ragdoll_list;echo THROW"
    read -ra P <<< "$PROFILE"; n=${#P[@]}
    # main lets go after frame n-1; off after frame n-1-st
    offRel=$((n - 1 - st)); mainRel=$((n - 1))
    total=$(( (offRel > mainRel ? offRel : mainRel) + 1 ))
    for ((f = 0; f < total; f++)); do
        m=${P[$(( f < n ? f : n - 1 ))]}
        of=$((f - LAG)); o=0; [ $of -ge 0 ] && o=$(python -c "print('%.3f' % ($OSCALE * ${P[$(( of < n ? of : n - 1 ))]}))")
        [ $f -le $mainRel ] && S="$S;vr_mock_hand_to main by 0 $m $(python -c "print($m/5)")"
        [ $f -le $offRel ] && S="$S;vr_mock_hand_to off by 0 $o $(python -c "print($o/5)")"
        S="$S;wait1"
        [ $f -eq $offRel ] && S="$S;-grableft;vr_mock_button off grip 0"
        [ $f -eq $mainRel ] && S="$S;-grabright;vr_mock_button main grip 0"
    done
    S="$S;echo LETGO;wait10;vr_ragdoll_list;wait20;vr_ragdoll_list;wait120;vr_ragdoll_list;toggleconsole;quit"
    echo "== stagger $st (off hand $st frames first)"
    OUTP=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "$FILTER|^THROW|^LETGO" -Timeout 300 2>&1 | grep -v "^$" | grep -v -E "^  [ 0-9][0-9] [a-z]|vr_mock_hand_to|^frame|^vr_ragdoll_list")
    [ -n "$VERBOSE" ] && echo "$OUTP"
    echo "$OUTP" | grep -E "hit by|struck|knocked|let go of by|rror"
    # The pelvis (forward is +y on e1m1's start) before the push, 10 and 30 frames after the throw, and at rest.
    echo "$OUTP" | grep -o "pelvis [-0-9. ]*" | awk '{y[NR]=$3; z[NR]=$4} END {if (NR < 4) {print "  (no throw: the limbs not taken)"; exit}
        printf "  pelvis: forward %+.1f %+.1f %+.1f units (10, 30 frames, at rest), up %+.1f %+.1f\n", y[2]-y[1], y[3]-y[1], y[4]-y[1], z[2]-z[1], z[3]-z[1]}'
done
