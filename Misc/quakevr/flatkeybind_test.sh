#!/bin/bash
# flatkeybind_test.sh <agent> -- the flat keys in Options > Controls (menu.c: the "Quake VR (flat screen)" group of the keys
# menu; vr_flatmove.cpp VR_FlatKeyGet/Set), flat-screen mode, headless, fast mode. One run, ending on its own
# "flatkeybind: done" line (-QuitOn):
#   link     VR menu > Locomotion > "Flat Keys (Options > Controls)" opens Controls on the group's first row (Lean left)
#   rebind   Enter on the Grab / pull row waits for a key (the Keys menu's "press a key"), a simulated key press (vr_mock_key k) sets
#            vr_flat_key_pull to "k"; Escape on a waiting row leaves it as it was
#   clash    k also bound (impulse 10): both rows name the clash and the rule ("in flat play: Grab / pull, not Next weapon")
#   fires    with the new key pressed in flat play, the flat key wins: it runs Grab / pull ("flatgrab: k down");
#            the old key (h) does nothing
#   clear    Backspace on the Swap hands row empties vr_flat_key_swap
#   reset    VR menu > Locomotion > Reset Flat Keys puts every flat key back (vr_flat_key_pull h again)
#   gear     Controls' Gear lights row (vr_flat_key_gearlights, 9, no clash); 9 pressed twice dims and lights the gear
#   numkeys  Controls' Cycle left hand row (vr_flat_key_cycle_left, 1: his 2026-11-22 holster keys) names its clash with
#            the Axe's bind; 1 pressed cycles the left hand; with vr_flat_holsters 0 it is not the flat key's (Quake's bind)
# Run 2, two keys per action (his words 2026-11-25; ends on "flatkeybind: two done"):
#   defaults the shipped keys: Next ammo type N, Throw B, toolgun adjust N, Crouch / slide "c CTRL"
#   crouch   C and Left Ctrl each crouch (vr_flatplay_status: held 1), neither key's bind runs (bound to echo for the
#            check); vr_flat_crouch_fire 1: Ctrl's bind runs too
#   second   Right then Enter then t on the Reload row adds a second key ("r t"); t reloads, r still does; Escape on a
#            waiting second slot leaves the row as it was; Backspace on the second slot clears it alone, on the first
#            clears it and the second moves up; the config written (writeconfig) keeps "r t"
# Run 3, a restart: the written config read by a fresh game keeps "r t" and t reloads; an older config (no flat key lines
# but a player's own drop key) gets the new defaults and keeps the player's key. Run 4, swimming: C held under water
# swims down (vr_flat_crouch_swim 1) further than with it 0.
# Prints "flatkeybind: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatkeybind; mkdir -p "$S"
N=19 # Locomotion (vr_menu_table.inc)
FILTER='^("vr_flat_key|"vr_gear_lights"|flatplay: |flatgrab: |flatkeybind|menu_keys)'
PRE="map e1m1; wait40; developer 1; vr_flatplay_debug 1; god; notarget; impulse 152; wait10"
LINK="menu_vr $N Flat Keys; wait3; vr_mock_key enter; wait2"
RB="menu_keys vr_flat_key_pull; wait2; vr_mock_key enter; wait2; vr_mock_key k; wait2; vr_flat_key_pull"
ESC="menu_keys vr_flat_key_reload; wait2; vr_mock_key enter; wait2; vr_mock_key escape; wait2; vr_flat_key_reload"
CL="bind k \"impulse 10\"; menu_keys vr_flat_key_pull; wait2; menu_keys \"impulse 10\"; wait2; unbind k"
FIRE="togglemenu; togglemenu; wait5; vr_mock_key k down; wait4; vr_mock_key k up; wait4; echo flatkeybind: after k; vr_mock_key h down; wait4; vr_mock_key h up; wait4; echo flatkeybind: after h"
CLR="menu_keys vr_flat_key_swap; wait2; vr_mock_key backspace; wait2; vr_flat_key_swap"
RST="menu_vr $N Reset Flat Keys; wait3; vr_mock_key enter; wait2; vr_flat_key_pull"
GEAR="menu_keys vr_flat_key_gearlights; wait2; togglemenu; togglemenu; wait5; vr_gear_lights 1; vr_mock_key 9 down; wait4; vr_mock_key 9 up; wait4; echo flatkeybind: gear1; vr_gear_lights; vr_mock_key 9 down; wait4; vr_mock_key 9 up; wait4; echo flatkeybind: gear2; vr_gear_lights"
NUM="menu_keys vr_flat_key_cycle_left; wait2; togglemenu; togglemenu; wait5; vr_mock_key 1 down; wait4; vr_mock_key 1 up; wait4; echo flatkeybind: num1; vr_flat_holsters 0; vr_mock_key 1 down; wait4; vr_mock_key 1 up; wait4; echo flatkeybind: num0; vr_flat_holsters 1"
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$PRE; $LINK; $RB; $ESC; $CL; $FIRE; $CLR; $RST; $GEAR; $NUM; echo flatkeybind: done; toggleconsole; quit" -QuitOn "^flatkeybind: done" -Timeout 150 -Filter "$FILTER" > "$S/run.log" 2>&1
L=$S/run.log
check $(grep -aq 'menu_keys: at "Lean left" (vr_flat_key_lean_left)' "$L" && echo 1 || echo 0) "link: the VR menu's Flat Keys button opens Controls on Lean left"
check $(grep -aq '"vr_flat_key_pull" is "k"' "$L" && echo 1 || echo 0) "rebind: Enter then k on Controls' Grab / pull row sets vr_flat_key_pull to k"
check $(grep -aq '"vr_flat_key_reload" is "r"' "$L" && echo 1 || echo 0) "rebind: Escape on a waiting row leaves vr_flat_key_reload as it was"
check $(grep -aq 'menu_keys: at "Grab / pull" (vr_flat_key_pull) !.*Grab / pull, not Next weapon' "$L" && echo 1 || echo 0) "clash: the flat key's row names the bind it beats"
check $(grep -aq 'menu_keys: at "Next weapon" (impulse 10) !.*Grab / pull, not Next weapon' "$L" && echo 1 || echo 0) "clash: the bind's row names the flat key that beats it"
check $(awk '/flatkeybind: after k/{exit} /flatgrab: k down/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "fires: k triggers Grab / pull"
check $(awk '/flatkeybind: after k/{a=1} a && /flatgrab: h down/{f=1} END{exit f}' "$L" && echo 1 || echo 0) "fires: the old key h does nothing"
check $(grep -aq '"vr_flat_key_swap" is ""' "$L" && echo 1 || echo 0) "clear: Backspace empties vr_flat_key_swap"
check $(awk '/is "k"/{k=1} k && /"vr_flat_key_pull" is "h"/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "reset: Reset Flat Keys puts Grab / pull back to h"
check $(grep -aq 'menu_keys: at "Gear lights" (vr_flat_key_gearlights)\s*$' "$L" && echo 1 || echo 0) "gear: Controls has the Gear lights row, its key 9 clashing with no bind"
check $(awk '/flatkeybind: gear1/{exit} /flatplay: 9: gear lights/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gear: 9 in flat play is the gear lights' key"
check $(awk '/flatkeybind: gear1/{a=1; next} /flatkeybind: gear2/{exit} a && /"vr_gear_lights" is "0"/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gear: the first press dims the gear (vr_gear_lights 0)"
check $(awk '/flatkeybind: gear2/{a=1; next} a && /"vr_gear_lights" is "1"/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gear: the second press lights it again (vr_gear_lights 1)"
check $(grep -aq 'menu_keys: at "Cycle left hand" (vr_flat_key_cycle_left) !.*Cycle left hand, not Axe' "$L" && echo 1 || echo 0) "numkeys: Controls' Cycle left hand row on 1, its clash with the Axe's bind named"
check $(awk '/flatkeybind: num1/{exit} /flatplay: 1: cycle the left hand/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "numkeys: 1 in flat play cycles the left hand"
check $(awk '/flatkeybind: num1/{a=1; next} /flatkeybind: num0/{exit} a && /flatplay: 1: cycle/{f=1} END{exit f}' "$L" && echo 1 || echo 0) "numkeys: with vr_flat_holsters 0, 1 is not the flat key's"

# Run 2: two keys per action.
F2='^("vr_flat_key|flatplay: crouch|flatkeybind|menu_keys|flatreload: |  The crouch keys)'
DEF="echo flatkeybind: defaults; vr_flat_key_ammo; vr_flat_key_throw; vr_flat_key_pull; vr_flat_key_toolgun_adjust; vr_flat_key_crouch"
CRO="bind CTRL \"echo flatkeybind: ctrl bind ran\"; bind c \"echo flatkeybind: c bind ran\"; vr_mock_key c down; wait4; vr_flatplay_status; vr_mock_key c up; wait30; vr_mock_key ctrl down; wait4; vr_flatplay_status; vr_mock_key ctrl up; wait30; echo flatkeybind: fire1; vr_flat_crouch_fire 1; vr_mock_key ctrl down; wait4; vr_mock_key ctrl up; wait30; vr_flat_crouch_fire 0; echo flatkeybind: fire0; bind CTRL +attack; bind c +movedown"
SEC="menu_keys vr_flat_key_reload; wait2; vr_mock_key rightarrow; wait2; vr_mock_key enter; wait2; vr_mock_key t; wait2; echo flatkeybind: second; vr_flat_key_reload"
TR="togglemenu; togglemenu; wait5; vr_flat_reload_debug 1; echo flatkeybind: press t; vr_mock_key t down; wait4; vr_mock_key t up; wait4; echo flatkeybind: press r; vr_mock_key r down; wait4; vr_mock_key r up; wait4; echo flatkeybind: pressed"
ESC2="menu_keys vr_flat_key_pull; wait2; vr_mock_key rightarrow; wait2; vr_mock_key enter; wait2; vr_mock_key escape; wait2; echo flatkeybind: esc2; vr_flat_key_pull"
CLR2="menu_keys vr_flat_key_reload; wait2; vr_mock_key rightarrow; wait2; vr_mock_key backspace; wait2; echo flatkeybind: clear2; vr_flat_key_reload; menu_keys vr_flat_key_crouch; wait2; vr_mock_key backspace; wait2; echo flatkeybind: clear1; vr_flat_key_crouch; vr_flat_key_crouch \"c CTRL\""
WR="menu_keys vr_flat_key_reload; wait2; vr_mock_key rightarrow; wait2; vr_mock_key enter; wait2; vr_mock_key t; wait2; togglemenu; writeconfig flatkeys2_test; echo flatkeybind: written"
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$PRE; $DEF; $CRO; $SEC; $TR; $ESC2; $CLR2; $WR; echo flatkeybind: two done; toggleconsole; quit" -QuitOn "^flatkeybind: two done" -Timeout 150 -Filter "$F2" > "$S/two.log" 2>&1
L=$S/two.log
D=$(awk '/flatkeybind: defaults/{a=1; next} a && /^"vr_flat_key/{print} /flatkeybind: fire1/{exit}' "$L" | tr '\n' ' ')
check $(echo "$D" | grep -q '"vr_flat_key_ammo" is "n"' && echo "$D" | grep -q '"vr_flat_key_throw" is "b"' && echo "$D" | grep -q '"vr_flat_key_pull" is "h"' && echo "$D" | grep -q '"vr_flat_key_toolgun_adjust" is "n"' && echo "$D" | grep -q '"vr_flat_key_crouch" is "c CTRL"' && echo 1 || echo 0) "defaults: ammo N, throw/drop B, grab/pull H, toolgun adjust N, crouch C and Left Ctrl ($D)"
H=$(awk '/flatkeybind: fire1/{exit} /^flatplay: crouch key: crouch/{print}' "$L" | grep -c .)
check $([ "$H" = 2 ] && echo 1 || echo 0) "crouch: C held crouches, Left Ctrl held crouches ($H of 2 presses crouched)"
check $(awk '/flatkeybind: fire1/{exit} /^flatkeybind: (ctrl|c) bind ran/{f=1} END{exit f}' "$L" && echo 1 || echo 0) "crouch: neither C's nor Ctrl's bind runs in flat play"
check $(awk '/flatkeybind: fire1/{a=1} /flatkeybind: fire0/{exit} a && /^flatkeybind: ctrl bind ran/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crouch: vr_flat_crouch_fire 1, Ctrl's bind runs too"
check $(grep -aq 'menu_keys: Reload key 2' "$L" && awk '/flatkeybind: second/{a=1; next} a{print; exit}' "$L" | grep -q '"vr_flat_key_reload" is "r t"' && echo 1 || echo 0) "second: Right, Enter, t on Controls' Reload row adds t as its second key (\"r t\")"
check $(awk '/flatkeybind: press t/{a=1; next} /flatkeybind: press r/{exit} a && /flatreload: r t: reload/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "second: the second key t reloads"
check $(awk '/flatkeybind: press r/{a=1; next} /flatkeybind: pressed/{exit} a && /flatreload: r t: reload/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "second: the first key r still reloads"
check $(awk '/flatkeybind: esc2/{a=1; next} a{print; exit}' "$L" | grep -q '"vr_flat_key_pull" is "h"' && echo 1 || echo 0) "second: Escape on a waiting second slot leaves Grab / pull as it was (h)"
check $(awk '/flatkeybind: clear2/{a=1; next} a{print; exit}' "$L" | grep -q '"vr_flat_key_reload" is "r"' && echo 1 || echo 0) "second: Backspace on the second slot clears it alone (\"r\")"
check $(awk '/flatkeybind: clear1/{a=1; next} a{print; exit}' "$L" | grep -q '"vr_flat_key_crouch" is "CTRL"' && echo 1 || echo 0) "second: Backspace on the first slot clears it, the second moves up (\"CTRL\")"
check $(grep -q '^vr_flat_key_reload "r t"' "$TREE/quakevr/flatkeys2_test.cfg" 2>/dev/null && echo 1 || echo 0) "second: the config written keeps both keys (vr_flat_key_reload \"r t\")"

# Run 3: a restart (a fresh game reads the written config), and an older config (with retired vr_flat_key_drop).
printf 'vr_cfg_version "224"\nvr_flat_key_drop "k"\nvr_flat_key_swap "u"\n' > "$TREE/quakevr/flatkeys2_old.cfg"
F3='^("vr_flat_key|flatkeybind|flatreload: |Unknown command)'
OLD="exec flatkeys2_old.cfg; vr_migrate_config; echo flatkeybind: old; vr_flat_key_ammo; vr_flat_key_throw; vr_flat_key_pull; vr_flat_key_crouch; vr_flat_key_swap"
RS="exec flatkeys2_test.cfg; echo flatkeybind: restart; vr_flat_key_reload; map e1m1; wait40; vr_flat_reload_debug 1; vr_mock_key t down; wait4; vr_mock_key t up; wait4"
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$OLD; $RS; echo flatkeybind: three done; toggleconsole; quit" -QuitOn "^flatkeybind: three done" -Timeout 150 -Filter "$F3" > "$S/three.log" 2>&1
L=$S/three.log
O=$(awk '/flatkeybind: old/{a=1; next} /flatkeybind: restart/{exit} a' "$L" | tr '\n' ' ')
check $(echo "$O" | grep -q '"vr_flat_key_ammo" is "n"' && echo "$O" | grep -q '"vr_flat_key_throw" is "b"' && echo "$O" | grep -q '"vr_flat_key_pull" is "h"' && echo "$O" | grep -q '"vr_flat_key_crouch" is "c CTRL"' && echo 1 || echo 0) "old config: defaults take the new keys (N, B, H, C and Left Ctrl) and retired vr_flat_key_drop is ignored"
check $(echo "$O" | grep -q '"vr_flat_key_swap" is "u"' && ! grep -aq 'Unknown command.*vr_flat_key_drop' "$L" && echo 1 || echo 0) "old config: the player's own key stays (swap u) with no unknown-command spam"
check $(awk '/flatkeybind: restart/{a=1; next} a{print; exit}' "$L" | grep -q '"vr_flat_key_reload" is "r t"' && grep -aq 'flatreload: r t: reload' "$L" && echo 1 || echo 0) "restart: the second key survives (\"r t\" read back) and t reloads"

# Run 4: C held under water swims down (vrtesthall's pool; setpos turns noclip on, the noclip after it off).
F4='^(Player pos|flatkeybind)'
SW() { echo "vr_flat_crouch_swim $1; setpos 384 -256 -40 0 90 0; noclip; wait20; echo flatkeybind: swim $1; viewpos; vr_mock_key c down; wait30; viewpos; vr_mock_key c up; wait5"; }
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "map vrtesthall; god; notarget; wait5; $(SW 1); $(SW 0); echo flatkeybind: four done; toggleconsole; quit" -QuitOn "^flatkeybind: four done" -Timeout 150 -Filter "$F4" > "$S/four.log" 2>&1
L=$S/four.log
Z=$(grep -a '^Player pos' "$L" | sed 's/^Player pos: ([^ ]* [^ ]* \([-0-9.]*\)).*/\1/' | tr '\n' ' ')
read z1a z1b z0a z0b <<<"$Z"
check $(awk -v a="$z1a" -v b="$z1b" -v c="$z0a" -v d="$z0b" 'BEGIN{exit !(a != "" && d != "" && (a - b) > (c - d) + 10)}' && echo 1 || echo 0) "swim: C held under water swims down (vr_flat_crouch_swim 1: down $z1a -> $z1b; 0: $z0a -> $z0b)"
verdict; exit $fail
