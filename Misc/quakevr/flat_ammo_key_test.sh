#!/bin/bash
# flat_ammo_key_test.sh <agent>: flat-screen play's Next Ammo Type key (vr_flat_key_ammo, N; impulse 43, QC
# VR_Impulse_WeaponButton / ToggleSecondaryAmmo): the rocket launcher in the main hand, N pressed twice: its other ammo,
# then its own again; with the toolgun in hand, N (its adjust key there) switches nothing. A weapon with no other ammo
# whose grip may be flipped (flipgrip, his words 2026-11-24): the dagger in the main hand, N flips its grip (reversed),
# N again back (upright), no ammo change; the shotgun (neither): N does nothing. Ends on "flatammo: N passed, M failed".
AGENT=${1:-posebake}
QT_NAME=flatammo
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch
KEY() { echo "vr_mock_key $1 down; wait3; vr_mock_key $1 up; wait3"; }
# (the dagger: impulse 149 with id 13 and flags 3072, its kind; 154: the shotgun. Counted waits: an impulse a frame.)
DAGGER="vr_test_give_wid 13; vr_test_give_flags 3072; impulse 149; wait10; echo flatammo: dagger"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "developer 1; vr_flat_reload_debug 1; vr_scenario flat_ammo_key; wait30; $(KEY n); $(KEY n); $(KEY F7); wait10; $(KEY n); $(KEY F7); wait10; $DAGGER; $(KEY n); wait5; $(KEY n); wait5; impulse 154; wait10; echo flatammo: shotgun; $(KEY n); wait5; echo flatammo: done; toggleconsole; quit" \
  -QuitOn "^flatammo: done" -Timeout 180 -Filter "^ammo|flatreload: n|flatammo|revgrip: hand|SCNERR" > "$S/flatammo.log" 2>&1
L="$S/flatammo.log"
check "$(grep -a '^ammo:' "$L" | head -1 | grep -qi 'rocket launcher: its other ammo' && echo 1 || echo 0)" "N: the rocket launcher to its other ammo"
check "$(grep -a '^ammo:' "$L" | sed -n 2p | grep -qi 'rocket launcher: its own ammo' && echo 1 || echo 0)" "N again: its own ammo"
# (N sent twice with the rocket launcher before the dagger: the toolgun's press swallowed)
check "$(test "$(awk '/^flatammo: dagger/{exit} {print}' "$L" | grep -ac '^flatreload: n: next ammo type')" = 2 && echo 1 || echo 0)" "the toolgun in hand: N sent nothing (2 presses sent of 3)"
D=$(awk '/^flatammo: dagger/{on=1;next} /^flatammo: shotgun/{on=0} on' "$L")
check "$(grep -a 'revgrip: hand' <<<"$D" | head -1 | grep -q 'flips its grip: reversed' && echo 1 || echo 0)" "the dagger: N flips its grip (reversed)"
check "$(grep -a 'revgrip: hand' <<<"$D" | sed -n 2p | grep -q 'flips its grip: upright' && echo 1 || echo 0)" "the dagger: N again flips it back (upright)"
check "$(grep -aq '^ammo:' <<<"$D" && echo 0 || echo 1)" "the dagger: no ammo change"
G=$(awk '/^flatammo: shotgun/{on=1;next} /^flatammo: done/{on=0} on' "$L")
check "$(grep -aq 'flatreload: n: next ammo type' <<<"$G" && ! grep -aq 'revgrip: hand\|^ammo:' <<<"$G" && echo 1 || echo 0)" "the shotgun (no other ammo, no flip): N sent, nothing changed"

# Magazine weapons loaded with normal ammo and 0 alternate ammo: pressing N must eject the OLD magazine without
# transforming the attached magazine to the new ammo type first (nailgun, super nailgun, thunderbolt).
GIVE_NG="vr_test_give_wid 6; impulse 149; wait10; give n 50; impulse 55; wait60; echo flatammo: mag_ng"
GIVE_SNG="vr_test_give_wid 7; impulse 149; wait10; give n 50; impulse 55; wait60; echo flatammo: mag_sng"
GIVE_LG="vr_test_give_wid 11; impulse 149; wait10; give c 50; impulse 55; wait60; echo flatammo: mag_lg"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "map vrtesthall; god; notarget; developer 1; vr_flat_reload_debug 1; vr_reload_debug 1; vr_flat_holsters 0; wait20; $GIVE_NG; $(KEY n); wait15; $GIVE_SNG; $(KEY n); wait15; $GIVE_LG; $(KEY n); wait15; echo flatammo: mag_done; toggleconsole; quit" \
  -QuitOn "^flatammo: mag_done" -Timeout 180 -Filter "^ammo|flatmag:|flatreload: n|flatammo|reload: |SCNERR" > "$S/flatammo_mag.log" 2>&1
M="$S/flatammo_mag.log"

NG=$(awk '/^flatammo: mag_ng/{on=1;next} /^flatammo: mag_sng/{on=0} on' "$M")
check "$(grep -aq 'flatmag: hand 1 weapon progs/v_lava.mdl mag none (visible 0, nomag 1)' <<<"$NG" && ! grep -aq 'v_lava.mdl mag .*visible 1' <<<"$NG" && echo 1 || echo 0)" "nailgun: N with 0 lava nails never shows attached lava mag"
check "$(grep -aq '^ammo: hand 1, Nailgun: its other ammo' <<<"$NG" && grep -aq 'reload: a magazine of 0 out of the gun (hand 1, flat-screen play: the ammo changed)' <<<"$NG" && echo 1 || echo 0)" "nailgun: N ejects old normal magazine at switch"

SNG=$(awk '/^flatammo: mag_sng/{on=1;next} /^flatammo: mag_lg/{on=0} on' "$M")
check "$(grep -aq 'flatmag: hand 1 weapon progs/v_lava2.mdl mag none (visible 0, nomag 1)' <<<"$SNG" && ! grep -aq 'v_lava2.mdl mag .*visible 1' <<<"$SNG" && echo 1 || echo 0)" "super nailgun: N with 0 lava nails never shows attached lava mag"
check "$(grep -aq '^ammo: hand 1, Super Nailgun: its other ammo' <<<"$SNG" && grep -aq 'reload: a magazine of 0 out of the gun (hand 1, flat-screen play: the ammo changed)' <<<"$SNG" && echo 1 || echo 0)" "super nailgun: N ejects old normal magazine at switch"

LG=$(awk '/^flatammo: mag_lg/{on=1;next} /^flatammo: mag_done/{on=0} on' "$M")
check "$(grep -aq 'flatmag: hand 1 weapon progs/v_light_plasma.mdl mag none (visible 0, nomag 1)' <<<"$LG" && ! grep -aq 'v_light_plasma.mdl mag .*visible 1' <<<"$LG" && echo 1 || echo 0)" "thunderbolt: N with 0 plasma never shows attached plasma cell"
check "$(grep -aq '^ammo: hand 1, Thunderbolt: its other ammo' <<<"$LG" && grep -aq 'reload: a magazine of 0 out of the gun (hand 1, flat-screen play: the ammo changed)' <<<"$LG" && echo 1 || echo 0)" "thunderbolt: N ejects old cell at switch"

verdict; exit $fail
