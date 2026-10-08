#!/bin/bash
# magcollide_test.sh <agent> -- seated magazines are solid (vr_reload_mag_collide; ROUND21.md, "Seated magazines are
# solid"; the author's note vrfiringrange_2026-10-08_22-25-59), headless, each against the setting off:
#   1. the empty main hand's palm brought into the side of the off hand's nailgun's magazine (vr_mock_hand_to magpalm):
#      drawn stopped at its surface (vr_debug_hand_collide: its "mag drawn" off it while the real hand is in it);
#   2. a hand grenade held in the main hand brought up into the nailgun's magazine from below (magheld): kept off it
#      (vr_debug_carry: "a weapon's magazine" meets first, before the gun's box);
#   3. the held nailgun's body in Box3D has its magazine as a hull of its own (vr_debug_box3d);
#   4. the nailgun, super nailgun and thunderbolt let go of, lying: their body reaches a point inside the magazine
#      (vr_reload_test 27: 0 units; off, the gun's own pieces 1.7 units or more off); the main hand gripping each one's
#      magazine still holds it, drawn where it is tracked;
#   5. the Box3D step's time: six guns in a heap, the held nailgun swept through them (vr_profile, exclusive).
# The magazine's pull, eject, knock-out and contact loading: reload_test.sh, contact_test.sh, gunshape_test.sh.
# Prints PASS/FAIL per check (and the numbers); exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
BASE="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2"
OFFGUN="wait3;vr_test_weaponinst 7;impulse 120;wait3;give n 100;give c 100;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10;vr_reload_bump_speed 100"

# 1. The empty hand into the magazine's side (out units off it; negative: into it).
STEPS=""
for o in 4 1 0 -0.5 -1; do STEPS="$STEPS;vr_mock_hand_to main magpalm 0 $o;wait3;vr_mock_hand_to main magpalm 0 $o;wait3;echo STEP $o;vr_debug_hand_collide 1;wait1;vr_debug_hand_collide 0"; done
for c in 1 0; do
    log=$(bash $KIT/run.sh $AGENT -Script "vr_reload_mag_collide $c;$BASE;impulse 156;$OFFGUN$STEPS;toggleconsole;quit" -Filter "^STEP|^handcollide: hand 1" 2>&1 | grep -v "^$\|^exit")
    last=$(echo "$log" | sed -n '/^STEP -1/,$p' | grep "^handcollide" | tail -1)
    echo "  magazine collide $c, 1 unit into it: ${last#handcollide: hand 1 }"
    if [ $c = 1 ]; then
        check $(echo "$last" | awk '{for(i = 1; i <= NF; i++) { if($i == "mag" && $(i + 1) == "real") r = $(i + 2); if($i == "mag" && $(i + 3) == "drawn") d = $(i + 4); if($i == "pushed") p = $(i + 1) } print (r < -0.5 && d > 0.3 && p > 1) ? 1 : 0}') "the empty hand stops at the magazine's surface (real in it, drawn off it, pushed back)"
    else
        check $(echo "$last" | awk '{for(i = 1; i <= NF; i++) if($i == "pushed") p = $(i + 1); print (p < 0.1 && $0 !~ / mag /) ? 1 : 0}') "off: the hand passes into it (not pushed back)"
    fi
done

# 2. A held grenade up into the magazine from below (along -2: 0.5 of its length under its far end).
for c in 1 0; do
    log=$(bash $KIT/run.sh $AGENT -Script "vr_reload_mag_collide $c;$BASE;impulse 156;$OFFGUN;vr_test_held_hand 1;vr_test_held_pick 10;impulse 252;+grabright;vr_mock_button main grip 1;wait20;vr_mock_hand_to main magheld -2 -1.01;wait3;vr_mock_hand_to main magheld -2 -1.01;wait3;vr_debug_carry 1;wait2;vr_debug_carry 0;toggleconsole;quit" -Filter "^held: .*meet .*deep|^test:" 2>&1)
    meetLine=$(echo "$log" | grep "meet" | tail -1)
    echo "  magazine collide $c: ${meetLine:-no meet}"
    [ $c = 1 ] && check $(echo "$meetLine" | grep -q "(a weapon's magazine) meet" && echo 1 || echo 0) "a held grenade under the magazine: kept off it"
    [ $c = 0 ] && check $([ -z "$meetLine" ] && echo 1 || echo 0) "off: nothing there (the gun's box is further up)"
done

# 3. The held nailgun's body.
log=$(bash $KIT/run.sh $AGENT -Script "vr_debug_box3d 1;$BASE;impulse 156;$OFFGUN;wait10;vr_debug_box3d 0;toggleconsole;quit" -Filter "hand's weapon progs/v_nail.mdl|vr_mag_on_v_nail.mdl seated" 2>&1)
echo "$log" | grep -v "^$\|^exit" | sort -u | head -3 | sed 's/^/  /'
check $(echo "$log" | grep -q "off hand's weapon progs/v_nail.mdl: [0-9]* convex pieces and its magazine" && echo 1 || echo 0) "the held nailgun's body: its pieces and its magazine"

# 4. Lying guns.
for gun in 156 157 161; do
    for c in 1 0; do
        d=$(bash $KIT/run.sh $AGENT -Script "vr_reload_mag_collide $c;$BASE;impulse $gun;$OFFGUN;vr_reload_test 20;impulse 125;wait120;vr_reload_test 27;impulse 125;wait3;toggleconsole;quit" -Filter "a point in its magazine" 2>&1 | grep -o "body [0-9.e-]* units" | tail -1 | cut -d' ' -f2)
        [ $c = 1 ] && on=$d || off=$d
    done
    check $(awk -v a="$on" -v b="$off" 'BEGIN { print (a != "" && b != "" && a + 0 < 0.01 && b + 0 > 1) ? 1 : 0 }') "impulse $gun's gun lying: its magazine part of its body ($on units; off $off)"
done

# 4b. The main hand gripping the magazine (the two-handed hold): held, and drawn where it is tracked (not pushed off it).
for gun in 156 157 161; do
    log=$(bash $KIT/run.sh $AGENT -Script "$BASE;impulse $gun;$OFFGUN;vr_mock_hand_to main mag 0;wait3;vr_mock_hand_to main mag 0;wait10;+grabmain;vr_mock_button main grip 1;wait30;vr_debug_hand_offset 2;wait3;vr_debug_hand_offset 0;toggleconsole;quit" -Filter "^handoffset main|holds the magazine" 2>&1)
    most=$(echo "$log" | grep -o "total [0-9.]*" | awk '{if($2 > m) m = $2} END {print m + 0}')
    check $(echo "$log" | grep -q "holds the magazine of the gun in hand 0" && awk -v m="$most" 'BEGIN { print m < 0.05 ? 1 : 0 }') "impulse $gun's gun: the hand gripping its magazine holds it, drawn on it ($most units off its tracked place)"
done

# 5. The Box3D step: six guns lying in a heap, the held nailgun swept through them.
HEAP=""
for g in 154 156 157 161 155 160; do HEAP="$HEAP;impulse $g;wait3;vr_test_weaponinst 7;impulse 120;wait3;vr_mock_hand off 0 1.0 -0.40 80 0 0;wait5;vr_reload_test 20;impulse 125;wait10"; done
SWEEP=$(awk 'BEGIN { for(i = 0; i < 120; i++) printf "vr_mock_hand off %.3f 0.35 -0.45 0 %d 0;wait1;", 0.25 * sin(i / 6), (i * 7) % 360 }')
for c in 1 0; do
    csv=$(bash $KIT/run.sh --exclusive $AGENT -Script "vr_reload_mag_collide $c;$BASE;give n 100;give c 100$HEAP;impulse 156;wait3;vr_test_weaponinst 7;impulse 120;wait60;vr_profile 1;vr_profile_interval 0;$SWEEP;vr_profile_dump;vr_profile 0;toggleconsole;quit" -Filter "^  -> " 2>&1 | grep -o "[A-Z]:.*\.csv" | tail -1)
    ms=$(grep -E ",box3d (step|reach)," "$csv" | awk -F, '{printf "%s %s ms (worst %s); ", $8, $11, $12}')
    echo "  magazine collide $c: $ms"
done
echo "  (the profile's box3d step and reach: their average and worst per frame)"
exit $fail
