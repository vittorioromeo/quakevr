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
float cvarh_skill;
float cvarh_sv_aim;
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
float cvarh_vr_melee_dmg_multiplier;
float cvarh_vr_melee_range_multiplier;
float cvarh_vr_melee_threshold;
float cvarh_vr_positional_damage;
float cvarh_vr_weapon_cycle_mode;
float cvarh_vr_weapon_throw_damage_mult;
float cvarh_vr_weapon_throw_mode;
float cvarh_vr_weapon_throw_velocity_mult;
float cvarh_vr_weapondrop_particles;

//
//
//
// ----------------------------------------------------------------------------
// CVar Handle Initialization
// ----------------------------------------------------------------------------

void() VR_CVars_InitAllHandles =
{
    cvarh_deathmatch = cvar_hmake("deathmatch");
    cvarh_fraglimit = cvar_hmake("fraglimit");
    cvarh_gamecfg = cvar_hmake("gamecfg");
    cvarh_noexit = cvar_hmake("noexit");
    cvarh_registered = cvar_hmake("registered");
    cvarh_samelevel = cvar_hmake("samelevel");
    cvarh_skill = cvar_hmake("skill");
    cvarh_sv_aim = cvar_hmake("sv_aim");
    cvarh_teamplay = cvar_hmake("teamplay");
    cvarh_temp1 = cvar_hmake("temp1");
    cvarh_timelimit = cvar_hmake("timelimit");
    cvarh_v_ipitch_level = cvar_hmake("v_ipitch_level");
    cvarh_v_iroll_level = cvar_hmake("v_iroll_level");
    cvarh_v_iyaw_level = cvar_hmake("v_iyaw_level");
    cvarh_vr_ammobox_drops_chance_mult = cvar_hmake("vr_ammobox_drops_chance_mult");
    cvarh_vr_ammobox_drops = cvar_hmake("vr_ammobox_drops");
    cvarh_vr_body_interactions = cvar_hmake("vr_body_interactions");
    cvarh_vr_enabled = cvar_hmake("vr_enabled");
    cvarh_vr_enemy_drops_chance_mult = cvar_hmake("vr_enemy_drops_chance_mult");
    cvarh_vr_enemy_drops = cvar_hmake("vr_enemy_drops");
    cvarh_vr_fakevr = cvar_hmake("vr_fakevr");
    cvarh_vr_forcegrab_eligible_haptics = cvar_hmake("vr_forcegrab_eligible_haptics");
    cvarh_vr_forcegrab_eligible_particles = cvar_hmake("vr_forcegrab_eligible_particles");
    cvarh_vr_forcegrab_mode = cvar_hmake("vr_forcegrab_mode");
    cvarh_vr_forcegrab_powermult = cvar_hmake("vr_forcegrab_powermult");
    cvarh_vr_forcegrab_radius = cvar_hmake("vr_forcegrab_radius");
    cvarh_vr_forcegrab_range = cvar_hmake("vr_forcegrab_range");
    cvarh_vr_holster_haptics = cvar_hmake("vr_holster_haptics");
    cvarh_vr_holster_mode = cvar_hmake("vr_holster_mode");
    cvarh_vr_melee_bloodlust = cvar_hmake("vr_melee_bloodlust");
    cvarh_vr_melee_dmg_multiplier = cvar_hmake("vr_melee_dmg_multiplier");
    cvarh_vr_melee_range_multiplier = cvar_hmake("vr_melee_range_multiplier");
    cvarh_vr_melee_threshold = cvar_hmake("vr_melee_threshold");
    cvarh_vr_positional_damage = cvar_hmake("vr_positional_damage");
    cvarh_vr_weapon_cycle_mode = cvar_hmake("vr_weapon_cycle_mode");
    cvarh_vr_weapon_throw_damage_mult = cvar_hmake("vr_weapon_throw_damage_mult");
    cvarh_vr_weapon_throw_mode = cvar_hmake("vr_weapon_throw_mode");
    cvarh_vr_weapon_throw_velocity_mult = cvar_hmake("vr_weapon_throw_velocity_mult");
    cvarh_vr_weapondrop_particles = cvar_hmake("vr_weapondrop_particles");
}
