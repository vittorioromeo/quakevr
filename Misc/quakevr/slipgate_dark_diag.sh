#!/bin/bash
# diagnostic (not a shipped test): why the view through a slipgate is dark from far away (vr_portals_shot reads the
# portal scene's own targets back: colour and depth, whole and in the gate's box on screen). Each CASE changes one
# thing at a time, from the same far position, so the numbers say which part of the view is missing.
#   CASE1 far, as shipped                      - the baseline
#   CASE2 far, r_novis 1                       - every surface is offered: is the destination reached at all?
#   CASE3 far, r_fullbright 1                  - the world drawn without its lightmap: geometry or lighting?
#   CASE4 far, no dynamic light / no shadows   - the clustered lights and shadow atlases off
#   CASE5 near, as shipped                     - the control that is known to work
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-dark}
F='CASE|VR portal shot|whole|box|Wrote|ENGINE CRASH|Host_Error|looking through'

bash "$K/run.sh" "$A" -Timeout 420 -Script "map start;wait90;god;notarget;
echo CASE1-far; setpos 232 400 24 0 90 0; wait60; vr_portals_shot; wait10;
echo CASE2-novis; r_novis 1; wait40; vr_portals_shot; wait10; r_novis 0; wait20;
echo CASE3-fullbright; r_fullbright 1; wait40; vr_portals_shot; wait10; r_fullbright 0; wait20;
echo CASE4-nolight; r_dynamic 0; vr_shadow_dlights 0; vr_shadow_maplights 0; wait40; vr_portals_shot; wait10; r_dynamic 1; vr_shadow_dlights 1; vr_shadow_maplights 1; wait20;
echo CASE5-near; setpos 232 1372 24 0 90 0; wait40; vr_portals_shot; wait10;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
