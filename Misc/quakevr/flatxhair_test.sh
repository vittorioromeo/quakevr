#!/bin/bash
# flatxhair_test.sh <agent> -- flatscreen play's visor crosshair (vr_flat_xhair; Quake/vr/vr_flatxhair.cpp; docs/vr-port/FLATPLAY.md,
# "The visor crosshair"), headless, in fast mode, one run of e1m1. Reads vr_flat_xhair_probe (each gun's shot's hit, its place
# on the view, snapped or not, where it is drawn; and where the pellets fired since the last probe landed, from the server's own
# tracer lines) at:
#   open     the shotgun, open space ahead: snapped on the centre; its pellets land on the drawn crosshair
#   wall     48 units from a wall with vr_flat_aim_point 0 (the gun shoots along the crosshair's way from its own muzzle, not at
#            the point): off the centre, past the snap, drawn on the real hit; its pellets land on it, not on the centre
#   dual     a shotgun in each hand there: two crosshairs, one each side, each on its own gun's pellets
#   merged   both guns aiming at the point again (vr_flat_aim_point 1): one crosshair, on the centre
#   menu     the menu up: not drawn
#   visor    vr_flat_visor 0: still drawn
#   styles   vr_flat_xhair_style 0..7, then vr_flat_xhair 0: each style draws at the centre (pixels against the last, plain)
# Prints PASS/FAIL lines and "flatxhair_test: N passed, M failed"; exits 1 on a failure. Screenshots (scratch/flatxhair_strip.png):
# open, wall, dual, then the eight styles and none.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
mkdir -p "$S"
LOG=$S/flatxhair_test.log
X="vr_flat_xhair_probe"
FIRE="+attack; wait3; -attack; wait20"
FIRE2="+attack; +offhandattack; wait3; -attack; -offhandattack; wait20"
STY="vr_flat_xhair_style 0; wait3; screenshot; vr_flat_xhair_style 1; wait3; screenshot; vr_flat_xhair_style 2; wait3; screenshot; vr_flat_xhair_style 3; wait3; screenshot; vr_flat_xhair_style 4; wait3; screenshot; vr_flat_xhair_style 5; wait3; screenshot; vr_flat_xhair_style 6; wait3; screenshot; vr_flat_xhair_style 7; wait3; screenshot; vr_flat_xhair 0; wait3; screenshot; vr_flat_xhair 1; vr_flat_xhair_style 2"
# (vr_flat_xhair_smooth 0.03: the placement checks count frames for the glide to settle, at the old default's pace)
SCRIPT="map e1m1; god; notarget; developer 1; r_norefresh 0; vr_flat_reload 0; vr_flat_xhair_smooth 0.03; setpos 480 -352 88; impulse 154; wait60; \
echo flatxhair_test: open; $X; $FIRE; $X; screenshot; \
tune vr_flat_aim_point 0; setpos 480 720 88; wait40; echo flatxhair_test: wall; $X; $FIRE; $X; screenshot; \
impulse 174; wait40; echo flatxhair_test: dual; $X; $FIRE2; $X; screenshot; \
tune vr_flat_aim_point 1; wait40; echo flatxhair_test: merged; $X; \
togglemenu; wait3; echo flatxhair_test: menu; $X; togglemenu; wait3; \
vr_flat_visor 0; wait3; echo flatxhair_test: visor; $X; vr_flat_visor 1; \
impulse 170; setpos 480 -352 88; wait40; echo flatxhair_test: styles; $X; $STY; \
tune_reset; echo flatxhair_test: end"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Clean -Out "$S/flatxhair_strip.png" -QuitOn "flatxhair_test: end" -Timeout 120 \
    -Filter "^flatxhair" -Script "$SCRIPT; toggleconsole; quit" > "$LOG" 2>&1
python "$WT/Misc/quakevr/flatxhair_check.py" "$LOG" "$KIT/bases/$AGENT/qbase/quakevr/screenshots"
