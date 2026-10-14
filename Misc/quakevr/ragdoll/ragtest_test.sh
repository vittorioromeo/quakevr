#!/bin/bash
# ragtest_test.sh <agent> [names] -- the ragdoll behaviour tests (ROUND22.md, "Ragdoll tests, tuning, self-collision"):
# every button of Debug > Tests > Ragdoll Behaviours pressed headless (vr_menu_press, as the menu's laser does) on a
# fresh monster on vrtesthall, one game for all, and whether its behaviour began (`ragact:` ... began). The buttons are
# read from Quake/vr/vr_ragtest.cpp's table: a row added there is tested here. Prints one PASS/FAIL line per button (its
# `ragact` line), then the totals; NAMES (or the arguments): only those (death_body crawl ...).
#   MON=n     the monster (vr_test_spawn's numbers: 0 grunt (default), 1 ogre, 2 zombie, 5 knight, 6 death knight,
#             8 enforcer); the zombie only dies beheaded, so most of its deaths fail by design
#   SELF=0|1  vr_ragdoll_self_collide for the run (default: the game's)
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
# (The body: tb_main plays its runs as batches, testbatch.sh; TB_SLOW=1: each alone by run.sh, as before.)
tb_body() {
TREE=C:/OHWorkspace/qvr-agents/$AGENT
SRC="$TREE/Quake/vr/vr_ragtest.cpp"
MON=${MON:-0}
PAGE="Ragdoll Behaviour Tests"
WANT=" ${*:-${NAMES:-}} "
OUT="$TREE/scratch/ragtest_out_$$.txt"
mkdir -p "$TREE/scratch"
# Each row: name|label (the table's first line of each row: {"vr_ragdoll_act <name>", "<group>", "<label>",).
ROWS=$(grep -o '{"vr_ragdoll_act [a-z_]*", "[^"]*", "[^"]*"' "$SRC" | sed -E 's/\{"vr_ragdoll_act ([a-z_]+)", "[^"]*", "([^"]*)"/\1|\2/')
PRE="sv_random_seed ${SEED:-7};map vrtesthall;wait30;god;notarget;vr_ragdoll_active_seed ${SEED:-0}"
[ -n "${SELF:-}" ] && PRE="$PRE;vr_ragdoll_self_collide $SELF"
S="$PRE"
LEDGES=""
while IFS='|' read -r name label; do
    [ -z "$name" ] && continue
    [ "$WANT" != "  " ] && [[ "$WANT" != *" $name "* ]] && continue
    # Where: the open hall facing north (the north wall 300 units off for the lean: found and moved to); the ledge last,
    # facing the platform's west lip from 100 units back (found and moved to it).
    case $name in
    lean) at="setpos -150 300 24 0 90 0;vr_test_spawn_dist 110" ;;
    ledge|pullup) LEDGES="$LEDGES $name"; continue ;;
    # (A grab needs a second monster: one first 160 units off, then the one tested at 110, nearer.)
    grab) at="setpos -150 0 24 0 90 0;vr_test_spawn_dist 160;vr_test_spawn $MON;impulse 241;wait5;vr_test_spawn_dist 110" ;;
    *) at="setpos -150 0 24 0 90 0;vr_test_spawn_dist 110" ;;
    esac
    wait=360
    case $name in crawl|getup) wait=700 ;; esac
    # (A catch needs a death animation that falls forwards: about half the grunt's; three tries, one must catch.)
    tries=1
    [ "$name" = catch ] && tries=3
    for ((i = 0; i < tries; i++)); do
        S="$S;vr_knockdown_test 20;vr_scene_clean all;wait5;$at;wait5;vr_test_spawn $MON;impulse 241;wait40;vr_menu_press $PAGE > $label;wait$wait"
    done
done <<< "$ROWS"
for name in $LEDGES; do
    label=$(echo "$ROWS" | grep "^$name|" | cut -d'|' -f2)
    S="$S;vr_knockdown_test 20;vr_scene_clean all;wait5;setpos 430 430 216 0 180 0;vr_mock_turn_to 180;wait150;vr_test_spawn_dist 100"
    S="$S;vr_test_spawn $MON;impulse 241;wait40;vr_menu_press $PAGE > $label;wait400"
done
S="$S;vr_ragdoll_act status;toggleconsole;quit"
tb_run_sh -Script "$S" -Filter "^ragact|vr_menu_press:|rror|CRASH|exit=|TIMEOUT" -Timeout 900 2>&1 | tr -d '\r' > "$OUT"
grep -E "rror|CRASH|TIMEOUT" "$OUT" | head -5
pass=0; fail=0
while IFS='|' read -r name label; do
    [ -z "$name" ] && continue
    [ "$WANT" != "  " ] && [[ "$WANT" != *" $name "* ]] && continue
    line=$(grep -E "^ragact: $name .*began after" "$OUT" | head -1)
    [ -z "$line" ] && line=$(grep -E "^ragact: $name .*(NOT begun|not begun)" "$OUT" | head -1)
    [ -z "$line" ] && line=$(grep -E "^ragact: $name: " "$OUT" | head -1)
    if [[ "$line" == *"zombie dies only beheaded"* ]]; then echo "SKIP $name (not a zombie's)"
    elif [[ "$line" == *"began after"* ]]; then pass=$((pass + 1)); echo "PASS $line"
    else fail=$((fail + 1)); echo "FAIL $name: ${line:-no ragact line (pressed? see $OUT)}"; fi
done <<< "$ROWS"
grep -E "^ragact: status" "$OUT"
echo "ragtest: $pass passed, $fail failed"
}
source "$(dirname "$0")/../testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
