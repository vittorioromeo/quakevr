#!/bin/bash
# weaponwheel_test.sh <agent> [flat] [out.png] -- the weapon wheel (vr_weaponwheel, vr_flat_wheel; Quake/vr/vr_weaponwheel.cpp,
# QC vr_flatholster.qc VR_Wheel_Pick; docs/vr-port/ROUND25.md, "weaponwheel", "flatwheel"). Fast mode; the run ends on
# the test's own verdict line (-QuitOn "weaponwheel_test: N passed, M failed").
#   (headset, the mock) vr_weaponwheel_test: a nailgun in the right hand (its grip held), a shotgun in the right shoulder
#   holster; B held opens the wheel round the right hand (its press kept from the server: the nailgun's magazine stays
#   in, the same rounds once holstered and drawn; a 2-frame tap of Y later still ejects it: ROUND25 "holsterammo") (ten slots: the empty hand, six holsters, DROP, FLIP GRIP, UNLOAD) and time
#   slows to vr_weaponwheel_timescale; the hand moved onto the right shoulder's slot lights it (a haptic tick), B let go:
#   the shotgun in the right hand, the nailgun into a holster; the left hand's Y held, the hand moved off the wheel, let
#   go: nothing moves; Y again onto the nailgun's slot: the left hand (its grip not held) takes it and keeps it; the right
#   hand's EMPTY HAND: the shotgun back in its shoulder holster; the left hand's UNLOAD: the nailgun's rounds back into
#   the reserve (then greyed); its DROP: the nailgun dropped; the dagger in the right hand: FLIP GRIP lit (greyed for the
#   shotgun), the hold flips nothing, let go on it: its grip turned over; time back at 1.
#   (flat) vr_weaponwheel_test flat (-flatscreenmode, the holsters on, the shotgun in the right hand and the nailgun in the
#   left): 2 tapped cycles, 2 held opens the wheel where the right hand is, the mouse picks the shotgun's holster; 1 held,
#   off the wheel: nothing; UNLOAD (FLIP GRIP greyed), DROP, an empty holster; the dagger: FLIP GRIP turns its grip over;
#   vr_flat_wheel 0: 1 cycles on the press, no wheel.
#   (grenade / flatgrenade) vr_weaponwheel_test grenade | flatgrenade (wheelgrenade): the back pouch's grenade entry:
#   greyed with an empty pouch, whole with rockets, split GRENADE / CLUSTER with multi-rockets too; CLUSTER into the first
#   hand (flat: the right, its shotgun holstered; headset: the empty left, held without its grip), armed, thrown (the count
#   stays down); GRENADE into the other hand (its gun holstered); flat: G puts it back. One screenshot (the split entry).
# Two screenshots (the lit slot; off the wheel) composed into -Out.
AGENT=${1:?agent}
MODE=${2:-vr}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/weaponwheel
mkdir -p "$S"
if [ "$MODE" = "flat" ] || [ "$MODE" = "flatgrenade" ]; then
    OUT=${3:-$S/wheel_$MODE.png}
    SCRIPT="vr_flat_holsters 1; vr_flat_wheel 1; map vrtesthall; wait60; god; notarget; developer 1; impulse 154; wait5; impulse 176; wait10; vr_weaponwheel_test $MODE"
    EXTRA="-ExtraArgs -flatscreenmode"
elif [ "$MODE" = "grenade" ]; then
    OUT=${3:-$S/wheel_grenade.png}
    SCRIPT="map vrtesthall; god; notarget; developer 1; vr_weaponwheel_test grenade"
    EXTRA=""
else
    OUT=${3:-$S/wheel.png}
    SCRIPT="map vrtesthall; developer 1; vr_weaponwheel_test"
    EXTRA=""
fi
bash $KIT/run.sh $AGENT $EXTRA -Script "$SCRIPT" -QuitOn "^weaponwheel_test: [0-9]+ passed" -Timeout 180 \
    -Filter "^(weaponwheel_test|weapon wheel|holster give|flat holsters|flatplay|grenade|flat grenade)" -Out "$OUT" > "$S/run_$MODE.log" 2>&1
grep -E "^(weaponwheel_test|weapon wheel|grenade|flat grenade)" "$S/run_$MODE.log" | tail -45
