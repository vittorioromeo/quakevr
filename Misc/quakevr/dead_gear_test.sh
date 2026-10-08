#!/bin/bash
# dead_gear_test.sh <agent> -- dead, the gear worn on the body hidden (vr_dead_hide_gear 1; the author's notes
# vrfiringrange_2026-10-08_00-02-05 and the follow-up: the belt flashlight and the back grenade pouch too), read from
# vr_gear_status on e1m1, headless (impulse 195: Die Now; a scratch save qvr_deadgear.sav in quakevr/):
#   1. alive, the torch in the left hand: holstered guns, sleeves, ammo pouch, grenade pouch, flashlight and its cord,
#      wrist gadget and straps, body and pauldrons all drawn; the HUD not the status bar on a hand
#   2. dead: none of them drawn (the guns' parts, the pouch's grenades neither); the status bar on a hand
#   3. respawned (restart), 4. dead again, 5. loaded: all back
#   6. vr_dead_hide_gear 0, dead: the holsters, the ammo pouch and the grenade pouch drawn (the flashlight hides anyway
#      with the body: it lies on the floor)
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
S="vr_reload_mode 3;vr_handgrenade 1;vr_flashlight 1;vr_flashlight_cord 1;vr_dead_hide_gear 1;map e1m1;wait60;impulse 9;wait10"
S="$S;save qvr_deadgear;vr_flashlight_give left;wait20;echo STEP alive;vr_gear_status;impulse 195;wait60;echo STEP dead;vr_gear_status"
S="$S;wait150;restart;wait90;echo STEP respawned;vr_gear_status;impulse 195;wait60;echo STEP dead2;vr_gear_status"
S="$S;load qvr_deadgear;wait60;echo STEP loaded;vr_gear_status;vr_dead_hide_gear 0;impulse 195;wait60;echo STEP dead_settingoff"
S="$S;vr_gear_status;vr_dead_hide_gear 1;toggleconsole;quit"
log=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "STEP|gear:|rror" 2>&1 | tr -d '\r' | tr '\n' ' ' | sed 's/STEP/\nSTEP/g')
step() { echo "$log" | grep "^STEP $1 " | head -1 | tr -s ' '; }
field() { step "$1" | grep -oE "$2 +-?[0-9]+" | head -1 | grep -oE -- "-?[0-9]+$"; }
# The worn gear's counts, drawn (all over 0) or not (all 0).
drawn() { for k in "holstered guns" "holster sleeves" "ammo pouch" "grenade pouch" "flashlight" "wrist gadget" "straps" "body"; do
    [ "$(field "$1" "$k")" -gt 0 ] 2>/dev/null || { echo 0; return; }; done; echo 1; }
hidden() { for k in "holstered guns" "their parts" "holster sleeves" "ammo pouch" "grenades" "grenade pouch" "flashlight" "cord" \
    "wrist gadget" "drawn" "straps" "body" "pauldrons"; do [ "$(field "$1" "$k")" = "0" ] || { echo 0; return; }; done; echo 1; }
for s in alive dead respawned dead2 loaded dead_settingoff; do echo "  $s: $(step $s | sed -E 's/^STEP [a-z0-9_]+ +//' | cut -c1-400)"; done
check $(drawn alive) "alive: the gear drawn"
check $([ "$(field alive cord)" = "1" ] && [ "$(field alive "status bar on a hand")" = "0" ] && echo 1 || echo 0) "alive: the torch's cord out, the HUD not on a hand"
check $(hidden dead) "dead: the gear hidden (flashlight, cord, grenade pouch too)"
check $([ "$(field dead "status bar on a hand")" = "1" ] && echo 1 || echo 0) "dead: the status bar on a hand"
check $(drawn respawned) "respawned: the gear back"
check $(hidden dead2) "dead again: hidden"
check $(drawn loaded) "loaded: the gear back"
check $([ "$(field dead_settingoff "holstered guns")" -gt 0 ] && [ "$(field dead_settingoff "grenade pouch")" = "1" ] && [ "$(field dead_settingoff "ammo pouch")" = "1" ] && echo 1 || echo 0) "setting off, dead: the holsters and pouches drawn"
check $(echo "$log" | grep -q "rror" && echo 0 || echo 1) "no error"
exit $fail
