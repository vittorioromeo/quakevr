#!/bin/bash
# visor_test.sh <agent> -- flatscreen play's visor HUD (vr_flat_visor; Quake/vr/vr_visor.cpp, vr_gadget_visor.inc;
# docs/vr-port/ROUND24.md, "Flat-screen visor HUD"), headless, in fast mode (the visor's clock is the frames' own).
# Reads vr_flat_visor_report through one run of e1m1 and the start map:
#   rest      standing still: the seven panels inside the view, each in its place, left and right mirrored, no sway;
#             every mount meets the screen's edges (report: contact), at rest, turning and walking too
#   turn      +right held: the panels lag the turn (a fraction of a degree, never more than 0.6 at sway 1)
#   settle    let go: back to exactly 0 within half a second
#   walk      +forward: the walk's bob moves them (at most a few tenths of a degree), then still again standing
#   hit       vr_flat_visor_fx damage 60: the glitch at once, the shake (in the sway) a moment later, both gone 0.5 s later
#   water     under the start map's pool: the fog up, the eye under water; surfaced: clearing, then clear
#   boot      vr_flat_visor_fx boot: the sweep under way, then done
#   lens      vr_flat_visor_warp -0.15 with vr_flat_visor_zoom 0.92, then 0.15 with 1.05: every mount still meets the edges
#   side      vr_flat_visor_side_out 0.3 and 3: the side mounts' readings nearer the edges and further in, the edges met
#   log       the log's mount at the top's middle, clear of the corners' readings; vr_flat_visor_log_width 0.5 with
#             _log_out 3 smaller and further down, 2 with 0.2 larger (shrunk to fit) and nearer the edge, the edge met
#   autohide  vr_flat_visor_autohide_log 1: with no lines the log's mount fades away; a message brings it back
#   status    the old status bar's room: vr_flat_visor 0 gives VR_VisorOn 0 (report: active 0)
# The screenshots (scratch/visor_strip.png): rest, three turning, settled, two walking.
# Prints PASS/FAIL lines and "visor_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
mkdir -p "$S"
LOG=$S/visor_test.log
R="vr_flat_visor_report"
SCRIPT="map e1m1; vr_flat_visor_autohide_log 0; wait60; god; notarget; wait60; echo visor_test: rest; $R; screenshot; +right; wait4; echo visor_test: turn; $R; screenshot; wait4; screenshot; wait4; $R; screenshot; -right; wait36; echo visor_test: settle; $R; screenshot; +forward; wait30; echo visor_test: walk; $R; screenshot; wait9; $R; screenshot; -forward; wait90; echo visor_test: still; $R; vr_flat_visor_fx damage 60; wait1; echo visor_test: hit; $R; wait3; echo visor_test: shaking; $R; wait33; echo visor_test: hit later; $R; map start; wait60; setpos 536 1440 100; wait45; echo visor_test: water; $R; setpos 544 288 28; wait36; echo visor_test: surfaced; $R; wait144; echo visor_test: dry; $R; vr_flat_visor_fx boot; wait20; echo visor_test: boot; $R; wait60; echo visor_test: booted; $R; vr_flat_visor_warp -0.15; vr_flat_visor_zoom 0.92; wait3; echo visor_test: pincushion; $R; vr_flat_visor_warp 0.15; vr_flat_visor_zoom 1.05; wait3; echo visor_test: barrel; $R; reset vr_flat_visor_warp; reset vr_flat_visor_zoom; vr_flat_visor_side_out 0.3; wait2; echo visor_test: side in; $R; vr_flat_visor_side_out 1; wait2; echo visor_test: side mid; $R; vr_flat_visor_side_out 3; wait2; echo visor_test: side out; $R; reset vr_flat_visor_side_out; vr_flat_visor_log_out 1; wait2; echo visor_test: log mid; $R; vr_flat_visor_log_width 0.5; vr_flat_visor_log_out 3; wait2; echo visor_test: log small; $R; vr_flat_visor_log_width 2; vr_flat_visor_log_out 0.2; wait2; echo visor_test: log big; $R; reset vr_flat_visor_log_width; reset vr_flat_visor_log_out; vr_flat_visor_autohide_log 1; wait45; echo visor_test: log hidden; $R; con_notifytime 3; vr_hud_console_log 1; echo a message; wait12; echo visor_test: log back; $R; vr_hud_console_log 0; reset vr_flat_visor_autohide_log; vr_flat_visor 0; wait3; echo visor_test: off; $R; echo visor_test: end"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Clean -Out "$S/visor_strip.png" -QuitOn "visor_test: end" \
    -Filter "^visor" -Script "$SCRIPT; toggleconsole; quit" > "$LOG" 2>&1
python "$WT/Misc/quakevr/visor_check.py" "$LOG"
