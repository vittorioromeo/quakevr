# flatprocreload_test.sh <agent> [base|matrix|all] -- flat-screen play's procedural reload (flatprocreload, 2026-11-28;
# vr_flatprocreload.cpp, QC vr_flatreload.qc "The procedural reload"; since reloadsys, played from each gun's keyframes,
# vr_reloadsys.hpp, quakevr/data/reloads.txt): the shotgun held in one hand, R: the free hand goes to the ammo pouch, takes
# taped pairs through VR's take and puts them in through VR's port test, 2 a trip, until it is full; a shot mid-trip
# cancels with no shell lost; a blocked port (vr_flat_prl_test_miss) trips the failsafe; dual wielding keeps the timed trip
# to the hip. prlfix: the gun's path over a whole reload has no step or back-and-forth spike (its per-frame trace,
# vr_flat_proc_reload_debug 2, at 72 and 144 frames a second), and the key-pose lock (vr_flat_prl_lock N: key N of the
# gun's reload) holds each key, follows an edit live (vr_reloadsys_set), steps next/previous, scrubs and lets go. prlmove:
# full reloads while running, jumping, sliding, crouch-strafing (and at 60 / 144 fps) seat every pair with no failsafe, the
# free hand's fingers closed on each pair. reloadsys: the super shotgun broken open (or already open), a pair (one shell for
# one place) into its chambers, closed by the end's flick; the matrix ("almost always succeed"): both guns, standing,
# running, jumping, crouching, sliding, at 60 and 144 fps: every round seated, no failsafe, full. base: all but the matrix;
# matrix: the matrix alone; all (the default): both. Each run ends on its own verdict line (-QuitOn);
# flatprocreload_check.py checks the logs and prints "flatprocreload: N passed, M failed".
AGENT=${1:?agent}
MODE=${2:-all}
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
SSG="impulse 155; give s 50; wait20"
SSGEMPTY="+attack; wait3; -attack; wait150; +attack; wait3; -attack; wait30" # both barrels fired (it breaks open by itself)
SSGONE="+attack; wait3; -attack; wait150" # one barrel fired, one live
J="+jump; wait3; -jump; wait30"
C="+flatcrouch; wait40; -flatcrouch; wait30"
MQ="^flat proc reload: main hand (done|cancelled)"
SQ="^flat proc reload: main hand: the super shotgun closed|^flat proc reload: main hand cancelled"

if [ "$MODE" != "matrix" ]; then
# R with the clip empty: trips of 2 to full, the server's "done (full)" ends it; the gun traced each frame (continuity).
run trips "^flat proc reload: main hand done" "$BASE; vr_flat_proc_reload_debug 2; $SG; $EMPTY; vr_flatprocreload_report; $R"
# The same at 144 frames a second: the path's continuity doesn't hang on the frame rate.
run cont144 "^flat proc reload: main hand done" "vr_fixed_frames_rate 144; $BASE; vr_flat_proc_reload_debug 2; $SG; $EMPTY; $R"
# The key-pose lock (no R): each of the shotgun's 6 keys held (reported twice), an edit followed live, next / previous, the
# scrub, let go. (wait40: the free hand's own pose settles off the gun's grip, flatsight's grip and look eases, before a key's
# two reports.)
LK="wait40; vr_flatprocreload_report; wait3; vr_flatprocreload_report"
run lock "^flatprocreload: the off hand back in its pose" "$BASE; $SG; vr_flat_prl_lock 1; $LK; vr_flat_prl_lock 2; $LK; vr_flat_prl_lock 3; $LK; \
vr_flat_prl_lock 4; $LK; vr_flat_prl_lock 5; $LK; vr_flat_prl_lock 6; $LK; vr_flat_prl_lock 1; wait40; \
echo fprl_test: before tune; vr_flatprocreload_report; vr_reloadsys_set shotgun 1 hand_ofs 0 0.5 5; wait3; echo fprl_test: tuned; vr_flatprocreload_report; \
vr_reloadsys_set shotgun 1 hand_ofs 0 0.5 1; \
vr_flat_prl_lock 6; vr_flat_prl_lock_next; wait3; echo fprl_test: next; vr_flatprocreload_report; \
vr_flat_prl_lock_prev; wait3; echo fprl_test: prev; vr_flatprocreload_report; \
vr_flat_prl_lock 4; wait40; echo fprl_test: port before; vr_flatprocreload_report; vr_reloadsys_set shotgun 4 hand_ofs 1 0 2; wait3; \
echo fprl_test: port nudged; vr_flatprocreload_report; vr_reloadsys_set shotgun 4 hand_ofs 0 0 0; \
vr_flat_prl_lock -1; vr_flat_prl_scrub 0.4; wait3; echo fprl_test: scrub; vr_flatprocreload_report; vr_flat_prl_scrub 0; vr_flat_prl_lock 0"
# prlmove: full reloads on the move (running in a circle, jumping, sliding, crouched strafing; running at 60 and 144 frames
# a second): every pair seated (none dropped), no failsafe, the gun full; the free hand's fingers closed on each pair.
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
# The super shotgun fired empty (broken open by itself): a pair into its chambers, closed by the end's flick (impulse 108).
run ssg "$SQ" "$BASE; $SSG; $SSGEMPTY; $R"
# One barrel fired: the reload's flick breaks it open (impulse 107) keeping the live shell, one shell in, closed.
run ssg1 "$SQ" "$BASE; $SSG; $SSGONE; $R"
fi

if [ "$MODE" != "base" ]; then
# reloadsys's matrix: both guns, standing, running in a circle, jumping, crouch-strafing, sliding, at 60 and 144 fps.
for GUN in sg ssg; do
    if [ $GUN = sg ]; then GIVE="$SG; $EMPTY"; Q="$MQ"; else GIVE="$SSG; $SSGEMPTY"; Q="$SQ"; fi
    for FPS in 60 144; do
        PRE="vr_fixed_frames_rate $FPS; $BASE; $GIVE"
        run "x_${GUN}_stand_$FPS" "$Q" "$PRE; $R"
        run "x_${GUN}_run_$FPS" "$Q" "$PRE; +forward; +right; $R"
        run "x_${GUN}_jump_$FPS" "$Q" "$PRE; +forward; +right; $R; $J; $J; $J; $J; $J; $J; $J; $J; $J; $J"
        run "x_${GUN}_crouch_$FPS" "$Q" "$PRE; +flatcrouch; +moveleft; +right; $R"
        run "x_${GUN}_slide_$FPS" "$Q" "$PRE; +forward; +right; $R; $C; $C; $C; $C; $C; $C"
    done
done
fi

python "$HERE/flatprocreload_check.py" "$S" "$MODE"
exit $?
