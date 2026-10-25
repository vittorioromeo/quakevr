#!/bin/bash
# ad_hits_test.sh <agent>: AD projectile pellets from the player's shotgun, super shotgun and burst rifle (vr_ad_test
# 40-42: AD_FirePellets at a frozen AD grunt, 128 and 384 units ahead) all land: their damage asserted. 44: an ammo
# pickup refreshes both hands' models.
set -eu
AGENT=${1:?worker name}
K=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/ad_hits_test.log
S='developer 0;sv_random_seed 7;vr_monster_health 0;vr_stealth_sneak 1;map e1m1;wait120;god;notarget;vr_ad_projectiles 2;vr_positional_damage 0;vr_gruntgun_damage 9;vr_ad_pellet_spread 0;scratch1 128;vr_ad_test 40;wait90;vr_ad_test 41;wait90;vr_ad_test 42;wait90;scratch1 384;vr_ad_test 40;wait90;vr_ad_test 41;wait90;vr_ad_pellet_spread 1;scratch1 128;vr_ad_test 41;wait90;scratch1 0;vr_ad_test 44;wait5;toggleconsole;quit'
bash "$K/run.sh" "$AGENT" -ExtraArgs '-nomapindex -noaddons' -Script "$S" -Filter '^adhits:|Host_Error|SV_Error|ENGINE ERROR|Fatal error|CRASH|exit=' -Timeout 300 > "$OUT"
grep -q '^exit=0' "$OUT"
grep -c '^adhits: source .* monster_army solid 3 ' "$OUT" | grep -q '^6$'
# No spread (vr_ad_pellet_spread 0): 6 pellets x 4 (shotgun, wid 4), 14 x 4 (super shotgun, 5), 1 x 9 (burst rifle
# round, 15): every pellet on the grunt, none stopped by the blast's other pellets.
grep -q '^adhits: wid 4 distance 128 damage 24 ' "$OUT"
grep -q '^adhits: wid 5 distance 128 damage 56 ' "$OUT"
grep -q '^adhits: wid 15 distance 128 damage 9 ' "$OUT"
grep -q '^adhits: wid 4 distance 384 damage 24 ' "$OUT"
grep -q '^adhits: wid 5 distance 384 damage 56 ' "$OUT"
# AD's spread (vr_ad_pellet_spread 1): some of the super shotgun's pellets miss at 128 units, not all.
grep -c '^adhits: wid 5 distance 128 damage ' "$OUT" | grep -q '^2$'
grep -qE '^adhits: wid 5 distance 128 damage ([4-9]|[1-4][0-9]|5[0-2]) ' "$OUT"
grep -q '^adhits: ammo update wid .* model progs/v_' "$OUT"
if grep -qE 'Host_Error|SV_Error|ENGINE ERROR|Fatal error|CRASH' "$OUT"; then exit 1; fi
printf 'adhits PASS: no spread SG 24, SSG 56 at 128 and 384 units, burst round 9; AD spread scatters; ammo refresh\n'
