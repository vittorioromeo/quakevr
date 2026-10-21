#!/bin/bash
# ad_pop_odds_test.sh <agent>: an AD projectile blast's head pop odds are one roll's at its whole share, as hitscan's.
# 6-pellet AD head volleys (vr_ad_test 51: a corpse, 50: a live kill) with chance pops: every armed pellet only tops the
# blast's chance up (VR_AD_PopTopUp), so 1 - prod(1 - roll) over a volley must equal its final chance. Then a live kill
# whose first roll loses and whose last top-up wins (vr_decap_pop_roll 0.3): a late cut on the just-killed body, the
# head popped and the body not gibbed by the rest of the blast.
set -eu
AGENT=${1:?worker name}
K=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/ad_pop_odds_test.log
B='developer 1;vr_stealth_sneak 1;vr_ragdoll_inherit 0;sv_gravity 0;vr_debug_shots 1;vr_ad_projectiles 2;vr_ragdoll 1;vr_limbs 1;vr_decap_pop_always_range 0;vr_decap_pop_never_range 20;vr_decap_pop_laser 1;vr_ragdoll_start 0;god'
V='map vrmelee-recording;wait30;sv_gravity 800;scratch1 0;scratch2 0;scratch4 1;scratch3 6'
S="$B;vr_decap_pop_roll 0.999;$V;vr_ad_test 51;wait270;$V;vr_ad_test 50;wait270;vr_decap_pop_roll 0.3;vr_decap_pop_pellet_weight 1;vr_decap_pop_never_range 1000;$V;vr_ad_test 50;wait270;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -ExtraArgs '-nomapindex' -Script "$S" -Filter 'adshots: pop|adshots-test: mode|Host_Error|SV_Error|ENGINE ERROR|exit=' -Timeout 300 > "$OUT"
grep -q '^exit=0' "$OUT"
grep -c '^adshots: pop blast ' "$OUT" | grep -q '^18$'
# Each volley (6 rolls): the chance it popped by its last roll, from the rolls, is its final chance.
awk '/^adshots: pop blast /{ if($8 == "0.000"){ keep = 1 } keep *= (1 - $10); last = $6; n++; if(n % 6 == 0){ d = (1 - keep) - last; if(d < -0.003 || d > 0.003){ bad = 1 }; printf "volley %d: rolls give %.3f, final chance %.3f\n", n / 6, 1 - keep, last } } END { exit bad }' "$OUT"
grep -q 'mode 51 region 0 health -1 damage 0 head 0 limbs 0 gib 0' "$OUT"
grep -q 'mode 50 region 0 health -1 damage 2 head 1 .* gib 0' "$OUT"
if grep -qE 'Host_Error|SV_Error|ENGINE ERROR' "$OUT"; then exit 1; fi
printf 'adpopodds PASS: 3 volleys topped up to their final chance; late top-up popped the head, no gib\n'
