#!/bin/bash
set -eu
K=C:/OHWorkspace/qvr-kit
AGENT="${1:-admenu2}"
T="C:/OHWorkspace/qvr-agents/$AGENT"
S='developer 1;sv_random_seed 7;map e1m1;wait120;god;notarget;vr_ad_test 44;wait5;vr_ad_ammo_models 0;vr_ad_resistance 0;vr_ad_upgrades 0;vr_ad_projectiles 1;vr_ad_test 65;wait100;vr_ad_ammo_models 1;vr_ad_resistance 1;vr_ad_upgrades 1;vr_ad_projectiles 0;vr_ad_test 65;wait100;vr_ad_ammo_models 2;vr_ad_resistance 2;vr_ad_upgrades 2;vr_ad_projectiles 2;vr_ad_pellet_speed 1.75;vr_ad_enemy_pellet_speed 2.5;vr_ad_test 65;wait100;vr_toolgun_spawnlist;vr_ad_test 44;wait5;vr_ad_test 58;wait60;vr_menu_path_check maps/vrcalibration.map;toggleconsole;quit'
bash "$K/run.sh" "$AGENT" -ExtraArgs '-nomapindex -noaddons' -Script "$S" -Filter '^admenu|^adhits:|adshots.*speed|menu paths:|AD (Shadow Axe|Widowmaker|Plasma Gun)|Host_Error|SV_Error|ENGINE ERROR|Fatal error|CRASH|exit=' -Timeout 300 > "$T/scratch/admenu_vanilla.log"
S='developer 1;sv_random_seed 7;vr_campaign_native ad;wait30;map ad_crucial;wait120;god;notarget;vr_ad_test 44;wait5;vr_ad_ammo_models 0;vr_ad_resistance 0;vr_ad_upgrades 0;vr_ad_projectiles 1;vr_ad_test 65;wait100;vr_ad_ammo_models 1;vr_ad_resistance 1;vr_ad_upgrades 1;vr_ad_projectiles 0;vr_ad_test 65;wait100;vr_ad_ammo_models 2;vr_ad_resistance 2;vr_ad_upgrades 2;vr_ad_projectiles 2;vr_ad_test 65;wait100;vr_ad_test 44;wait5;vr_menu_path_check maps/vrcalibration.map;toggleconsole;quit'
bash "$K/run.sh" "$AGENT" -Games '-game ad -game quakevr' -ExtraArgs '-nomapindex -noaddons' -Script "$S" -Filter '^admenu|^adhits:|menu paths:|Host_Error|SV_Error|ENGINE ERROR|Fatal error|CRASH|exit=' -Timeout 300 > "$T/scratch/admenu_ad.log"
grep -E '^admenu:|^admenu-rules:|^admenu-hands:|^admenu-speed:|menu paths:|exit=' "$T/scratch/admenu_vanilla.log" "$T/scratch/admenu_ad.log"
