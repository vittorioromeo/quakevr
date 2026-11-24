#!/bin/bash
# sng_spin_test.sh <agent> -- the super nailgun's barrels spin up and coast down (his note 2026-11-23: they snapped back
# to their start angle when the trigger was let go). vr_adweapons.cpp spinFrame drives the gun's own firing frames by the
# Widowmaker's spin (vr_ad_widowmaker_spinup / _spindown). Headless on e1m1, in mock VR and flat play (-flatscreenmode):
# the gun in the main hand (impulse 157; its spin hand 1), the trigger held 60 frames, then vr_ad_widowmaker_spin_info every frame through
# the coast (100 frames). Checks: turning while held; after the release the turn never jumps (no frame more than 30
# degrees from the last, as a 450 deg/s spin's 6.25 a frame; the old snap was a jump to 0) and never runs backwards; at
# rest at the end on a frame (a multiple of 45 degrees), speed 0. Verdict `sngspin: N passed, M failed`; exits 1 on a
# failure. Since weapons1123 (his note 2026-11-23: it fires at once; the Widowmaker's whirr at 70%): at full speed 10
# frames into the hold (vr_sng_spinup 0.12 s), and its motor playing at vr_sng_motor's share of Motor Volume
# (0.7 x 0.4 = 0.28) while held.
AGENT=$1
QT_NAME=sngspin
source "$(dirname "$0")/qvrtest.sh"
SPIN="map e1m1;wait10;god;notarget;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 157;wait10;vr_mock_hand main 0.15 1.3 -0.35 70 0 0;wait5;+attack;wait10;vr_ad_widowmaker_spin_info;wait50;vr_ad_widowmaker_spin_info;-attack"
i=0
while [ $i -lt 100 ]; do
    SPIN="$SPIN;wait1;vr_ad_widowmaker_spin_info"
    i=$((i + 1))
done
SPIN="$SPIN;echo sngspin-test: done"
for mode in vr flat; do
    extra=""
    [ "$mode" = flat ] && extra="-flatscreenmode"
    log=$(bash $KIT/run.sh $AGENT -ExtraArgs "$extra" -Script "$SPIN" -QuitOn "sngspin-test: done" -Timeout 300 \
        -Filter "widowmaker spin hand 1: turn|  model:|  motor:|rror|exit=" 2>&1 | tr -d '\r')
    spin=$(grep -E "widowmaker spin hand 1: turn" <<<"$log")
    echo "$spin" | sed -n '1p;2p;3p;$p' | sed 's/^/  /' | cut -c1-110
    motor=$(grep -E "  motor:" <<<"$log" | sed -n 2p)
    echo "  $motor"
    check $(grep -q "the super nailgun (its own firing frames), 8 barrels" <<<"$log" && echo 1 || echo 0) "$mode: the gun's spin read from its own frames"
    check $(head -1 <<<"$spin" | grep -qE "speed 450 deg/s \(top 450\).*trigger 1" && echo 1 || echo 0) "$mode: at full speed 10 frames into the hold (it fires at once)"
    check $(sed -n 2p <<<"$spin" | grep -qE "speed [1-9][0-9]* deg/s.*trigger 1" && echo 1 || echo 0) "$mode: its barrels turn with the trigger held"
    check $(grep -qE "wanted 0\.2[6-9]" <<<"$motor" && echo 1 || echo 0) "$mode: its motor at 0.7 of the Widowmaker's volume while held ($motor)"
    # The turns after the release: each from the last (mod 360) forwards, 0..30 degrees.
    jumps=$(grep -oE "turn [0-9.]+ deg" <<<"$spin" | grep -oE "[0-9.]+" | awk 'NR > 2 { d = $1 - p; if(d < -180) d += 360; if(d < -0.01 || d > 30) n++ } { p = $1 } END { print n + 0 }')
    check $([ "$jumps" = 0 ] && echo 1 || echo 0) "$mode: the turn continuous after the release (jumps or turns back: $jumps)"
    check $(tail -1 <<<"$spin" | grep -qE "turn (0|45|90|135|180|225|270|315)\.0 deg, speed 0 deg/s" && echo 1 || echo 0) "$mode: at rest after, on a frame"
    check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "$mode: the run ended on its last line"
done
verdict; exit $fail
