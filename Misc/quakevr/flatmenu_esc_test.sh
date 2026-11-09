#!/bin/bash
# flatmenu_esc_test.sh <agent> -- Escape in the flat-screen menu is Back to game (vr_menu_flat_esc_game 1; ROUND24.md,
# "Flat fixes, round 2"), headless. In e1m1, flat-screen (-flatscreenmode), in both layouts (vr_menu_flat_vr_style 0
# and 1): Escape on the main menu and on Options closes the menu; togglemenu after it opens Options again (as Back to
# game remembers); with vr_menu_flat_esc_game 0 Escape on Options goes back to the main menu (Quake's). The VR mock:
# Escape on Options goes back a page as before (the headset's menu button is Back to game there already).
AGENT=${1:-blendmodels}
QT_NAME=flatmenu_esc
source "$(dirname "$0")/qvrtest.sh"
# (m_state: 1 the main menu, 10 Options: menu.h m_state_e)
one() { # one <style>: the script's part for a layout, its lines tagged E<style>
    local s=$1
    echo "vr_menu_flat_vr_style $s; vr_menu_flat_esc_game 1; togglemenu; wait5; echo E$s main; vr_mock_key escape; wait5;" \
         "menu_options; wait5; echo E$s options; vr_mock_key escape; wait5;" \
         "togglemenu; wait5; echo E$s reopen; vr_mock_key downarrow; wait5; vr_mock_key escape; wait5;" \
         "vr_menu_flat_esc_game 0; menu_options; wait5; echo E$s off; vr_mock_key escape; wait5; vr_mock_key escape; wait5"
}
# reopen: the menu togglemenu opened, read from a down arrow's line (10: m_options, where Escape left it)
S="map e1m1; wait60; god; notarget; $(one 0) $(one 1) echo E done; toggleconsole; quit"
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^E done" -Timeout 120 -Filter "^E|^vr_mock_key" -Script "$S" 2>&1)
after() { grep -a -A1 "^$1[[:space:]]*\$" <<<"$log" | grep -a "^vr_mock_key" | head -1; }
for s in 0 1; do
    l=$(after "E$s main"); check $(grep -q "(menu closed)" <<<"$l" && echo 1 || echo 0) "layout $s: Escape on the main menu closes it ($l)"
    l=$(after "E$s options"); check $(grep -q "(menu closed)" <<<"$l" && echo 1 || echo 0) "layout $s: Escape on Options closes it ($l)"
    l=$(after "E$s reopen"); check $(grep -q "menu 10, " <<<"$l" && echo 1 || echo 0) "layout $s: togglemenu after it opens Options again ($l)"
    l=$(after "E$s off"); check $(grep -q "menu 1, " <<<"$l" && ! grep -q "(menu closed)" <<<"$l" && echo 1 || echo 0) "layout $s: vr_menu_flat_esc_game 0: Escape on Options goes back to the main menu ($l)"
done
S="map e1m1; wait60; god; notarget; vr_menu_flat_esc_game 1; menu_options; wait5; echo E vr; vr_mock_key escape; wait5; vr_mock_key escape; wait5; echo E done; toggleconsole; quit"
log=$(bash $KIT/run.sh $AGENT -QuitOn "^E done" -Timeout 120 -Filter "^E|^vr_mock_key" -Script "$S" 2>&1)
l=$(after "E vr"); check $(grep -q "menu 1, " <<<"$l" && ! grep -q "(menu closed)" <<<"$l" && echo 1 || echo 0) "VR mock: Escape on Options goes back a page, as before ($l)"
verdict; exit $fail
