#!/bin/bash
# adlimbs_test.sh <agent> -- the limbs of models made as they load (limbfix, ROUND26 "adknights"): AD's armed death guard
# and crossbow knight (progs/vrdguard.mdl, vrdcrossbow.mdl: vr_monstermods.cpp VR_AdArmedModelFile, no file of the
# game's) had their limbs built from a file that wasn't there, and the map load's limb prebuild precached them anyway:
# `Host_Error: Mod_LoadModel: progs/vrdcrossbow.mdl#limb6 not found` on ad_azad (and e1m1 with AD's data). Checks, in
# the AD campaign: ad_azad loads (no Host_Error), and vr_limb_models builds each model's 13 limbs (none "-: <why>").
# Needs AD in the base dir. Prints `adlimbs: N passed, M failed`; exits 1 on a failure.
AGENT=${1:?worktree name}
source "$(dirname "$0")/../qvrtest.sh"
S="vr_campaign_native ad;wait30;map ad_azad;wait30;echo ADLIMBS_LOADED;vr_limb_models progs/vrdguard.mdl;vr_limb_models progs/vrdcrossbow.mdl;echo ADLIMBS_END;toggleconsole;quit"
OUT=$(bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "Host_Error|ADLIMBS_|^vr_limb_models|^  *[0-9]+ [a-z_0-9]+ " -QuitOn "ADLIMBS_END|Host_Error" -Timeout 240 2>&1)
echo "$OUT" | grep -q "^Host_Error" && ok=0 || ok=1
check $ok "no Host_Error ($(echo "$OUT" | grep -m1 "^Host_Error"))"
echo "$OUT" | grep -q "ADLIMBS_LOADED" && ok=1 || ok=0
check $ok "ad_azad loaded"
built=$(echo "$OUT" | grep -cE "^  *[0-9]+ [a-z_0-9]+ +bones")
unmade=$(echo "$OUT" | grep -cE "^  *[0-9]+ [a-z_0-9]+ +-:")
[ "$built" = 26 ] && [ "$unmade" = 0 ] && ok=1 || ok=0
check $ok "the two models' limbs built: $built of 26, $unmade not"
verdict
exit $fail
