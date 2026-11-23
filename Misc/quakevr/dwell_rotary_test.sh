#!/bin/bash
# dwell_rotary_test.sh <agent> -- Dwell's Rotary Shotgun held as a gauntlet (dwellwpn3; DWELL.md 9.9): vr_dw_rotary_gauntlet,
# its model laid round the super nailgun's hand (vr_adweapons.cpp rotaryGauntletParts, weapons.txt slot 56), its shell port
# on the housing's inner side (vr_view_internal.hpp loadPorts, DwVrRotary). Needs Dwell v2.2 in the base dir
# (<base>/dwellv2p2). Headless on e1m1, vr_ad_test 218: drawn as a gauntlet, its port found by the engine, the pouch's
# handfuls held there fill its tube; with the setting 0 the super shotgun's grip (dwrotary verdict); the seven models made
# (vr_adweapon_models). Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_adweapon_models;vr_ad_test 218" \
    -QuitOn "dwrotary: [0-9]+ passed" -Timeout 300 -Filter "dwrotary|vr_adweapon_models:|progs/dw_rotary|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL|vr_adweapon_models:|progs/dw_rotary|port found" | cut -c1-220 | head -8
expect_verdict "$log" dwrotary "the Rotary as a gauntlet, its port found and its tube filled there; the grip with the setting off"
check $(grep -q "vr_adweapon_models: 7 made, 0 not made" <<<"$log" && echo 1 || echo 0) "the seven models made (the Rotary as a gauntlet)"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "the run ended on its verdict"
verdict
exit $fail
