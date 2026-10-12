#!/bin/bash
# limb_wear_test.sh <agent>: a ragdolled corpse's head and limbs have their own health against every weapon
# (vr_limbs.qc VR_Limb_CorpseWear; ROUND22.md, "Corpse limbs: any weapon wears them off"). With the pop chances 0, each
# weapon's repeated hits at a corpse's limb take it off (vr_limb_test 30 shotgun, 31 super shotgun, 32 lightning,
# 33 nails, 34 rifle lasers, 35 AD projectile pellets, 36 axe blows), its whole-body health untouched; 37 shotgun blasts
# at the torso gib it; 38 shotgun blasts at the head pop it. Then the worn damage persists through a save/load (vr_limb_test 28, 29). Any FAIL or a missing
# PASS fails the script.
set -e
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT="$TREE/scratch/limb_wear_test.txt"
mkdir -p "$TREE/scratch"
SETUP="developer 1;sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_limbs 1;vr_limbs_corpses 1;vr_decap 1;vr_decap_corpses 1;vr_ragdoll 1;vr_corpse_gib 1;vr_corpse_nogib 0;vr_limbs_chance_scale 0;vr_decap_chance_scale 0;vr_test_spawn 0;vr_test_spawn_dist 90;impulse 241;wait30;vr_test_kill 1;wait160"
: > "$OUT"
for n in 30 31 32 33 34 35 36 37 38; do
    bash "$KIT/run.sh" "$AGENT" -Script "$SETUP;vr_limb_test $n;wait10;toggleconsole;quit" -Filter "limbtest: wear|Error|CRASH" -Timeout 180 | tee -a "$OUT"
done
bash "$KIT/run.sh" "$AGENT" -Script "$SETUP;vr_limb_test 28;wait20;save limbwear_dmg;wait10;load limbwear_dmg;wait40;vr_limb_test 29;wait10;toggleconsole;quit" -Filter "limbtest: 2[89]|Error|CRASH" -Timeout 180 | tee -a "$OUT"
PASSES=$(grep -c "PASS" "$OUT" || true)
if grep -qE "FAIL|Error|CRASH" "$OUT" || [ "$PASSES" -lt 11 ]; then
    echo "LIMB WEAR FAILED ($PASSES/11 PASS)"
    exit 1
fi
echo "LIMB WEAR PASS ($PASSES/11)"
