#!/bin/bash
# embedhold_test.sh <agent> -- vr_embed_test 64 (QC/vr_flatembedwalk_test.qc): flat play's stuck blade keeps its hands on their arms
# (embedhold2, his flat notes 2026-11-25 items 4 and 11). Six cases (a crate, a ragdoll, a live grunt, an ogre, a spear, a sword with the
# other hand free): after the stab he stands still half a second, and the engine's .vr_embed_armpast (cm the drawn wrist is past the most
# its arm reaches) stays under 3 cm there, the hold is still on and the blade came back out of the body by at most 2 units on its own
# (nothing moves the drawn hand: "the blade comes out about 0.1 s after the stab"); then he walks away and the hand stays on its wrist,
# the blade out into the hand or let go. The spear's and the sword's other hand holds the stuck blade the whole second
# (vr_flat_embed_other_hand). Two runs: the shipped defaults (vr_flat_embed_arm 1) and the rule off (0). Ends `embedhold: N passed, M failed`.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}

run()
{
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 300 \
        -Script "map vrtesthall; god; notarget; wait5; setpos 0 300 25 0 90 0; wait5; vr_flat_embed_arm $1; vr_embed_test 64" \
        -QuitOn "^flatembedwalk: [0-9]+ passed" -Filter "^(flatembedwalk: [0-9]+ passed|flatembedwalk: FAIL|exit=)" 2>&1
}

ON=$(run 1)
OFF=$(run 0)
echo "$ON"  | grep -E "^(flatembedwalk: FAIL|exit=)"
echo "$OFF" | grep -E "^(flatembedwalk: FAIL|exit=)"
V=$(printf '%s\n' "$ON" "$OFF" | grep -E "^flatembedwalk: [0-9]+ passed" )
echo "$V"
if [ "$(echo "$V" | grep -c passed)" != "2" ]; then
    echo "embedhold: FAIL a run printed no verdict (arm rule on/off)"
else
    echo "$V" | awk '/passed/ {p += $2; f += $4} END {printf "embedhold: %d passed, %d failed\n", p, f}'
fi
