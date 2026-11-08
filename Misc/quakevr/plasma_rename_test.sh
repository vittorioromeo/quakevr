#!/bin/bash
# plasma_rename_test.sh <agent> -- the Thunderbolt's plasma model renamed progs/v_plasma.mdl -> progs/v_light_plasma.mdl
# (2026-11-08: the old name is Rogue's and AD's Plasma Gun's). Headless on e1m1, fast mode:
#   vr, flat   the VR mock and flat-screen play: the Thunderbolt given into the main hand (impulse 161), its button
#              pressed (impulse 43: plasma ammo); the hand's model, its weapon settings' slot 16 and barrel mask
#              (vr_barrelmask_report), its painted sights (vr_sight_check), its cell's magazine and well loaded (mcache);
#              saved (scratch saves quakevr/qvr_plasmarename_<case>[_shot].sav), the trigger held, saved again: still
#              held, and flat a plasma round spent
#   oldsave    the flat save with the old name written in (as a save made before the rename): loaded, the hand's model is
#              the new one (vr_progs.cpp renameOldModels)
# Each run quits at the end of its script. Prints "plasma_rename: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/plasma_rename
mkdir -p "$S"
Q=$TREE/quakevr
F='^weaponmodel|^currentammo|^ammocounter|^weaponflags|barrelmask slot|^sightcheck (v_|[a-z_]*: no)|: progs/(vr_mag(well)?_on_)?v_(light_)?plasma|load: an older save|plasma.*not found|Host_Error|rror|exit='
GIVE="map e1m1;wait10;god;notarget;impulse 9;wait5;impulse 161;wait10;impulse 43;wait10"
LOOK="edict 1;vr_barrelmask_report;vr_sight_check;mcache"
run() { # run <case> <extra args> <script>
    bash $KIT/run.sh $AGENT $2 -Script "$3;toggleconsole;quit" -Timeout 300 -Filter "$F" > "$S/$1.log" 2>&1
}
cases() { # the hand's checks on <log> <case>
    local L=$S/$1.log
    check $(grep -aq "exit=0" "$L" && echo 1 || echo 0) "$1: the run ended by its script"
    check $(grep -aq "^weaponmodel *progs/v_light_plasma.mdl" "$L" && echo 1 || echo 0) "$1: the main hand holds progs/v_light_plasma.mdl"
    # (the console wraps the report's long line: its parts looked for apart)
    check $(grep -aq "barrelmask slot 16 progs/v_light_plasma.mdl" "$L" && grep -aqE "marked [1-9][0-9]* mask 1" "$L" && echo 1 || echo 0) \
        "$1: its weapon settings' slot 16, its barrel mask marked"
    check $(grep -aq "^sightcheck v_light_plasma.mdl main" "$L" && echo 1 || echo 0) "$1: its painted sights read"
    check $(grep -aq ": progs/vr_mag_on_v_light_plasma.mdl$" "$L" && grep -aq ": progs/vr_magwell_on_v_light_plasma.mdl$" "$L" \
        && echo 1 || echo 0) "$1: its cell's magazine and well loaded"
    check $(! grep -aqE "Host_Error|not found|ERROR" "$L" && echo 1 || echo 0) "$1: no error"
}
# (the VR mock's hands are open: vr_weapon_grip_mode 1 keeps the gun in the hand; run.sh restores the config)
SHOT="+attack;wait30;-attack;wait5"
VRSHOT="vr_mock_button main trigger 1;wait30;vr_mock_button main trigger 0;wait5" # (the mock's trigger, not +attack)
run vr "" "vr_weapon_grip_mode 1;$GIVE;$LOOK;save qvr_plasmarename_vr;$VRSHOT;save qvr_plasmarename_vr_shot"
cases vr
run flat "-ExtraArgs -flatscreenmode" "$GIVE;$LOOK;save qvr_plasmarename_flat;$SHOT;save qvr_plasmarename_flat_shot"
cases flat
# The player's (the first edict's) field <name> in save <file>.
field() { grep -a -m1 "^\"$2\" " "$Q/$1.sav" | cut -d'"' -f4; }
for c in vr flat; do # its button: the secondary ammo; the shot: the hand's counter down (saved before and after)
    check $([ "$(field qvr_plasmarename_$c weaponflags)" = "17.000000" ] && echo 1 || echo 0) \
        "$c: the plasma ammo selected (weaponflags $(field qvr_plasmarename_$c weaponflags))"
    # (the shot is checked flat only: the mock's trigger alone fires nothing here; the effects go by the slot, checked)
    a=$(field qvr_plasmarename_$c ammocounter); b=$(field qvr_plasmarename_${c}_shot ammocounter)
    [ $c = flat ] && check $([ -n "$a" ] && [ -n "$b" ] && [ "$a" != "$b" ] && echo 1 || echo 0) "$c: plasma fired (the counter $a, then $b)"
    check $(grep -aq '"weaponmodel" "progs/v_light_plasma.mdl"' "$Q/qvr_plasmarename_${c}_shot.sav" && echo 1 || echo 0) \
        "$c: still held after the shot"
done
# A save from before the rename: the flat one with the old names written in.
sed 's#v_light_plasma#v_plasma#g' "$Q/qvr_plasmarename_flat.sav" > "$Q/qvr_plasmarename_old.sav"
check $(! grep -aq "v_light_plasma" "$Q/qvr_plasmarename_old.sav" && grep -aq '"weaponmodel" "progs/v_plasma.mdl"' "$Q/qvr_plasmarename_old.sav" \
    && echo 1 || echo 0) "oldsave: the save holds only the old name"
run oldsave "-ExtraArgs -flatscreenmode" "load qvr_plasmarename_old;wait10;$LOOK"
L=$S/oldsave.log
check $(grep -aq "load: an older save: progs/v_plasma.mdl is now progs/v_light_plasma.mdl ([1-9]" "$L" && echo 1 || echo 0) \
    "oldsave: the old name renamed on load"
check $(grep -aq "^weaponmodel *progs/v_light_plasma.mdl" "$L" && echo 1 || echo 0) "oldsave: the main hand holds progs/v_light_plasma.mdl"
check $(grep -aE "barrelmask slot 16 progs/v_light_plasma.mdl" "$L" >/dev/null && echo 1 || echo 0) "oldsave: its slot 16"
check $(! grep -aqE "Host_Error|not found|ERROR" "$L" && echo 1 || echo 0) "oldsave: no error"
verdict
exit $fail
