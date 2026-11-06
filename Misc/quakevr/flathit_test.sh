#!/bin/bash
# flathit_test.sh <agent> -- flatscreen play's hit feedback (docs/vr-port/ROUND24.md, "Flat hit feedback"; vr_flathit.cpp):
# a real grunt shoots him from his right (e1m1's start, the grunt spawned 200 units up the corridor, he turned to face away
# from it a quarter turn), each run going on from his first hit (vr_flat_hit_test) and ending on its own line. Fast mode.
# The checks are flathit_check.py's (the log's probe lines and the screenshots' pixels).
#   a. hipfire: the punch turns the view left, away from the hit (the probe's yaw, and the picture moves by about as many
#      pixels), and settles back; a mark stands at the crosshair's right (pixels on its ring, and the probe's angle); the
#      hands' knock untouched (share 1).
#   b. sighted: the hands' knock kept at vr_flat_ads_hitreact (0.2) of itself; the punch at vr_flat_hitpunch_ads (0.5).
#   c. low health: the screen's edges redder at 10 health than at 100 (vr_flat_blood_health_size / _opacity), its middle not.
AGENT=${1:-armourstudio}
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
SECTIONS=${SECTIONS:-abc}
# (Quake's own view kick, its red flash, dynamic lights and the headset's blood at the edges off, so the pictures show the
# punch and the marks alone.)
P="map e1m1; wait10; developer 1; r_norefresh 0; setpos 480 -352 88 0 90 0; give h 250; vr_blood_hit 0; v_kickpitch 0; v_kickroll 0; gl_polyblend 0; r_dynamic 0; impulse 154; wait20; vr_test_spawn 0; vr_test_spawn_dist 200; impulse 241; wait10; setpos 480 -352 88 0 180 0; wait2"
# (The safety net: a run with no hit by then ends on this line, and its checks fail.)
END="wait900; echo flathit_timeout; toggleconsole; quit"
shots() { grep -a "full size in" "$1" | sed 's/.*full size in //' | tr -d '\r' | tail -1; }
judge() { while IFS='|' read -r ok what; do check "$ok" "$what"; done < <(python "$HERE/flathit_check.py" "$@" 2>&1 | tr -d '\r'); }

if [[ $SECTIONS == *a* ]]; then
  log=$HERE/scratch/flathit_a.log
  # (A bigger punch than his default, so the picture's move is well past a pixel's rounding: the grunt's hits are 3-5 points.)
  T="wait4, screenshot, vr_flat_hit_probe, wait3, screenshot, tune vr_flat_hitdir 0, wait2, screenshot, tune vr_flat_hitdir 0.8, wait60, vr_flat_hit_probe, screenshot, tune_reset, echo flathit_done"
  bash $KIT/run.sh $AGENT -Clean -ExtraArgs "-flatscreenmode" -Script "$P; tune vr_flat_hitpunch 6; vr_flat_hit_test $T; $END" -Filter "^flathit" -QuitOn "flathit_done|flathit_timeout" -Out flathit_a.png -Timeout 120 > "$log" 2>&1
  grep -a "^flathit" "$log" | grep -v "^flathit_done" | head -12 | sed 's/^/  /'
  dir=$(shots "$log"); mapfile -t png < <(ls "$dir"/*.png 2>/dev/null | sort | tail -4)
  judge a "$log" "${png[@]}"
fi

if [[ $SECTIONS == *b* ]]; then
  log=$HERE/scratch/flathit_b.log
  T="wait30, vr_flat_hit_probe, echo flathit_done"
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$P; vr_flat_aim 1; wait40; vr_flat_hit_probe; vr_flat_hit_test $T; $END" -Filter "^flathit" -QuitOn "flathit_done|flathit_timeout" -Timeout 120 > "$log" 2>&1
  grep -a "^flathitknock\|^flathitpunch" "$log" | tail -2 | sed 's/^/  /'
  judge b "$log"
fi

if [[ $SECTIONS == *c* ]]; then
  log=$HERE/scratch/flathit_c.log
  bash $KIT/run.sh $AGENT -Clean -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait10; r_norefresh 0; setpos 480 -352 88 0 90 0; give h 100; wait30; screenshot; give h 10; wait2; vr_blood_info; screenshot; give h 100; wait2; echo flathit_done; toggleconsole; quit" -Filter "^blood|flathit_done" -QuitOn "flathit_done" -Out flathit_c.png -Timeout 60 > "$log" 2>&1
  grep -a "^blood" "$log" | sed 's/^/  /'
  dir=$(shots "$log"); mapfile -t png < <(ls "$dir"/*.png 2>/dev/null | sort | tail -2)
  judge c "$log" "${png[@]}"
fi

verdict; exit $fail
