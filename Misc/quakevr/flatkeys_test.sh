#!/bin/bash
# flatkeys_test.sh <agent> -- flatscreen play's keys (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Keys"), headless.
# Every case runs twice over where it matters: once with -flatscreenmode, once without, because the keys belong to the
# flatscreen player alone and a headset player keeps his own ways (Left Ctrl, F, G and the box taken at once are the
# flatscreen's: the game code says so and the engine does nothing). Prints PASS/FAIL, exits 1 on a failure.
#   crouch  Left Ctrl holds and releases (+flatcrouch / -flatcrouch, the console's way): vr_flatkeys_test 1 read standing,
#           held and let go again — his eyes come down vr_flat_crouch_drop (18 units) while the key is held and go back to
#           where they were when it is released; while held he is crouched (.vr_stand under 1, vr_crouch_status's box).
#           Without the flag the same key does nothing: the game code answers "flat play off".
#   torch   F switches the flashlight (the engine's vr_flashlight_toggle): worn on the head and lit at the start, put out
#           by F (the packed state read back: bit 0 clear), lit again by F.
#   drop    G (impulse 47, weapons.qc, VR_Forcegrab_FlatDrop): the axe in the main hand leaves the hand the way the game
#           drops a weapon ("flat drop:", then the hand weaponless). Without the flag the impulse is refused the same way.
#   collect a box grabbed with E (impulse 46) is collected as soon as it is grabbed — vr_carry_autocollect 0 and the box
#           ahead, taken at once ("carry: taken at once", "You got the shells"). In the headset, with the same setting off,
#           a grabbed box is held and not taken (vr_carry_autocollect stays the headset player's own setting).
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
FLAT=-ExtraArgs\ -flatscreenmode
P="map e1m1; wait60; developer 1; god; notarget"
# The item the tests put ahead of him: a shells box, held where it is spawned (vr_test_spawn_hold), at 48 units.
BOX="vr_test_spawn_dist 48; vr_test_spawn_hold 1; vr_test_spawn 101; impulse 241; wait10"
# The headset's own way of taking it: the mock hand's grip (the pattern holsteritems_test.sh uses).
GRIP="vr_test_spawn_hold 1; vr_mock_hand main 0.25 1.1 -0.3 0 0 0; wait5; +grabmain; vr_mock_button main grip 1; wait2; vr_test_spawn 101; impulse 241; wait10; vr_mock_button main grip 0; -grabmain; wait20"
run() { local tag=$1 flat=$2 script=$3 filter=$4; local a=(); [ "$flat" = flat ] && a=(-ExtraArgs "-flatscreenmode")
        bash $KIT/run.sh $AGENT "${a[@]}" -Low -Script "$script; wait10; toggleconsole; quit" -Filter "$filter" > "$S/$tag.log" 2>&1; }

# 1. crouch: down while the key is held, back up when it is released.
run crouchflat flat "$P; vr_flatkeys_test 1; +flatcrouch; wait40; vr_flatkeys_test 1; -flatcrouch; wait40; vr_flatkeys_test 1" "^flatkeys:"
eyes=$(grep -a "^flatkeys: crouched" "$S/crouchflat.log" | sed 's/.*eye \([0-9.]*\).*/\1/')
n=$(echo "$eyes" | grep -c .)
top=$(echo "$eyes" | head -1); low=$(echo "$eyes" | sort -g | head -1); back=$(echo "$eyes" | tail -1)
deep=$(grep -ac "^flatkeys: crouched 1 stand 0\." "$S/crouchflat.log")
check $([ "$n" = 3 ] && awk -v a="$top" -v b="$low" -v c="$back" 'BEGIN{exit !((a-b)>=15 && c==a)}' && echo 1 || echo 0) \
    "crouch: eyes $top -> $low -> $back units, crouched while held ($deep of 3 readings), the drop at least 15 of the 18 vr_flat_crouch_drop asks for"
check $(grep -qa "torch lit 1" "$S/crouchflat.log" && echo 1 || echo 0) "crouch: the run itself is flatscreen (the torch worn and lit at the start)"
run crouchvr vr "$P; +flatcrouch; wait40; vr_flatkeys_test 1" "^flatkeys:"
check $(grep -qa "flat play off" "$S/crouchvr.log" && ! grep -qa "^flatkeys: crouched" "$S/crouchvr.log" && echo 1 || echo 0) \
    "crouch in the headset: the key does nothing (the game code: flat play off)"

# 2. the flashlight: F out, F back on.
run torchflat flat "$P; vr_flashlight_toggle; wait20; vr_flatkeys_test 1; wait10; vr_flashlight_toggle; wait20; vr_flatkeys_test 1" "^flatkeys: carry"
lit=$(grep -ao "torch lit [01]" "$S/torchflat.log" | sed 's/torch lit //')
first=$(echo "$lit" | head -1); last=$(echo "$lit" | tail -1); cnt=$(echo "$lit" | grep -c .)
check $([ "$cnt" = 2 ] && [ "$first" = 0 ] && [ "$last" = 1 ] && echo 1 || echo 0) \
    "torch: F puts it out (packed state bit 0 clear) and F lights it again ($first then $last)"
run torchvr vr "$P; vr_flashlight_toggle; wait20; vr_flashlight_status; wait10; toggleconsole; quit" "flashlight"
check $(grep -qa "flashlight" "$S/torchvr.log" && echo 1 || echo 0) "torch: the same command in the headset (the head's own torch, untouched by the flatscreen's gate)"

# 3. G drops what a hand holds, and refuses in the headset.
run dropflat flat "$P; impulse 152; wait15; vr_flatkeys_test 2; wait20; vr_flatplay_status" "^(flatkeys|flat drop):"
held=$(grep -a "^flatkeys: G sent" "$S/dropflat.log" | sed 's/.*held weapon \([0-9.]*\).*/\1/')
gone=$(grep -ac "^flatkeys: dropped $held: main hand weaponless 1" "$S/dropflat.log")
check $([ -n "$held" ] && [ "$held" != 0 ] && [ "$gone" = 1 ] && grep -qa "^flat drop:" "$S/dropflat.log" && echo 1 || echo 0) \
    "drop: G frees the main hand (it held weapon $held, then weaponless 1, with the game's own flat drop: line)"
run dropvr vr "$P; impulse 152; wait15; vr_flatkeys_test 2; wait20" "^(flatkeys|flat drop):"
check $(grep -qa "flat play off" "$S/dropvr.log" && ! grep -qa "^flat drop:" "$S/dropvr.log" && echo 1 || echo 0) \
    "drop in the headset: impulse 47 is the flatscreen's and does nothing"

# 4. a box grabbed with E is collected at once, flatscreen; a grabbed box is only held in the headset.
run boxflat flat "$P; vr_carry_autocollect 0; $BOX; impulse 46; wait25; vr_flatkeys_test 1" "^(flatkeys:|carry:|You got)"
got=$(grep -ac "You got the shells" "$S/boxflat.log"); took=$(grep -ac "carry: taken at once" "$S/boxflat.log")
check $([ "$got" = 1 ] && [ "$took" = 1 ] && echo 1 || echo 0) \
    "collect: E's grab takes the box at once with the saved setting off (taken at once $took, got $got)"
run boxvr vr "$P; vr_carry_autocollect 0; vr_test_spawn_dist 48; $GRIP; vr_holster_zones_report" "^(carry:|You got|zone hand main:)"
check $(! grep -qa "You got the shells" "$S/boxvr.log" && ! grep -qa "carry: taken at once" "$S/boxvr.log" && echo 1 || echo 0) \
    "collect in the headset: with the setting off a grabbed box is held, not taken (vr_carry_autocollect stays his setting)"

verdict; exit $fail
