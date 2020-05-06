//
//
//
// ----------------------------------------------------------------------------
// CVar Handle Variables
// ----------------------------------------------------------------------------

string cvarh_deathmatch;
string cvarh_fraglimit;
string cvarh_gamecfg;
string cvarh_noexit;
string cvarh_registered;
string cvarh_samelevel;
string cvarh_skill;
string cvarh_sv_aim;
string cvarh_teamplay;
string cvarh_temp1;
string cvarh_timelimit;
string cvarh_v_ipitch_level;
string cvarh_v_iroll_level;
string cvarh_v_iyaw_level;
string cvarh_vr_ammobox_drops_chance_mult;
string cvarh_vr_ammobox_drops;
string cvarh_vr_body_interactions;
string cvarh_vr_enabled;
string cvarh_vr_enemy_drops_chance_mult;
string cvarh_vr_enemy_drops;
string cvarh_vr_fakevr;
string cvarh_vr_forcegrab_eligible_haptics;
string cvarh_vr_forcegrab_eligible_particles;
string cvarh_vr_forcegrab_mode;
string cvarh_vr_forcegrab_powermult;
string cvarh_vr_forcegrab_radius;
string cvarh_vr_forcegrab_range;
string cvarh_vr_holster_haptics;
string cvarh_vr_holster_mode;
string cvarh_vr_melee_bloodlust;
string cvarh_vr_melee_dmg_multiplier;
string cvarh_vr_melee_range_multiplier;
string cvarh_vr_melee_threshold;
string cvarh_vr_positional_damage;
string cvarh_vr_weapon_cycle_mode;
string cvarh_vr_weapon_throw_damage_mult;
string cvarh_vr_weapon_throw_mode;
string cvarh_vr_weapon_throw_velocity_mult;
string cvarh_vr_weapondrop_particles;

// TODO VR: (P0) builtin
string(string xCvarName) cvar_hmake =
{
    return xCvarName;
}

float(string xCvarName) cvar_hget =
{
    return cvar(xCvarName);
}

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
