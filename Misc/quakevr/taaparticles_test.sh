#!/bin/bash
# taaparticles_test.sh <agent> -- particles at half size under TAA (his note 2026-11-12: a popped limb's blood stream
# not seen until he moved or turned; ROUND24.md, "A limb's fountain hidden by the TAA"). Flat-screen play, vrfiringrange,
# the view still: a screenshot, a blood spray 24 units ahead (vr_water_blood_spray 24 400: some 10000 particles, the
# large ones at half size: vr_particle_halfres_force 1), 22 frames, a screenshot; once with vr_flat_taa 0, once with 1.
# The share of the view's lower half the blood changed (taaparticles_check.py): with TAA at least 3/4 of without it.
# Before the fix the half-size blend went into the window's own framebuffer under TAA: 0.021 against 0.206.
# Prints PASS/FAIL and `taaparticles: N passed, M failed`; exits 1 on a failure.
AGENT=${1:-limbpfx}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
SHOTS=$KIT/bases/$AGENT/qbase/quakevr/screenshots
shot() {
    bash $KIT/run.sh $AGENT -Clean -ExtraArgs "-flatscreenmode" -Timeout 200 -Filter "vr_water_blood_spray|rror" \
        -Script "map vrfiringrange; wait60; god; notarget; vr_particle_seed 7; vr_particle_halfres_force 1; vr_flat_taa $1; wait30; screenshot; vr_water_blood_spray 24 400; wait22; screenshot; toggleconsole; quit" \
        > "$S/taaparticles_$1.log" 2>&1
    python "$TREE/Misc/quakevr/taaparticles_check.py" "$SHOTS" | awk '{print $2}'
}
off=$(shot 0)
on=$(shot 1)
check $(awk -v a="$off" 'BEGIN{exit !(a != "" && a > 0.05)}' && echo 1 || echo 0) \
    "without TAA the blood changes ${off:-?} of the view's lower half (over 0.05)"
check $(awk -v a="$off" -v b="$on" 'BEGIN{exit !(a != "" && b != "" && b >= 0.75 * a)}' && echo 1 || echo 0) \
    "with TAA ${on:-?}, at least 3/4 of without it (${off:-?})"
verdict; exit $fail
