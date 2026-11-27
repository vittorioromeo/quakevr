#!/bin/bash
# attach_test.sh <agent> -- attach2: weapon attachments, the bayonet on the shotgun and the super shotgun (QC vr_attach.qc,
# QC/vr_attach_test.qc; docs/vr-port/ATTACHMENTS.md). Map vrtesthall, headless, fast mode:
#   vr_embed_test 79 (VR, mock headset): snap-on, the drawn tip where the server's is, pull-off (bayonet2: the grip at the mount
#     alone leaves it on, the grip and B/Y pull it off; a stuck gun's shot barely shoves its grunt, the hand kept with a firing margin), holster, drop and pick-up, thrown and
#     stuck by its point, stabbed and fired stuck (both guns), fired in the hand (`attach: N passed, M failed`);
#   vr_embed_test 80 (flat-screen): J on/off/on, V's thrust, fired and reloaded stuck, the super shotgun broken open and shut
#     stuck (`attachflat: N passed, M failed`);
#   both: the HUD shows the stuck gun's ammo (the server's readouts in the QC test; the client's vr_flathud_report after its
#     marker), the bayonet taken off is drawn in the off hand (vr_held_report after its marker; flat: on the
#     screen): Misc/quakevr/attach_probe_check.py reads them (bayonetfix);
#   vr_embed_test 81/82 across a level change and across a save and its load (`attachpersist: N passed, M failed`);
#   flatattachanim: vr_embed_test 86 / 87 (flat): J's hand trip with the bayonet / the dagger (`attachanim:` /
#     `attachanimdagger: N passed, M failed`);
#   menu_vr attachtest: Weapon Offsets > Attachments edits the shotgun's bayonet mount (`attachmenu_test: N passed, M failed`);
#   daggerattach: vr_embed_test 84 / 85, 79 / 80 with the dagger as the bayonet (Daggers as Bayonets, vr_attach_daggers;
#     85 first checks the option off puts nothing on: `attachdagger:` / `attachdaggerflat: N passed, M failed`); 81/82 also
#     carry a shotgun with a dagger in the off hand and take the dagger off after.
# Each run ends on its verdict line (-QuitOn); the script ends `attach_test: N passed, M failed`.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; developer 1; wait5; setpos 0 300 25 0 90 0; wait5"
FILTER="^attach|2h grip: .* took|rror|^flathud: right hand|^held_report: off"
vr=$(bash $KIT/run.sh $AGENT -Timeout 300 -Script "vr_debug_2h_grip 1; $START; vr_embed_test 79" -QuitOn "^attach: [0-9]+ passed" -Filter "$FILTER")
echo "$vr"
flat=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 300 -Script "$START; vr_embed_test 80" \
    -QuitOn "^attachflat: [0-9]+ passed" -Filter "$FILTER")
echo "$flat"
lvl=$(bash $KIT/run.sh $AGENT -Timeout 200 -Script "$START; vr_embed_test 81; wait2; changelevel e1m2; wait30; vr_embed_test 82" \
    -QuitOn "^attachpersist: [0-9]+ passed" -Filter "^attachpersist|rror")
echo "$lvl"
sav=$(bash $KIT/run.sh $AGENT -Timeout 200 -Script "$START; vr_embed_test 81; wait2; save attach_test; wait2; map e1m1; wait5; load attach_test; wait10; vr_embed_test 82" \
    -QuitOn "^attachpersist: [0-9]+ passed" -Filter "^attachpersist|rror")
echo "$sav"
dvr=$(bash $KIT/run.sh $AGENT -Timeout 300 -Script "vr_debug_2h_grip 1; $START; vr_embed_test 84" -QuitOn "^attachdagger: [0-9]+ passed" -Filter "$FILTER")
echo "$dvr"
dflat=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 300 -Script "$START; vr_embed_test 85" \
    -QuitOn "^attachdaggerflat: [0-9]+ passed" -Filter "$FILTER")
echo "$dflat"
# flatattachanim: J's hand trip (vr_flatattach.cpp), the bayonet (86) and the dagger (87).
anim=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 200 -Script "$START; vr_embed_test 86" \
    -QuitOn "^attachanim: [0-9]+ passed" -Filter "^attachanim|^flatattach|rror")
echo "$anim"
danim=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 200 -Script "$START; vr_embed_test 87" \
    -QuitOn "^attachanimdagger: [0-9]+ passed" -Filter "^attachanim|^flatattach|rror")
echo "$danim"
# bayomounts: the bayonet on every other gun that takes one, and the bayonet's mount drawn (attach_guns_check.py).
guns=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 400 -Script "$START; vr_embed_test 96" \
    -QuitOn "^attachguns: [0-9]+ passed" -Filter "^attach|rror")
echo "$guns" | grep -v "^attach_drawn"
menu=$(bash $KIT/run.sh $AGENT -Timeout 120 -Script "menu_vr attachtest; toggleconsole; quit" -QuitOn "^attachmenu_test: [0-9]+ passed" \
    -Filter "^attachmenu_test")
echo "$menu"
expect_verdict "$vr" attach "the bayonet on the shotguns (VR)"
expect_verdict "$flat" attachflat "the bayonet on the shotguns (flat)"
expect_verdict "$lvl" attachpersist "the bayonets kept across a level change"
expect_verdict "$sav" attachpersist "the bayonets kept across a save and its load"
expect_verdict "$dvr" attachdagger "the dagger as the shotguns' bayonet (VR)"
expect_verdict "$dflat" attachdaggerflat "the dagger as the shotguns' bayonet (flat; the option off first)"
expect_verdict "$anim" attachanim "J's hand trip with the bayonet (flat)"
expect_verdict "$danim" attachanimdagger "J's hand trip with the dagger (flat)"
expect_verdict "$menu" attachmenu_test "Weapon Offsets > Attachments edits the mount"
expect_verdict "$guns" attachguns "the bayonet on every other gun that takes one (flat)"
while read -r ok what; do check "$ok" "$what"; done < <(python "$(dirname "$0")/attach_guns_check.py" - <<< "$guns")
# bayonetfix: the HUD on a stuck gun, the bayonet taken off drawn in the hand (Misc/quakevr/attach_probe_check.py).
while read -r ok what; do check "$ok" "$what"; done < <(python "$(dirname "$0")/attach_probe_check.py" vr - <<< "$vr")
while read -r ok what; do check "$ok" "$what"; done < <(python "$(dirname "$0")/attach_probe_check.py" flat - <<< "$flat")
verdict
exit $fail
