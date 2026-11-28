# reloadstudio_pose_test.sh <agent> [guns] -- reloadstudio2 (his note 2026-11-28 item 11): the Reload Studio shows what the
# game does. For each gun (sg ssg ng sng lg np gl px rl rv; or the ones named), in flat-screen play: the studio's lock on the
# gun's take key (the item in the hand at the pouch) and on the carry after it (off its destination by vr_flat_prl_test_miss),
# before any real reload of it; then a real reload held on the same keys (vr_flat_prl_hold); `vr_reloadstudio posetest`
# compares them (the hand placed and drawn off the gun hand, the item in the hand, the grasp's palm, the fingers) and ends on
# "reloadstudio_pose: N passed, M failed". Prints "reloadstudio_pose_test: N passed, M failed" over the guns.
AGENT=${1:?agent}
GUNS=${2:-sg ssg ng sng lg np gl px rl rv}
QT_NAME=reloadstudio_pose_test
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/reloadstudio_pose
mkdir -p "$S"
BASE="vr_flat_holsters 0; map vrtesthall; god; notarget; developer 1; vr_flat_proc_reload_debug 1; vr_flat_autoreload 0; vr_flat_proc_reload 1; vr_flat_prl_giveup 6; wait5; setpos 0 300 25 0 90 0; wait5"
SHOT="+attack; wait5; -attack; wait90"
for GUN in $GUNS; do
    case $GUN in
        sg) GIVE="impulse 154; wait20; +attack; wait300; -attack; wait10" ;;
        ssg) GIVE="impulse 155; give s 50; wait20; +attack; wait3; -attack; wait150; +attack; wait3; -attack; wait30" ;;
        ng) GIVE="impulse 156; give n 100; wait20; +attack; wait30; -attack; wait10" ;;
        sng) GIVE="impulse 157; give n 100; wait20; +attack; wait30; -attack; wait10" ;;
        lg) GIVE="impulse 161; give c 60; wait20; +attack; wait30; -attack; wait10" ;;
        np) GIVE="vr_test_give_wid 29; impulse 149; give n 100; wait20; +attack; wait30; -attack; wait10" ;;
        gl) GIVE="impulse 158; give r 50; wait20; +attack; wait300; -attack; wait20" ;;
        px) GIVE="impulse 159; give r 50; wait20; +attack; wait300; -attack; wait20" ;;
        rl) GIVE="impulse 160; give r 50; wait20; +attack; wait400; -attack; wait20" ;;
        rv) GIVE="vr_test_give_wid 31; impulse 149; give s 50; wait80; $SHOT; $SHOT" ;;
    esac
    bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$BASE; $GIVE; vr_reloadstudio posetest" \
        -QuitOn "^reloadstudio_pose: [0-9]+ passed" -Timeout 120 -Filter "^reloadstudio_pose" > "$S/$GUN.log" 2>&1
    OUT=$(cat "$S/$GUN.log")
    grep -a "reloadstudio_pose: .* (take|carry):" -E "$S/$GUN.log"
    expect_verdict "$OUT" reloadstudio_pose "$GUN: the lock's poses as a real reload's"
done
verdict
