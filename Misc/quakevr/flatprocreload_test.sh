# flatprocreload_test.sh <agent> -- flat-screen play's procedural reload (flatprocreload, 2026-11-28; vr_flatprocreload.cpp,
# QC vr_flatreload.qc "The procedural reload"): the shotgun held in one hand, R: the free hand goes to the ammo pouch, takes
# taped pairs through VR's take and puts them in through VR's port test, 2 a trip, until it is full; a shot mid-trip
# cancels with no shell lost; a blocked port (vr_flat_prl_test_miss) trips the failsafe; dual wielding and the super
# shotgun keep the timed trip to the hip. prlfix: the gun's path over a whole reload has no step or back-and-forth spike
# (its per-frame trace, vr_flat_proc_reload_debug 2, at 72 and 144 frames a second), and the key-pose lock
# (vr_flat_prl_lock) holds each key, follows a tunable live, steps next/previous, scrubs and lets go. prlmove: full reloads
# while running, jumping, sliding, crouch-strafing (and at 60 / 144 fps) seat every pair with no failsafe, the free hand's
# fingers closed on each pair (a headset's grip), and the hand-at-port offsets move the locked hand at the port. Each run ends on its
# own verdict line (-QuitOn); flatprocreload_check.py checks the logs and prints "flatprocreload: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatprocreload
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(flat ?(proc ?)?reload|flatprocreload|fprl_test|fprl_trace)"
run() { local tag=$1 quit=$2 script=$3
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$script; wait600; echo fprl_test: $tag timed out; toggleconsole; quit" \
            -QuitOn "$quit|^fprl_test: $tag timed out" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
BASE="vr_flat_holsters 0; map vrtesthall; god; notarget; developer 1; vr_flat_reload_debug 1; vr_flat_proc_reload_debug 1; vr_flat_autoreload 0; vr_flat_proc_reload 1; wait5; setpos 0 300 25 0 90 0; wait5"
R="vr_mock_key r down; wait2; vr_mock_key r up"
SG="impulse 154; wait20"
EMPTY="+attack; wait300; -attack; wait10" # the shotgun's clip fired empty (its shots 0.5 s apart)

# R with the clip empty: trips of 2 to full, the server's "done (full)" ends it; the gun traced each frame (continuity).
run trips "^flat proc reload: main hand done" "$BASE; vr_flat_proc_reload_debug 2; $SG; $EMPTY; vr_flatprocreload_report; $R"
# The same at 144 frames a second: the path's continuity doesn't hang on the frame rate.
run cont144 "^flat proc reload: main hand done" "vr_fixed_frames_rate 144; $BASE; vr_flat_proc_reload_debug 2; $SG; $EMPTY; $R"
# The key-pose lock (no R): each key held (reported twice), a tunable followed live, next / previous, the scrub, let go.
# (wait40: the free hand's own pose settles off the gun's grip, flatsight's grip and look eases, before a key's two reports.)
LK="wait40; vr_flatprocreload_report; wait3; vr_flatprocreload_report"
run lock "^flatprocreload: the off hand back in its pose" "$BASE; $SG; vr_flat_prl_lock 1; $LK; vr_flat_prl_lock 2; $LK; vr_flat_prl_lock 3; $LK; \
vr_flat_prl_lock 4; $LK; vr_flat_prl_lock 5; $LK; vr_flat_prl_lock 6; $LK; vr_flat_prl_lock 7; $LK; vr_flat_prl_lock 3; wait40; \
echo fprl_test: before tune; vr_flatprocreload_report; tune vr_flat_prl_pouch_up 5; wait3; echo fprl_test: tuned; vr_flatprocreload_report; tune vr_flat_prl_pouch_up 1; \
vr_flat_prl_lock 7; vr_flat_prl_lock_next; wait3; echo fprl_test: next; vr_flatprocreload_report; \
vr_flat_prl_lock_prev; wait3; echo fprl_test: prev; vr_flatprocreload_report; \
vr_flat_prl_lock 6; wait40; echo fprl_test: port before; vr_flatprocreload_report; tune vr_flat_prl_port_up 2; tune vr_flat_prl_port_fwd 1; wait3; \
echo fprl_test: port nudged; vr_flatprocreload_report; tune vr_flat_prl_port_up 0; tune vr_flat_prl_port_fwd 0; \
vr_flat_prl_lock 8; vr_flat_prl_scrub 0.5; wait3; echo fprl_test: scrub; vr_flatprocreload_report; vr_flat_prl_scrub 0; vr_flat_prl_lock 0"
# prlmove: full reloads on the move (running in a circle, jumping, sliding, crouched strafing; running at 60 and 144 frames
# a second): every pair seated (none dropped), no failsafe, the gun full; the free hand's fingers closed on each pair.
J="+jump; wait3; -jump; wait30"
C="+flatcrouch; wait40; -flatcrouch; wait30"
MQ="^flat proc reload: main hand (done|cancelled)"
run mrun "$MQ" "$BASE; $SG; $EMPTY; +forward; +right; $R"
run mjump "$MQ" "$BASE; $SG; $EMPTY; +forward; +right; $R; $J; $J; $J; $J; $J; $J; $J; $J; $J; $J"
run mslide "$MQ" "$BASE; $SG; $EMPTY; +forward; +right; $R; $C; $C; $C; $C; $C; $C"
run mcrouch "$MQ" "$BASE; $SG; $EMPTY; +flatcrouch; +moveleft; +right; $R"
run mrun60 "$MQ" "vr_fixed_frames_rate 60; $BASE; $SG; $EMPTY; +forward; +right; $R"
run mrun144 "$MQ" "vr_fixed_frames_rate 144; $BASE; $SG; $EMPTY; +forward; +right; $R"
# A shot (a dry click: nothing in yet) while the first pair is carried: cancelled, the pair back in the pouch. (wait20: the
# carry of the first pair with his promoted times, 0.22-0.43 s; wait30 was past it, the pair already in.)
run fire "^flatprocreload: the off hand back in its pose" "$BASE; $SG; $EMPTY; $R; wait20; vr_flatprocreload_report; +attack; wait2; -attack"
# A quick melee (the shove's impulse, as mouse 5 sends it) while the pair is carried: cancelled, the pair back.
run melee "^flat proc reload: main hand cancelled" "$BASE; $SG; $EMPTY; $R; wait20; impulse 54"
# The port out of reach (the hand stops 12 units short): the client's failsafe cancels, the pair back in the pouch.
run blocked "^flat proc reload: main hand cancelled" "$BASE; vr_flat_prl_test_miss 12; $SG; $EMPTY; $R"
# A shotgun in each hand: the timed trip to the hip for both.
run dual "^flat reload: main hand done" "$BASE; impulse 174; wait5; $SG; +attack; +offhandattack; wait300; -attack; -offhandattack; wait10; $R"
# The super shotgun: the timed trip.
run ssg "^flat reload: main hand done" "$BASE; impulse 155; give s 50; wait20; +attack; wait3; -attack; wait150; +attack; wait3; -attack; wait30; $R"

python "$HERE/flatprocreload_check.py" "$S"
exit $?
