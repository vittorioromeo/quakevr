#!/bin/bash
# prone_vent_test.sh <agent> -- flat-screen lying down in vrteleporters' 20-high vent (scenario prone_vent; ROUND24.md,
# "Flat fixes, round 2"), headless. Lying in the vent, under the teleporter in the wall over it:
#   crouch   the crouch pressed while lying: still lying (server box 16), eyes under the vent's top, not teleported;
#   out      crawled out (setpos past the vent) with that get-up waiting: up by itself;
#   key      lying in the vent again, the prone key (vr_prone_toggle) to get up: still lying, not teleported;
#   jump     the jump key pressed lying in the vent (vr_flat_prone_jump_stands): the same as the key: still lying, not teleported;
#   cancel   the key pressed again (it takes the wish back): out of the vent he stays lying;
#   open     then the key out in the open: up.
AGENT=${1:-blendmodels}
QT_NAME=prone_vent
source "$(dirname "$0")/qvrtest.sh"
IN="setpos 624 -112 24 0 0 0; noclip"
OUT="setpos 760 -112 24 0 0 0; noclip"
S="map vrteleporters; wait60; god; notarget; setpos 480 -112 24 0 0 0; noclip; wait10; vr_prone_toggle; wait90; $IN; wait30"
S="$S; +flatcrouch; wait60; echo PVT crouch; vr_prone_status; -flatcrouch; wait60; echo PVT crouchup; vr_prone_status"
S="$S; $OUT; wait60; echo PVT out; vr_prone_status"
S="$S; vr_prone_toggle; wait90; $IN; wait30; vr_prone_toggle; wait60; echo PVT key; vr_prone_status"
S="$S; +jump; wait30; -jump; wait30; echo PVT jump; vr_prone_status"
S="$S; vr_prone_toggle; wait30; $OUT; wait60; echo PVT cancel; vr_prone_status"
S="$S; vr_prone_toggle; wait60; echo PVT open; vr_prone_status; echo PVT done; toggleconsole; quit"
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^PVT done" -Timeout 120 -Filter "^PVT|^prone: on" -Script "$S" 2>&1)
# at <tag>: the vr_prone_status line after the marker
at() { grep -a -A1 "^PVT $1" <<<"$log" | grep -a "^prone: on" | head -1; }
f() { sed -E "s/.* $2 ([-0-9.]+).*/\1/" <<<"$1"; } # f <line> <field>
lying() { [ "$(f "$1" lying)" = "1" ] && echo 1 || echo 0; }
up() { [ "$(f "$1" lying)" = "0" ] && echo 1 || echo 0; }
inVent() { awk -v x="$(f "$1" x)" 'BEGIN{print (x > 600 && x < 650) ? 1 : 0}'; }
eyeLow() { awk -v e="$(f "$1" eye)" 'BEGIN{print (e > 0 && e <= 17) ? 1 : 0}'; }
both() { [ "$1$2" = "11" ] && echo 1 || echo 0; }
for t in crouch crouchup key jump; do
    l=$(at $t)
    check $(both $(lying "$l") $(inVent "$l")) "$t: still lying in the vent, not teleported (${l:-no status})"
    check $(eyeLow "$l") "$t: eyes under the vent's 20-high top ($(f "$l" eye))"
done
l=$(at out); check $(up "$l") "out: the waiting get-up rises once out of the vent (${l:-no status})"
l=$(at cancel); check $(lying "$l") "cancel: the second press took the wish back: still lying out of the vent (${l:-no status})"
l=$(at open); check $(up "$l") "open: the key out in the open: up (${l:-no status})"
verdict; exit $fail
