#!/bin/bash
# dwell_owned_test.sh <agent> -- Dwell's weapon variants owned per player per base weapon (variantowned, his decision
# 2026-11-24; vr_ad_test 231-232, dwell/dw_test.qc; vr_weaponinst.qc WeaponInst_Owned): headless on e1m1, the Crystal
# Lance and the Rotary Shotgun owned through their pickups, their records gone, then every path that makes or restores a
# lightning gun or a super shotgun checked to give the variant (the weapon key, next weapon, a plain pickup, the cheat,
# holster draws immersive and quick slot, the weapon wheel, a plain prop taken, his own drop; none left to PlayerPostThink's
# sync). Then (1) the VR mock and a level change to e1m2, (2) flat-screen play (its holsters off: the weapon keys work)
# and a save and a load (a scratch save qvr_dwowned.sav in quakevr/): both still owned, holster 0's plain super shotgun
# the Rotary, the weapon key's lightning gun the Lance. The waits: 231's verdict 4 frames on (wait10), as
# dwell_weapons_test.sh's moves. Needs no Dwell data (the variants are ours). MP: python Misc/quakevr/mp/mp_test.py
# variantowned. Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
one() { # <mode> <move> <first commands> [run.sh options...]
    local mode=$1 move=$2 first=$3; shift 3
    log=$(bash $KIT/run.sh $AGENT "$@" -Script "${first}map e1m1;wait10;god;notarget;vr_ad_test 231;wait10;$move;wait10;vr_ad_test 232" \
        -QuitOn "dwowned2: [0-9]+ passed" -Timeout 300 -Filter "dwowned|rror|load|exit=" 2>&1 | tr -d '\r')
    echo "$log" | grep -E "FAIL|load failed" | head -8
    expect_verdict "$log" dwowned "$mode: every path gives the owned variant"
    expect_verdict "$log" dwowned2 "$mode: still owned after the $move"
    check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "$mode: the run ended on the verdict"
}
one vr "changelevel e1m2" ""
one flat "save qvr_dwowned;wait5;load qvr_dwowned" "vr_flat_holsters 0;" -ExtraArgs "-flatscreenmode"
verdict
exit $fail
