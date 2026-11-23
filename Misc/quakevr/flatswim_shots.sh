#!/bin/bash
# flatswim_shots.sh <agent> [out.png] -- screenshots of flat-screen play's swimming strokes (vr_flatswim.cpp) in
# vrtesthall's pool, composed into one image: treading still, the breaststroke a quarter and three quarters into a stroke
# (moving forward), the sidestroke (moving right), and the nail gun held one-handed with the free hand stroking.
AGENT=${1:?agent}
OUT=${2:-C:/OHWorkspace/qvr-kit/scratch/flatswim_strokes.png}
source "$(dirname "$0")/qvrtest.sh"
at() { echo "setpos $1; noclip; wait20"; }
bash $KIT/run.sh $AGENT -Clean -ExtraArgs -flatscreenmode -Out "$OUT" -Timeout 150 -Filter "^flatswim_shots" -QuitOn "^flatswim_shots: done" \
    -Script "map vrtesthall; god; notarget; crosshair 0; wait5; \
$(at '384 -256 -64 20 90 0'); wait40; screenshot; \
$(at '240 -400 -64 20 45 0'); +forward; wait50; screenshot; wait40; screenshot; -forward; \
$(at '240 -400 -64 20 135 0'); +moveright; wait60; screenshot; -moveright; \
vr_test_give_wid 6; impulse 149; give n 200; $(at '384 -256 -64 20 90 0'); wait90; screenshot; \
echo flatswim_shots: done; toggleconsole; quit"
