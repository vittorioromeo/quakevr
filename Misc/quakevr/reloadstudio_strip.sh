# reloadstudio_strip.sh <agent> [guns] -- reloadstudio3: frame strips of the Reload Studio against the game, for review. For each
# gun (ng sg ssg rv; or the ones named: sg ssg ng sng lg np gl px rl rv), in flat-screen play, partly fired as the pose test
# has it: `vr_reloadstudio strip` shoots the studio's lock on each key, then a real reload held at each key's end, from the
# studio's orbit camera; reloadstudio_strip.py lays them out a row a key (studio left, game right) into
# <worktree>/scratch/reloadstudio3_<gun>.png. Prints "reloadstudio_strip_test: N passed, M failed" over the guns.
AGENT=${1:?agent}
GUNS=${2:-ng sg ssg rv}
QT_NAME=reloadstudio_strip_test
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/reloadstudio_strip
mkdir -p "$S"
BASE="vr_flat_holsters 0; map vrtesthall; god; notarget; developer 1; vr_flat_autoreload 0; vr_flat_proc_reload 1; vr_flat_prl_giveup 6; wait5; setpos 0 300 25 0 90 0; wait5"
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
    bash $KIT/run.sh $AGENT -Clean -ExtraArgs -flatscreenmode -Script "$BASE; $GIVE; crosshair 0; vr_reloadstudio strip" \
        -QuitOn "^reloadstudio_strip: [0-9]+ passed" -Timeout 300 -Filter "^reloadstudio_strip" -Out "$S/${GUN}_raw.png" > "$S/$GUN.log" 2>&1
    OUT=$(cat "$S/$GUN.log")
    expect_verdict "$OUT" reloadstudio_strip "$GUN: the strip's keys shot"
    DIR=$(grep -a -o "full size in .*" "$S/$GUN.log" | sed 's/^full size in //' | tr -d '\r')
    python "$(dirname "$0")/reloadstudio_strip.py" "$S/$GUN.log" "$DIR" "C:/OHWorkspace/qvr-agents/$AGENT/scratch/reloadstudio3_$GUN.png" "$GUN"
done
verdict
