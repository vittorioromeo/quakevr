#!/bin/bash
# config111_test.sh <agent> -- config version 111 (ROUND21.md, "The author's melee, throwing and menu settings of
# 2026-10-09 are the defaults"), headless: a config of version 110 holding every old default takes the new ones; a
# setting the player changed (vr_melee_speed 2.5, vr_menu_scale 0.3) keeps its value. Expected: the NEW block lists
# each setting at its new default (vr_cfg_version 111), the KEPT block 2.5 and 0.3 (and vr_bash_damage 10, new).
AGENT=${1:-defaults5}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
OLD="vr_2h_throw_velocity_mult 1.0;vr_bash_damage 8;vr_bullettime_tap_sound 0.6;vr_chainsaw_damage 80;vr_counter_damage 1.5"
OLD="$OLD;vr_dmg_chainsaw_swing 20;vr_dmg_laser 18;vr_enfrifle_damage 15;vr_gib_spawn_harmless 0.3;vr_gruntgun_damage 5"
OLD="$OLD;vr_headbutt_damage 32;vr_melee_bloodlust_mult 0.5;vr_melee_dmg_multiplier 1.0;vr_melee_speed 3;vr_menu_distance 100"
OLD="$OLD;vr_menu_scale 0.18;vr_menu_sharpen 0.5;vr_parry_stagger 0.75;vr_parry_stamina_cost 30;vr_parry_unarmed_reduction 0.5"
OLD="$OLD;vr_prop_drop_grace 0.5;vr_quad_melee_damage 1;vr_strike_stamina_cost_2h 6;vr_strike_stamina_punch 4"
OLD="$OLD;vr_sword_damage_mult 1;vr_throw_slowmo_flick 1;vr_throw_slowmo_long_travel 0.2;vr_throw_slowmo_short_travel 0.15"
OLD="$OLD;vr_weapon_throw_damage_mult 0.35;vr_weight_damage_exp 0.4;vr_weight_lenient 0.5"
P="${OLD//;/
}"; P=$(echo "$P" | sed 's/ .*//' | tr '\n' ';')
S="wait5;echo === NEW;vr_cfg_version 110;$OLD;vr_migrate_config;${P}vr_cfg_version"
S="$S;echo === KEPT;vr_cfg_version 110;$OLD;vr_melee_speed 2.5;vr_menu_scale 0.3;vr_migrate_config"
S="$S;vr_melee_speed;vr_menu_scale;vr_bash_damage;echo === END;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|\" is \"" "$@"
