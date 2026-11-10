#!/bin/bash
# flat_fling_test.sh <agent> -- a cut-off head flies off vr_flat_cutoff_fling_mult (1.25) times faster on a flat screen
# (his note 2026-11-10; vr_box3d_ragparts.cpp cutLimb). The same sword slash at a standing grunt's head (vr_decap_test 1,
# the throw up off: vr_decap_head_lift 0) in a headset (the mock) and flat (-flatscreenmode): the cut head's speed
# ("decaptest: head ... N u/s"). PASS: flat over headset within 1.15..1.35. Ends `flatfling: PASS|FAIL ...`.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="map e1m1;wait30;god;notarget;vr_decap_head_lift 0;vr_decap_chance_scale 1;vr_test_spawn 0;impulse 241;wait20;vr_decap_test 1;wait30;toggleconsole;quit"
speed() { grep -ao "decaptest: head [^ ]* at .* [0-9]* u/s" | grep -ao "[0-9]* u/s" | head -1 | awk '{ print $1 + 0 }'; }
VR=$(bash "$KIT/run.sh" "$AGENT" -Filter "decaptest: head" -Script "$S" -Timeout 120 2>&1 | speed)
FLAT=$(bash "$KIT/run.sh" "$AGENT" -ExtraArgs "-flatscreenmode" -Filter "decaptest: head" -Script "$S" -Timeout 120 2>&1 | speed)
awk -v v="${VR:-0}" -v f="${FLAT:-0}" 'BEGIN {
    r = (v > 0) ? f / v : 0
    ok = (v > 0 && r >= 1.15 && r <= 1.35)
    printf "flatfling: %s the cut head %s u/s in a headset, %s u/s flat: x%.2f (vr_flat_cutoff_fling_mult 1.25)\n", ok ? "PASS" : "FAIL", v, f, r
    exit !ok
}'
