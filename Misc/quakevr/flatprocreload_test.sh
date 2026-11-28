# flatprocreload_test.sh <agent> -- flat-screen play's procedural reload (flatprocreload, 2026-11-28; vr_flatprocreload.cpp,
# QC vr_flatreload.qc "The procedural reload"): the shotgun held in one hand, R: the free hand goes to the ammo pouch, takes
# taped pairs through VR's take and puts them in through VR's port test, 2 a trip, until it is full; a shot mid-trip
# cancels with no shell lost; a blocked port (vr_flat_prl_test_miss) trips the failsafe; dual wielding and the super
# shotgun keep the timed trip to the hip. Each run ends on its own verdict line (-QuitOn); flatprocreload_check.py checks
# the logs and prints "flatprocreload: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatprocreload
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(flat ?(proc ?)?reload|flatprocreload|fprl_test)"
run() { local tag=$1 quit=$2 script=$3
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$script; wait600; echo fprl_test: $tag timed out; toggleconsole; quit" \
            -QuitOn "$quit|^fprl_test: $tag timed out" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
BASE="vr_flat_holsters 0; map vrtesthall; god; notarget; developer 1; vr_flat_reload_debug 1; vr_flat_proc_reload_debug 1; vr_flat_autoreload 0; vr_flat_proc_reload 1; wait5; setpos 0 300 25 0 90 0; wait5"
R="vr_mock_key r down; wait2; vr_mock_key r up"
SG="impulse 154; wait20"
EMPTY="+attack; wait300; -attack; wait10" # the shotgun's clip fired empty (its shots 0.5 s apart)

# R with the clip empty: trips of 2 to full, the server's "done (full)" ends it.
run trips "^flat proc reload: main hand done" "$BASE; $SG; $EMPTY; vr_flatprocreload_report; $R"
# A shot (a dry click: nothing in yet) while the first pair is carried: cancelled, the pair back in the pouch.
run fire "^flatprocreload: the off hand back in its pose" "$BASE; $SG; $EMPTY; $R; wait30; vr_flatprocreload_report; +attack; wait2; -attack"
# A quick melee (the shove's impulse, as mouse 5 sends it) while the pair is carried: cancelled, the pair back.
run melee "^flat proc reload: main hand cancelled" "$BASE; $SG; $EMPTY; $R; wait30; impulse 54"
# The port out of reach (the hand stops 12 units short): the client's failsafe cancels, the pair back in the pouch.
run blocked "^flat proc reload: main hand cancelled" "$BASE; vr_flat_prl_test_miss 12; $SG; $EMPTY; $R"
# A shotgun in each hand: the timed trip to the hip for both.
run dual "^flat reload: main hand done" "$BASE; impulse 174; wait5; $SG; +attack; +offhandattack; wait300; -attack; -offhandattack; wait10; $R"
# The super shotgun: the timed trip.
run ssg "^flat reload: main hand done" "$BASE; impulse 155; give s 50; wait20; +attack; wait3; -attack; wait150; +attack; wait3; -attack; wait30; $R"

python "$HERE/flatprocreload_check.py" "$S"
exit $?
