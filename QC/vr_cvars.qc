//
//
//
// ----------------------------------------------------------------------------
// CVar Handle Variables
// ----------------------------------------------------------------------------

float cvarh_deathmatch;
float cvarh_fraglimit;
float cvarh_gamecfg;
float cvarh_noexit;
float cvarh_registered;
float cvarh_samelevel;
float cvarh_saved1;
float cvarh_saved2;
float cvarh_saved3;
float cvarh_saved4;
float cvarh_scratch1;
float cvarh_scratch2;
float cvarh_scratch3;
float cvarh_scratch4;
float cvarh_skill;
float cvarh_sv_accelerate;
float cvarh_sv_aim;
float cvarh_sv_friction;
float cvarh_sv_gravity;
float cvarh_sv_maxspeed;
float cvarh_sv_stopspeed;
float cvarh_teamplay;
float cvarh_temp1;
float cvarh_timelimit;
float cvarh_v_ipitch_level;
float cvarh_v_iroll_level;
float cvarh_v_iyaw_level;
float cvarh_vr_ammobox_drops_chance_mult;
float cvarh_vr_ammobox_drops;
float cvarh_vr_body_interactions;
float cvarh_vr_enabled;
float cvarh_vr_enemy_drops_chance_mult;
float cvarh_vr_enemy_drops;
float cvarh_vr_fakevr;
float cvarh_vr_forcegrab_eligible_haptics;
float cvarh_vr_forcegrab_eligible_particles;
float cvarh_vr_forcegrab_mode;
float cvarh_vr_forcegrab_powermult;
float cvarh_vr_forcegrab_radius;
float cvarh_vr_forcegrab_range;
float cvarh_vr_holster_haptics;
float cvarh_vr_holster_mode;
float cvarh_vr_melee_bloodlust;
float cvarh_vr_melee_bloodlust_mult;
float cvarh_vr_melee_dmg_multiplier;
float cvarh_vr_melee_range_multiplier;
float cvarh_vr_melee_threshold;
float cvarh_vr_positional_damage;
float cvarh_vr_weapon_cycle_mode;
float cvarh_vr_weapon_throw_damage_mult;
float cvarh_vr_weapon_throw_mode;
float cvarh_vr_weapon_throw_velocity_mult;
float cvarh_vr_weapondrop_particles;
float cvarh_vr_2h_spread_reduction;
float cvarh_vr_2h_throw_velocity_mult;
float cvarh_vr_headbutt_velocity_threshold;
float cvarh_vr_headbutt_damage_mult;
float cvarh_vr_verbosebots;
float cvarh_vr_forcegrabbable_ammo_boxes;
float cvarh_vr_forcegrabbable_health_boxes;
float cvarh_vr_forcegrabbable_return_time_deathmatch;
float cvarh_vr_forcegrabbable_return_time_singleplayer;
float cvarh_vr_enable_reload;

//
//
//
// ----------------------------------------------------------------------------
// CVar Handle Initialization
// ----------------------------------------------------------------------------

void() VR_CVars_InitAllHandles =
{
    cvar_hclear();

#define VR_CVAR_HMAKE(xName) \
    cvarh_ ## xName = cvar_hmake(#xName)

    VR_CVAR_HMAKE(deathmatch);
    VR_CVAR_HMAKE(fraglimit);
    VR_CVAR_HMAKE(gamecfg);
    VR_CVAR_HMAKE(noexit);
    VR_CVAR_HMAKE(registered);
    VR_CVAR_HMAKE(samelevel);
    VR_CVAR_HMAKE(saved1);
    VR_CVAR_HMAKE(saved2);
    VR_CVAR_HMAKE(saved3);
    VR_CVAR_HMAKE(saved4);
    VR_CVAR_HMAKE(scratch1);
    VR_CVAR_HMAKE(scratch2);
    VR_CVAR_HMAKE(scratch3);
    VR_CVAR_HMAKE(scratch4);
    VR_CVAR_HMAKE(skill);
    VR_CVAR_HMAKE(sv_accelerate);
    VR_CVAR_HMAKE(sv_aim);
    VR_CVAR_HMAKE(sv_friction);
    VR_CVAR_HMAKE(sv_gravity);
    VR_CVAR_HMAKE(sv_maxspeed);
    VR_CVAR_HMAKE(sv_stopspeed);
    VR_CVAR_HMAKE(teamplay);
    VR_CVAR_HMAKE(temp1);
    VR_CVAR_HMAKE(timelimit);
    VR_CVAR_HMAKE(v_ipitch_level);
    VR_CVAR_HMAKE(v_iroll_level);
    VR_CVAR_HMAKE(v_iyaw_level);
    VR_CVAR_HMAKE(vr_ammobox_drops);
    VR_CVAR_HMAKE(vr_ammobox_drops_chance_mult);
    VR_CVAR_HMAKE(vr_body_interactions);
    VR_CVAR_HMAKE(vr_enabled);
    VR_CVAR_HMAKE(vr_enemy_drops);
    VR_CVAR_HMAKE(vr_enemy_drops_chance_mult);
    VR_CVAR_HMAKE(vr_fakevr);
    VR_CVAR_HMAKE(vr_forcegrab_eligible_haptics);
    VR_CVAR_HMAKE(vr_forcegrab_eligible_particles);
    VR_CVAR_HMAKE(vr_forcegrab_mode);
    VR_CVAR_HMAKE(vr_forcegrab_powermult);
    VR_CVAR_HMAKE(vr_forcegrab_radius);
    VR_CVAR_HMAKE(vr_forcegrab_range);
    VR_CVAR_HMAKE(vr_holster_haptics);
    VR_CVAR_HMAKE(vr_holster_mode);
    VR_CVAR_HMAKE(vr_melee_bloodlust);
    VR_CVAR_HMAKE(vr_melee_bloodlust_mult);
    VR_CVAR_HMAKE(vr_melee_dmg_multiplier);
    VR_CVAR_HMAKE(vr_melee_range_multiplier);
    VR_CVAR_HMAKE(vr_melee_threshold);
    VR_CVAR_HMAKE(vr_positional_damage);
    VR_CVAR_HMAKE(vr_weapon_cycle_mode);
    VR_CVAR_HMAKE(vr_weapon_throw_damage_mult);
    VR_CVAR_HMAKE(vr_weapon_throw_mode);
    VR_CVAR_HMAKE(vr_weapon_throw_velocity_mult);
    VR_CVAR_HMAKE(vr_weapondrop_particles);
    VR_CVAR_HMAKE(vr_2h_spread_reduction);
    VR_CVAR_HMAKE(vr_2h_throw_velocity_mult);
    VR_CVAR_HMAKE(vr_headbutt_velocity_threshold);
    VR_CVAR_HMAKE(vr_headbutt_damage_mult);
    VR_CVAR_HMAKE(vr_verbosebots);
    VR_CVAR_HMAKE(vr_forcegrabbable_ammo_boxes);
    VR_CVAR_HMAKE(vr_forcegrabbable_health_boxes);
    VR_CVAR_HMAKE(vr_forcegrabbable_return_time_deathmatch);
    VR_CVAR_HMAKE(vr_forcegrabbable_return_time_singleplayer);
    VR_CVAR_HMAKE(vr_enable_reload);

#undef VR_CVAR_HMAKE
}
