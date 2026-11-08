#!/bin/bash
# flattoolgun_test.sh <agent> -- flat-screen play's toolgun (ROUND24.md, "Flat tools 2"): F7 puts it in the main hand,
# mouse 3 opens its panel as a screen overlay, the desktop mouse moves the panel's cursor (the view does not turn), the
# left button clicks on it, Esc and mouse 3 close it; B held is the headset's X/A (the mouse moves and turns what the
# physgun holds). Headless, fast mode; each run ends on its own last line (-QuitOn); prints "flattoolgun: N passed, M failed".
#   panel    F7; mouse 3; Monsters > (page 2) > Armed Humanoid > Choose; Esc; the left button: an armed grunt spawned;
#            mouse 3; Weapons > Grunt Burst Rifle; mouse 3 shut; the left button: weapon_gruntgun spawned
#   physgun  a grunt spawned, the physgun takes it (the left button held); B held: the mouse up pushes it out 20 units,
#            right turns it -20 degrees, the view's angles unchanged; B let go: the mouse the view's again
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flattoolgun
mkdir -p "$S"
FILTER="^(toolgun|vr_flat_mouse_move|flatplay: F7|flattoolgun|viewangles)"
FLAT="-ExtraArgs -flatscreenmode"
BASE="map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; wait5; setpos 0 300 25 25 90 0; wait5"
KEY() { echo "vr_mock_key $1 down; wait2; vr_mock_key $1 up; wait3"; }
CLICK=$(KEY mouse1)
TO() { echo "vr_flat_mouse_move to $1; wait2; $CLICK"; }

bash $KIT/run.sh $AGENT $FLAT -Script "$BASE; $(KEY F7); wait10; vr_toolgun_tool 0; $(KEY mouse3); $(TO Monsters); $(TO '>'); $(TO 'Armed Humanoid'); $(TO Choose); vr_flat_mouse_move 30 0; screenshot; $(KEY escape); vr_flat_mouse_move 5 0; vr_toolgun_status; $CLICK; wait5; $(KEY mouse3); $(TO Weapons); $(TO '>'); vr_flat_mouse_move to Grunt Burst Rifle; wait3; screenshot; $CLICK; $(KEY mouse3); $CLICK; wait5; screenshot; echo flattoolgun: panel done; toggleconsole; quit" \
    -QuitOn "^flattoolgun: panel done" -Timeout 180 -Filter "$FILTER" -Out "$S/panel.png" > "$S/panel.log" 2>&1
L="$S/panel.log"
check $(grep -aq "^flatplay: F7: the toolgun" "$L" && echo 1 || echo 0) "panel: F7 gives the toolgun"
check $(grep -aq "^toolgun: panel open" "$L" && echo 1 || echo 0) "panel: mouse 3 opens the panel"
check $(grep -a "^vr_flat_mouse_move: 30 0" "$L" | grep -aq "taken" && echo 1 || echo 0) "panel: the mouse is the cursor's while it is open"
check $(grep -a "^vr_flat_mouse_move: 5 0" "$L" | grep -aq "the view's" && echo 1 || echo 0) "panel: Esc closed it, the mouse the view's again"
check $(grep -aiq "^toolgun: hand 1, tool spawn, panel closed" "$L" && echo 1 || echo 0) "panel: still in the game after Esc (no menu), panel closed"
check $(grep -aqE "^toolgun: spawned [0-9]+ monster_[a-z_]* " "$L" && echo 1 || echo 0) "panel: the left button spawned the armed humanoid chosen on it ($(grep -aE '^toolgun: spawned' "$L" | head -1))"
check $(grep -aqE "^toolgun: spawned [0-9]+ weapon_gruntgun " "$L" && echo 1 || echo 0) "panel: the left button spawned the Grunt Burst Rifle chosen on it"

bash $KIT/run.sh $AGENT $FLAT -Script "$BASE; $(KEY F7); wait10; vr_toolgun_tool 0; vr_toolgun_select 0; wait2; $CLICK; wait20; vr_toolgun_tool 2; wait3; vr_toolgun_status; vr_mock_key mouse1 down; wait10; vr_toolgun_status; vr_mock_key b down; wait3; vr_flat_mouse_move 0 -40; vr_flat_mouse_move 50 0; wait5; vr_toolgun_status; viewpos; vr_mock_key b up; wait3; vr_flat_mouse_move 5 0; vr_mock_key mouse1 up; wait3; echo flattoolgun: physgun done; toggleconsole; quit" \
    -QuitOn "^flattoolgun: physgun done" -Timeout 180 -Filter "^(toolgun: (spawned|physgun|hand|aim|target)|vr_flat_mouse_move|flattoolgun|\(|Current)" > "$S/physgun.log" 2>&1
L="$S/physgun.log"
d=$(grep -aE "^toolgun: physgun holds [0-9]+, distance" "$L" | sed -E 's/.*distance ([0-9.-]+).*/\1/' | tr '\n' ' ')
t=$(grep -aE "^toolgun: physgun holds [0-9]+, distance" "$L" | tail -1 | sed -E 's/.*turn (.*)/\1/')
check $(grep -a "^toolgun: hand" "$L" | tail -1 | grep -aq "adjust 1" && echo 1 || echo 0) "physgun: B held is the gun hand's X/A (adjust 1)"
check $(awk -v d="$d" 'BEGIN { n = split(d, a, " "); print (n >= 2 && a[2] - a[1] > 19.5 && a[2] - a[1] < 20.5) ? 1 : 0 }') "physgun: the mouse up 40 counts pushed it out 20 units (distances $d)"
check $(echo "$t" | grep -aqE "(^| )-20\.0( |$)" && echo 1 || echo 0) "physgun: the mouse right 50 counts turned it -20 degrees (turn $t)"
check $(grep -a "^vr_flat_mouse_move: 0 -40" "$L" | grep -aq taken && grep -a "^vr_flat_mouse_move: 5 0" "$L" | grep -aq "the view's" && echo 1 || echo 0) "physgun: the mouse the tool's while B is held, the view's after"
verdict; exit $fail
