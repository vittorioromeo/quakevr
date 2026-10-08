#!/bin/bash
# gunshape_test.sh <agent> -- the guns' collision in convex pieces and the guns lying about loading (ROUND21.md, "Guns in
# convex pieces; guns lying about load"; the author's notes vrfiringrange_2026-10-08_10-28-15, 10-31-35), headless:
#   1. each held gun's body (vr_physics_shapes): its convex pieces, the most their hulls lie off the drawn gun against
#      one hull's (vr_box3d_gun_pieces 12 and 1);
#   2. the shotgun turned port up, a shell dropped from above onto its receiver with Loose Leniency 0: in with the pieces,
#      not with one hull (it rested on the hull over the port);
#   3. a gun let go of, lying as a prop (vr_reload_test 20): a round tossed into its opening goes in (the shotgun, the
#      nailgun, the thunderbolt, the super shotgun broken open, the grenade launcher), one lying sideways stays out, one
#      held at its load point goes in (the shotgun, the super nailgun, the rocket launcher), the super shotgun shut taps;
#   4. the Box3D step's time with guns lying in a heap and a held gun among them, pieces and one hull (vr_profile).
# Prints PASS/FAIL per check (and the numbers); exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
BASE="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2"

# 1. The shapes.
shapes=$(for p in 12 1; do
    for gun in 154 155 156 157 161; do
        bash $KIT/run.sh $AGENT -Script "vr_box3d_gun_pieces $p;$BASE;impulse $gun;wait3;vr_test_weaponinst 7;impulse 120;wait3;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;wait30;vr_physics_shapes none;toggleconsole;quit" -Filter "hand's weapon|convex pieces" 2>&1 | grep -v "^$\|^exit" | paste -sd' ' | sed "s/^/pieces $p: /"
    done
done 2>&1)
echo "$shapes" | cut -c1-230
check $(echo "$shapes" | grep "^pieces 12" | awk '{ok = 1; for(i = 1; i <= NF; i++) if($i == "gun" && $(i + 2) == "units") { g = $(i + 1) + 0; o = $(i + 7) + 0; if(!(g < 0.7 * o || g < 1.2)) ok = 0 } n++ } END {print (n == 5 && ok) ? 1 : 0}') "each gun in pieces: its hulls lie off the drawn gun far less than one hull's (or within 1.2 units)"
check $(echo "$shapes" | grep "^pieces 1:" | grep -c "none (one hull" | awk '{print $1 == 5 ? 1 : 0}') "Guns' Shape One Hull (vr_box3d_gun_pieces 1): one hull each"

# 2. The shotgun port up, a shell dropped from 8 units above its load point, Loose Leniency 0.
DROP="give s 30;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;vr_mock_hand off -0.15 1.0 -0.40 -70 0 180;wait10;vr_reload_test 5;impulse 125;wait2;vr_reload_test 13;impulse 125;wait60;vr_reload_test 0;impulse 125;wait2"
for p in 12 1; do
    log=$(bash $KIT/run.sh $AGENT -Script "vr_box3d_gun_pieces $p;vr_reload_contact_leniency 0;$BASE;impulse 154;wait3;vr_test_weaponinst 7;impulse 120;wait3;$DROP;vr_reload_contact_leniency 6;vr_box3d_gun_pieces 12;toggleconsole;quit" -Filter "^reload: off hand" 2>&1 | grep "^reload: off hand" | tail -1)
    [ $p = 12 ] && check $(echo "$log" | grep -q "clip 1 " && echo 1 || echo 0) "the shotgun in pieces: a shell dropped onto its receiver goes into the port with no leniency ($(echo "$log" | cut -c9-40))"
    [ $p = 1 ] && check $(echo "$log" | grep -q "clip 0 " && echo 1 || echo 0) "one hull (as before): it stays out with no leniency (it rests on the hull) ($(echo "$log" | cut -c9-40))"
done

# 3. Guns lying about.
lying() { # <gun> <mode: toss side held> [open]
    local gun=$1 mode=$2 S="$BASE;impulse $1;wait3;vr_test_weaponinst 7;impulse 120;wait3;give s 30;give n 100;give c 100;give r 20;vr_mock_hand main 0.25 1.1 -0.3 0 0 0"
    case $gun in
        154|155|158|159|160) S="$S;vr_mock_hand off -0.15 1.0 -0.40 80 0 0;wait10;vr_reload_test 5;impulse 125;wait2";;
        *) S="$S;vr_mock_hand off -0.6 1.0 -0.40 50 0 0;wait10;vr_reload_test 6;impulse 125;wait60;vr_mock_hand off -0.15 1.0 -0.40 80 0 0;wait10";;
    esac
    [ "$3" = open ] && S="$S;vr_reload_test 15;impulse 125;wait20"
    S="$S;vr_reload_test 20;impulse 125;wait120"
    case $mode in
        toss) S="$S;vr_reload_test 21;impulse 125;wait40";;
        side) S="$S;vr_reload_test 24;impulse 125;wait40";;
        held) S="$S;+grabmain;vr_mock_button main grip 1;wait10;vr_reload_test 22;impulse 125;wait5;vr_reload_test 22;impulse 125;wait3";;
    esac
    bash $KIT/run.sh $AGENT -Script "$S;vr_reload_test 23;impulse 125;toggleconsole;quit" -Filter "^reload: (.*lying about|lying)" 2>&1
}
for row in "154 toss shotgun" "156 toss nailgun" "161 toss thunderbolt" "155 toss super_shotgun open" "158 toss grenade_launcher" "154 held shotgun" "157 held super_nailgun" "160 held rocket_launcher"; do
    set -- $row; name=${3//_/ }
    log=$(lying $1 $2 $4)
    check $(echo "$log" | grep -qE "(into|seated in) the .* lying about \((a loose round by contact|from a hand)\)" && echo 1 || echo 0) "lying $name: a round $([ $2 = toss ] && echo tossed into its opening || echo held at its load point) goes in ($(echo "$log" | grep -oE "(its magazine [0-9]+ of [0-9]+|it holds [0-9]+)" | head -1))"
done
log=$(lying 154 side)
check $(! echo "$log" | grep -q "into the" && echo 1 || echo 0) "lying shotgun: a shell lying sideways at its opening stays out"
log=$(lying 155 held)
check $(echo "$log" | grep -q "can't take it" && ! echo "$log" | grep -q "into the" && echo 1 || echo 0) "lying super shotgun shut: a shell held at its breech taps, stays out"

# 4. The Box3D step with six guns lying in a heap where the off hand's gun sweeps through them.
HEAP=""
for g in 154 156 157 161 155 160; do HEAP="$HEAP;impulse $g;wait3;vr_test_weaponinst 7;impulse 120;wait3;vr_mock_hand off 0 1.0 -0.40 80 0 0;wait5;vr_reload_test 20;impulse 125;wait10"; done
SWEEP=$(awk 'BEGIN { for(i = 0; i < 120; i++) printf "vr_mock_hand off %.3f 0.35 -0.45 0 %d 0;wait1;", 0.25 * sin(i / 6), (i * 7) % 360 }')
for p in 12 1; do
    csv=$(bash $KIT/run.sh --exclusive $AGENT -Script "vr_box3d_gun_pieces $p;$BASE$HEAP;impulse 154;wait3;vr_test_weaponinst 7;impulse 120;wait60;vr_profile 1;vr_profile_interval 0;$SWEEP;vr_profile_dump;vr_profile 0;vr_box3d_gun_pieces 12;toggleconsole;quit" -Filter "^  -> " 2>&1 | grep -o "[A-Z]:.*\.csv" | tail -1)
    ms=$(grep -E ",box3d (step|reach)," "$csv" | awk -F, '{printf "%s %s ms (worst %s); ", $8, $11, $12}')
    echo "pieces $p: $ms"
done
echo "(the profile's box3d step and reach: their average and worst per frame)"
exit $fail
