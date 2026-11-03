#!/bin/bash
# closequarters_test.sh <agent>: the close-quarters slowdown (vr_closequarters; ROUND23.md, "Close-quarters slowdown").
# vrarena: a grunt spawned 600 units ahead (notarget: it stands, on the flat floor past the start's steps), walked
# straight into (250 frames: it is reached in about 140) then "approach": unslowed far off, slowed only within Start
# Radius, to Min Factor at its face. Then "coast" (the engine drives it, each step on its own condition): from 140 units
# short of the grunt, run at it with Brake Momentum off, let go of the stick at Start Radius, coast to a stop; the same
# with it on (it must stop shorter, short of the grunt); then strafe past (12 frames: nothing braked) and "end": the
# sideways move kept Strafe Factor of its speed, and the factors' rule on the settings. Ends on
# "vr_closequarters_test: N passed, M failed". CQS="cmd;" runs cmd first (e.g. "vr_closequarters_braking 40;");
# CQF overrides the output filter. PIN sets the slowdown's settings the checks were written for (the shipped defaults moved
# on 2026-11-03: start 1, end 0, min 0.8, strafe 0.85, angle 45, braking 60).
PIN="vr_closequarters_start 1.6;vr_closequarters_end 0.7;vr_closequarters_min 0.5;vr_closequarters_strafe 0.95;vr_closequarters_angle 30;vr_closequarters_braking 80;"
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
S="sv_random_seed 7;map vrarena;wait30;god;notarget;${PIN}${CQS}vr_test_spawn 0;vr_test_spawn_dist 600;impulse 241;wait40"
S="$S;vr_closequarters_test start;+forward;wait250;-forward;wait2;vr_closequarters_test approach"
S="$S;vr_closequarters_test coast"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "${CQF:-vr_closequarters_test}" -QuitOn "vr_closequarters_test: \d+ passed" -Timeout 120
