#!/bin/bash
# shove_blood_test.sh <agent> -- a shove alone never bleeds (his note 2026-11-21: "blood particles are coming out of every
# limb ... when the shove results in the enemy becoming an active ragdoll"; ROUND25.md, "fixes1121"). One run on
# vrfiringrange, grunts shoved down (vr_knockdown_test 21, Knockdowns' Chance 100), the frames its flung limbs bled a
# trail counted (vr_decal_count's "ragdoll limbs flung" line) per case:
#   fresh   an unhurt grunt shoved down: 0 frames bled
#   old     a grunt shot for 5 (vr_test_hurt) 3 s before the shove: 0 (the old wound does not bleed again; it did: 220)
#   with    a grunt shot for 5 as it is shoved (2 frames before): its limbs bleed (more than 0; damage with the shove)
# Each wait is the logic's own frames at 72 a second: 40 for the spawn to settle, 200 (2.8 s, past the 1 s grace) from the
# old wound to the shove, 2 from the shot to the shove, 300 for the ragdoll to land and the count to settle.
# The last line: "shove_blood: N passed, M failed".
AGENT=${1:?worktree name}
source "$(dirname "$0")/../qvrtest.sh"
mkdir -p "$TREE/scratch"
OUT="$TREE/scratch/shove_blood_test_out.txt"
SPAWN="setpos 316 -556 56 0 180 0;vr_test_spawn 0;vr_test_spawn_dist 60;impulse 241;wait40"
S="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_knockdown_surprise 0;vr_knockdown_chance 100"
S="$S;$SPAWN;vr_decal_count;vr_knockdown_test 21;wait300;vr_decal_count;vr_knockdown_test 20"
S="$S;$SPAWN;vr_test_hurt 5;wait200;vr_decal_count;vr_knockdown_test 21;wait300;vr_decal_count;vr_knockdown_test 20"
S="$S;$SPAWN;vr_decal_count;vr_test_hurt 5;wait2;vr_knockdown_test 21;wait300;vr_decal_count;vr_knockdown_test 20"
bash "$KIT/run.sh" "$AGENT" -Script "$S;toggleconsole;quit" -Filter "limbs flung|test hurt|rror|CRASH" -Timeout 300 2>&1 \
    | tr -d '\r' > "$OUT"
grep -o "limbs flung: [0-9]* frames" "$OUT" | grep -o "[0-9]*" | tr '\n' ' ' > "$OUT.n"
read -r -a N < "$OUT.n"
if [ "${#N[@]}" -ne 6 ] || grep -q "rror\|CRASH" "$OUT"; then
    check 0 "the run: ${#N[@]} counts of 6 (see $OUT)"
else
    FRESH=$((N[1] - N[0])); OLD=$((N[3] - N[2])); DOWN=$((N[5] - N[4]))
    check $([ "$FRESH" = 0 ] && echo 1 || echo 0) "fresh: an unhurt grunt shoved down bled $FRESH frames (0)"
    check $([ "$OLD" = 0 ] && echo 1 || echo 0) "old: one shot 3 s before the shove bled $OLD frames (0)"
    check $([ "$DOWN" -gt 0 ] && echo 1 || echo 0) "with: one shot as it was shoved bled $DOWN frames (over 0)"
fi
verdict; exit $fail
