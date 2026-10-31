#!/bin/bash
# offhand_recoil_test.sh <agent> -- the two-handed recoil reduction (vr_recoil_2h_*, weaponfx::twoHandRecoil) for the
# other hand gripping the held gun anywhere, not only at its two-handed hotspot (his ask, 2026-10-31; ROUND23.md,
# "Two-handed recoil for any grip on the gun"). e1m1, the main hand holding the shotgun; vr_recoil_2h_print after each
# step (the checker reads the main hand's line):
#   one       one-handed: support 0, every share 1
#   hot       the off hand gripping its hotspot 0: support 1 (help 1), the two-handed shares (< 1)
#   *rel      two frames after letting go: easing out (0 < support < 1, the shares between); *off 40 frames on: 1
#   any<p>    the off hand gripping it anywhere (a free grip, not a hotspot: help 2) at vr_mock_hand_to off held <p>
#             (of the way from its handle to its tip, cm off its line): the shotgun's top (its hotspot takes most of
#             the rest of it); then the thunderbolt's top by its muzzle, 12 units out (anylg*): the hotspot's shares
#   touch     the off hand on its body, not gripping: one-handed
#   shell*    the off hand holding a shell from the ammo pouch, its grip closed, on the body and on the hotspot: one-handed
# Prints "offhandrecoil: N passed, M failed" (the checker); the run quits itself after its last step.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
WT=C:/OHWorkspace/qvr-agents/$AGENT; T=$WT/scratch; mkdir -p "$T"
P() { echo "echo recoil2h step $1;vr_recoil_2h_print"; }
REST="vr_mock_hand off -0.25 1.0 -0.3 70 0 0;wait10"
GRIP="+graboff;vr_mock_button off grip 1;wait30"
# Grip at "$2" (vr_mock_hand_to off ...), print as $1, let go, print easing out and off.
STEP() { echo "$REST;vr_mock_hand_to off $2;wait5;vr_mock_hand_to off $2;wait5;$GRIP;$(P "$1");vr_mock_button off grip 0;-graboff;wait2;$(P "$1rel");wait40;$(P "$1off")"; }
S="map e1m1;wait30;god;notarget;developer 1;vr_debug_2h_grip 1;vr_reload_debug 1;vr_weapon_grip_mode 1;give s 30;impulse 9;wait2;impulse 154;wait3"
S="$S;vr_mock_hand main 0.2 1.25 -0.35 70 0 0;$REST;wait20;$(P one)"
S="$S;$(STEP hot "heldspot 0")"
for p in "0.4 3" "0.45 3" "0.5 3" "0.4 4"; do S="$S;$(STEP "any${p// /_}" "held $p")"; done
S="$S;impulse 161;wait30"
S="$S;$(STEP anylg0.9_3 "held 0.9 3")"
S="$S;impulse 154;wait30;$REST;vr_mock_hand_to off held 0.45;wait5;vr_mock_hand_to off held 0.45;wait30;$(P touch)"
S="$S;vr_mock_hand_to off ammopouch;wait5;vr_mock_hand_to off ammopouch;wait5;$GRIP;$REST"
S="$S;vr_mock_hand_to off held 0.45 3;wait5;vr_mock_hand_to off held 0.45 3;wait30;$(P shellbody)"
S="$S;vr_mock_hand_to off heldspot 0;wait5;vr_mock_hand_to off heldspot 0;wait30;$(P shellhot)"
S="$S;echo recoil2h done;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "^recoil2h|^2h grip|^vr_mock_hand_to|^reload|rror" -QuitOn "^recoil2h done" -Timeout 300 > "$T/offhandrecoil.log" 2>&1
python "$WT/Misc/quakevr/twohand/offhand_recoil_check.py" "$T/offhandrecoil.log"
