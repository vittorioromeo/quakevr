#!/bin/bash
# holsteritems_test.sh <agent> [case...] -- ROUND22.md, "Holsters: items' zones, the full cue, auto-collect, handle
# spheres". e1m1, the mock hands; prints PASS/FAIL, exits 1 on a failure. Cases (all with none given):
#   item    a shells box held 4.5 units out of the right hip zone's middle, sideways (its half depth 3.6): with
#           vr_holster_zone_item_hip 1.5 the hand is at the zone (hotspot 6, "item 1") and the box is taken when let go;
#           at 1 it is not (it drops)
#   full    100 shells: the box let go at the right hip holster and at the ammo pouch: "carry: full up" (the cue) and
#           not taken
#   auto    vr_carry_autocollect 1: a shells box put into the gripping hand (vr_test_spawn_hold) is taken at once ("taken at once", drawn into the
#           hand: collect fx hotspot 242); 100 shells: the full cue, still held (the zones' "item 1"); 0: held, not taken
#   handle  the shotgun put in the right shoulder holster; the empty off hand 4 units beyond its grip as drawn (away from
#           the box's middle: out of the box): with vr_holster_zone_handle_radius 5 it is at the holster (hotspot 4), and
#           its grip takes the gun; with 0 it is not (the box alone)
AGENT=$1; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-item full auto handle}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
run() { bash $KIT/run.sh $AGENT -Script "$1;toggleconsole;quit" -Filter "$2"; }
P="map e1m1;wait60;developer 1;god;notarget;vr_reload_mode 3;vr_holster_mode 0;vr_debug_collect_fx 1;vr_holster_weapons_draw_shoulder 1"
HOLD="vr_test_spawn_dist 48;vr_test_spawn_hold 1;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait5;+grabmain;vr_mock_button main grip 1;wait2;vr_test_spawn 101;impulse 241;wait10"
REL="vr_mock_button main grip 0;-grabmain;wait20"
F="^zone hand main:|carry:|You got|collect fx: maps|rror"
for c in $CASES; do
    case $c in
    item)
        for s in 1.5 1; do
            AT="vr_mock_hand_to main zonebox 3 4.5 0 0;wait3;vr_mock_hand_to main zonebox 3 4.5 0 0;wait3"
            log=$(run "$P;vr_holster_zone_item_hip $s;$HOLD;$AT;vr_holster_zones_report;$REL" "$F")
            hs=$(echo "$log" | grep "^zone hand main:" | head -1 | sed 's/.*hotspot \([0-9]*\).*item \([01]\).*/\1 item \2/')
            got=$(echo "$log" | grep -c "You got the shells")
            if [ $s = 1.5 ]; then want="6 item 1:1"; else want="item 1:0"; fi
            ok=0; [ "$got" = "${want#*:}" ] && echo "$hs" | grep -q "${want%:*}" && ok=1
            [ $s = 1 ] && echo "$hs" | grep -q "^6 " && ok=0
            check $ok "item: x$s, the box 4.5 units out of the hip zone's middle: hotspot $hs, taken $got (want ${want#*:})"
        done ;;
    full)
        for at in "holster 3" "ammopouch"; do
            AT="vr_mock_hand_to main $at;wait3;vr_mock_hand_to main $at;wait3"
            log=$(run "$P;vr_weapon_grip_mode 1;give s 100;wait3;$HOLD;$AT;$REL" "$F")
            ok=0; echo "$log" | grep -q "carry: full up" && ! echo "$log" | grep -q "You got the shells" && ok=1
            check $ok "full: 100 shells, let go at $at: the full cue, not taken"
        done ;;
    auto)
        FLOOR="$HOLD;wait5;vr_holster_zones_report" # (put into the gripping hand: VR_Carry_Start, as a grip or a force grab's catch)
        log=$(run "$P;vr_carry_autocollect 1;$FLOOR;$REL" "$F")
        ok=0; echo "$log" | grep -q "carry: taken at once" && echo "$log" | grep -q "into hotspot 242" &&
            [ "$(echo "$log" | grep -c "You got the shells")" = 1 ] && ok=1
        check $ok "auto: put into the gripping hand, taken at once, drawn into the hand"
        log=$(run "$P;vr_carry_autocollect 1;give s 100;wait3;$FLOOR;wait5;vr_holster_zones_report" "$F")
        ok=0; echo "$log" | grep -q "carry: full up" && ! echo "$log" | grep -q "You got the shells" &&
            echo "$log" | grep "^zone hand main:" | tail -1 | grep -q "item 1" && ok=1
        check $ok "auto: 100 shells: the full cue, still held"
        log=$(run "$P;vr_carry_autocollect 0;$FLOOR" "$F")
        ok=0; ! echo "$log" | grep -q "You got the shells" && echo "$log" | grep "^zone hand main:" | tail -1 | grep -q "item 1" && ok=1
        check $ok "auto: off: held, not taken"
        ;;
    handle)
        DRAW="vr_mock_hand_to main zone 3;wait5;vr_mock_hand_to main zone 3;wait5;+grabright;vr_mock_button main grip 1;wait10;vr_mock_hand main 0.3 1.0 -0.2 -40 0 0;wait3"
        PUT="vr_mock_hand_to main zonegrip 1 0 0 0;wait3;vr_mock_hand_to main zonegrip 1 0 0 0;wait3;-grabright;vr_mock_button main grip 0;wait30;vr_mock_hand main 0.3 1.0 -0.2 0 0 0;wait5"
        for r in 5 0; do
            AT="vr_mock_hand_to off zonehandle 1 4;wait3;vr_mock_hand_to off zonehandle 1 4;wait3"
            TAKE="+graboff;vr_mock_button off grip 1;wait15;vr_holster_zones_report"
            log=$(run "$P;vr_holster_zone_handle_radius $r;$DRAW;$PUT;vr_holster_zones_report;$AT;vr_holster_zones_report;$TAKE" "^zone (rshoulder|hand off)|rror")
            hs=$(echo "$log" | grep "^zone hand off:" | sed -n 2p | sed 's/.*hotspot \([0-9]*\).*/\1/')
            depth=$(echo "$log" | grep "^zone hand off depth" | sed -n 2p | sed 's/.*rshoulder \([0-9.]*\).*/\1/')
            took=0; echo "$log" | grep "^zone rshoulder" | tail -1 | grep -q "a gun in it" || took=1
            if [ $r = 5 ]; then ok=0; [ "$hs" = 4 ] && [ $took = 1 ] && ok=1; else ok=0; [ "$hs" != 4 ] && [ $took = 0 ] && ok=1; fi
            check $ok "handle: radius $r, the off hand 4 beyond the shoulder gun's grip (depth $depth): hotspot $hs, drawn $took"
        done ;;
    *) echo "unknown case $c (item full auto handle)"; exit 2 ;;
    esac
done
exit $fail
