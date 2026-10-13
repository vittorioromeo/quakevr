#!/bin/bash
# pouch_test.sh <agent> -- pouch reloading (QC vr_reload.qc VR_Reload_PouchFrame; the author's request 2026-10-13: a gun
# brought to the ammo pouch by its load point takes its rounds from it), headless, by the mock hands
# (vr_mock_hand_to <hand> pouchport [units]: the hand's own gun's load point on the top middle of the pouch's rounds):
#   1. dual-wielded super shotguns, both emptied and broken open: each filled at the pouch (2 each, the reserve 4 less),
#      the off hand's again at several angles; shut, nothing; 4 units above the rounds (15 cm), nothing, 1.5 (6 cm) fills;
#      Super Shotgun's toggle off, nothing.
#   2. the shotgun: a shell per touch (held there, still one; away and back, two); upside down (its port facing up), none.
#   3. the nailgun: its magazine in, nothing; dropped, the gun upside down, nothing; upright, a fresh magazine (the nails
#      50 less); the thunderbolt likewise; the super nailgun rolled onto its side (its well on its left).
#   4. the reserve empty: nothing (the empty pouch's knock).
#   5. holstering: the hand at a hip holster (its reach made to win over the pouch's), nothing; the holster's reach back,
#      the touch loads.
#   6. save and load: a gun filled at the pouch keeps its shells; after the load, a touch fills it again.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
REP="vr_reload_test 0;impulse 125;wait2"
F="^reload: (off hand|mode|[0-9] into|.*from the pouch|.*pouch is empty|.*magazine in: nothing|the super shotgun open)|^MARK"
run() { bash $KIT/run.sh $AGENT -Script "$1;toggleconsole;quit" -Filter "$F" 2>&1; }
# The off hand holding gun impulse $1 (150 + its id), the main hand empty, given ammo.
setup() { echo "map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse $1;wait3;vr_test_weaponinst 7;impulse 120;wait3;give s 30;give n 200;give c 200;vr_mock_hand main 0.3 1.3 -0.4 0 0 0;vr_mock_hand off -0.3 1.3 -0.40 0 0 0;wait10"; }
# The off hand away from the pouch at pose $1, then its gun's load point at the pouch's rounds ($2 units above them).
away() { echo "vr_mock_hand off -0.3 1.3 -0.40 $1;wait15"; }
to() { echo "$(away "$1");vr_mock_hand_to off pouchport ${2:-0};wait15"; }
# The off hand's clip (mag: whether a magazine is in) after marker $2 in log $1.
after() { echo "$1" | sed -n "/^MARK $2 *$/,\$p" | grep -m1 "^reload: off hand"; }
clip() { after "$1" "$2" | sed -E 's/^reload: off hand weapon [0-9]+ clip ([0-9]+) .*/\1/'; }
mag() { after "$1" "$2" | sed -E 's/.* mag ([0-9]+) .*/\1/'; }
mainclip() { after "$1" "$2" | sed -E 's/.*main hand weapon [0-9]+ clip ([0-9]+) .*/\1/'; }
shells() { echo "$1" | sed -n "/^MARK $2 *$/,\$p" | grep -m1 "^reload: mode" | sed -E 's/.* shells ([0-9]+) .*/\1/'; }
nails() { echo "$1" | sed -n "/^MARK $2 *$/,\$p" | grep -m1 "^reload: mode" | sed -E 's/.* nails ([0-9]+) .*/\1/'; }
mark() { echo "echo MARK $1;$REP"; }

# 1. Dual-wielded super shotguns.
SSG="$(setup 155);impulse 155;wait5;vr_reload_test 5;impulse 125;wait2;vr_reload_test 30;impulse 125;wait2;vr_reload_test 15;impulse 125;wait2;vr_reload_test 31;impulse 125;wait20"
S="$SSG;$(mark start);$(to "0 0 0");$(mark off);vr_mock_hand main 0.3 1.3 -0.4 0 0 0;wait15;vr_mock_hand_to main pouchport;wait15;$(mark main)"
for pose in "90 0 0" "-60 45 0" "0 90 90" "45 0 180"; do
    S="$S;vr_reload_test 5;impulse 125;wait2;$(to "$pose");$(mark "${pose// /_}")"
done
S="$S;vr_reload_test 5;impulse 125;wait2;$(to "0 0 0" 4);$(mark far);vr_mock_hand_to off pouchport 1.5;wait15;$(mark near)"
S="$S;vr_reload_pouch_sshot 0;vr_reload_test 5;impulse 125;wait2;$(to "0 0 0");$(mark toggle);vr_reload_pouch_sshot 1"
log=$(run "$S")
s0=$(shells "$log" start)
check $([ "$(clip "$log" off)" = 2 ] && [ "$(shells "$log" off)" = $((s0 - 2)) ] && echo 1 || echo 0) "super shotgun (off hand): open, its breech at the pouch fills both chambers (clip $(clip "$log" off), shells $s0 -> $(shells "$log" off))"
check $([ "$(mainclip "$log" main)" = 2 ] && [ "$(shells "$log" main)" = $((s0 - 4)) ] && echo 1 || echo 0) "super shotgun (main hand, dual-wielded): fills too (clip $(mainclip "$log" main), shells $(shells "$log" main))"
for pose in 90_0_0 -60_45_0 0_90_90 45_0_180; do
    check $([ "$(clip "$log" $pose)" = 2 ] && echo 1 || echo 0) "super shotgun at pose ${pose//_/ }: fills (clip $(clip "$log" $pose))"
done
check $([ "$(clip "$log" far)" = 0 ] && [ "$(clip "$log" near)" = 2 ] && echo 1 || echo 0) "super shotgun 4 units above the rounds: nothing ($(clip "$log" far)); 1.5 units: fills ($(clip "$log" near))"
check $([ "$(clip "$log" toggle)" = 0 ] && echo 1 || echo 0) "super shotgun's toggle off: nothing (clip $(clip "$log" toggle))"
log=$(run "$(setup 155);vr_reload_test 5;impulse 125;wait2;$(mark start);$(to "0 0 0");$(mark shut)")
check $([ "$(clip "$log" shut)" = 0 ] && [ "$(shells "$log" shut)" = "$(shells "$log" start)" ] && echo 1 || echo 0) "super shotgun shut: nothing (clip $(clip "$log" shut), shells $(shells "$log" start) -> $(shells "$log" shut))"

# 2. The shotgun: a shell per touch.
log=$(run "$(setup 154);vr_reload_test 5;impulse 125;wait2;$(mark start);$(to "50 0 0");$(mark one);wait30;$(mark held);$(to "50 0 0");$(mark two);vr_reload_test 5;impulse 125;wait2;$(to "50 0 180");$(mark upside)")
check $([ "$(clip "$log" one)" = 1 ] && [ "$(clip "$log" held)" = 1 ] && [ "$(clip "$log" two)" = 2 ] && echo 1 || echo 0) "shotgun: a shell per touch (touched $(clip "$log" one), held there $(clip "$log" held), away and back $(clip "$log" two))"
check $([ "$(clip "$log" upside)" = 0 ] && echo 1 || echo 0) "shotgun upside down (its port facing up): nothing (clip $(clip "$log" upside))"

# 3. Magazines: the nailgun, the thunderbolt, the super nailgun.
for row in "156 nailgun 50_0_0 50_0_180" "161 thunderbolt 50_0_0 50_0_180" "157 super_nailgun 0_0_-90 0_0_90"; do
    set -- $row; gun=$1; name=${2//_/ }; right=${3//_/ }; wrong=${4//_/ }
    log=$(run "$(setup $gun);$(mark start);$(to "$right");$(mark magin);$(away "$right");vr_reload_test 6;impulse 125;wait30;$(mark out);$(to "$wrong");$(mark wrong);$(to "$right");$(mark right)")
    n0=$(nails "$log" out); n1=$(nails "$log" right)
    [ $gun = 161 ] && { n0=$(echo "$log" | sed -n "/^MARK out *$/,\$p" | grep -m1 "^reload: mode" | sed -E 's/.* cells ([0-9]+),.*/\1/'); n1=$(echo "$log" | sed -n "/^MARK right *$/,\$p" | grep -m1 "^reload: mode" | sed -E 's/.* cells ([0-9]+),.*/\1/'); }
    check $(echo "$log" | grep -q "magazine in: nothing" && [ "$(mag "$log" magin)" = 1 ] && echo 1 || echo 0) "$name: its magazine in, nothing at the pouch"
    check $([ "$(mag "$log" out)" = 0 ] && [ "$(mag "$log" wrong)" = 0 ] && echo 1 || echo 0) "$name: magazine out, the wrong way ($wrong): nothing (mag $(mag "$log" wrong))"
    check $([ "$(mag "$log" right)" = 1 ] && [ "$(clip "$log" right)" -gt 0 ] && [ $((n0 - n1)) = "$(clip "$log" right)" ] && echo 1 || echo 0) "$name: the right way ($right): a fresh magazine (clip $(clip "$log" right), reserve $n0 -> $n1)"
done

# 4. The reserve empty.
log=$(run "$(setup 156);$(away "0 0 0");vr_reload_test 6;impulse 125;wait30;give n 0;wait2;$(mark start);$(to "50 0 0");$(mark empty)")
check $([ "$(mag "$log" empty)" = 0 ] && echo "$log" | grep -q "pouch is empty" && echo 1 || echo 0) "nailgun with no nails left: nothing, the empty pouch's knock (mag $(mag "$log" empty))"

# 5. Holstering: the hand at a hip holster wins.
log=$(run "$(setup 155);vr_reload_test 5;impulse 125;wait2;vr_reload_test 15;impulse 125;wait20;vr_mock_hand_to off holster 2;wait20;$(mark hip);vr_hip_holster_thresh 40;vr_ammo_pouch_thresh 0.5;$(to "0 0 0");$(mark wins);vr_hip_holster_thresh 6.5;vr_ammo_pouch_thresh 5;wait15;$(mark back)")
check $([ "$(clip "$log" hip)" = 0 ] && echo 1 || echo 0) "super shotgun open at the left hip holster: nothing (clip $(clip "$log" hip))"
check $([ "$(clip "$log" wins)" = 0 ] && [ "$(clip "$log" back)" = 2 ] && echo 1 || echo 0) "its breech at the pouch, the hand within the hip holster's reach: nothing ($(clip "$log" wins)); the reach back: fills ($(clip "$log" back))"

# 6. Save and load.
log=$(run "$(setup 155);vr_reload_test 5;impulse 125;wait2;vr_reload_test 15;impulse 125;wait20;$(to "0 0 0");$(mark filled);save pouch_test;wait5;$(away "0 0 0");load pouch_test;wait30;$(mark loaded);vr_reload_test 5;impulse 125;wait2;$(to "0 0 0");$(mark again)")
check $([ "$(clip "$log" filled)" = 2 ] && [ "$(clip "$log" loaded)" = 2 ] && [ "$(clip "$log" again)" = 2 ] && echo 1 || echo 0) "save and load: filled $(clip "$log" filled), loaded $(clip "$log" loaded), emptied and filled again $(clip "$log" again)"

exit $fail
