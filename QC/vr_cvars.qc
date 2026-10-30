// ----------------------------------------------------------------------------
// CVar Handle Variables: the engine's own cvars and the ones vr_cvars.inc makes through its macros. Quake VR's other
// cvars the QC reads have theirs generated (vr_cvars_gen.qc: Misc/quakevr/gen_vr_cvars_qc.py, from vr_cvars.inc and
// the QC's uses; a new cvarh_ of one: run it).
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
float cvarh_vr_knockdown_chance_army;
float cvarh_vr_knockdown_chance_enforcer;
float cvarh_vr_knockdown_chance_dog;
float cvarh_vr_knockdown_chance_zombie;
float cvarh_vr_knockdown_chance_mummy;
float cvarh_vr_knockdown_chance_knight;
float cvarh_vr_knockdown_chance_hknight;
float cvarh_vr_knockdown_chance_ogre;
float cvarh_vr_knockdown_chance_demon;
float cvarh_vr_knockdown_chance_shambler;
float cvarh_vr_knockdown_chance_gremlin;
float cvarh_vr_knockdown_chance_crawler;
float cvarh_vr_kdtrig_resist_army;
float cvarh_vr_kdtrig_resist_enforcer;
float cvarh_vr_kdtrig_resist_knight;
float cvarh_vr_kdtrig_resist_hknight;
float cvarh_vr_kdtrig_resist_ogre;
float cvarh_vr_kdtrig_resist_zombie;
float cvarh_developer;

#include "vr_cvars_gen.qc"

// ----------------------------------------------------------------------------
// CVar Handle Initialization
// ----------------------------------------------------------------------------

void() VR_CVars_InitAllHandles =
{
    cvar_hclear();

#define VR_CVAR_HMAKE(xName) \
    cvarh_ ## xName = cvar_hmake(#xName)
    VR_CVAR_HMAKE(developer);
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
    VR_CVAR_HMAKE(vr_knockdown_chance_army);
    VR_CVAR_HMAKE(vr_knockdown_chance_enforcer);
    VR_CVAR_HMAKE(vr_knockdown_chance_dog);
    VR_CVAR_HMAKE(vr_knockdown_chance_zombie);
    VR_CVAR_HMAKE(vr_knockdown_chance_mummy);
    VR_CVAR_HMAKE(vr_knockdown_chance_knight);
    VR_CVAR_HMAKE(vr_knockdown_chance_hknight);
    VR_CVAR_HMAKE(vr_knockdown_chance_ogre);
    VR_CVAR_HMAKE(vr_knockdown_chance_demon);
    VR_CVAR_HMAKE(vr_knockdown_chance_shambler);
    VR_CVAR_HMAKE(vr_knockdown_chance_gremlin);
    VR_CVAR_HMAKE(vr_knockdown_chance_crawler);
    VR_CVAR_HMAKE(vr_kdtrig_resist_army);
    VR_CVAR_HMAKE(vr_kdtrig_resist_enforcer);
    VR_CVAR_HMAKE(vr_kdtrig_resist_knight);
    VR_CVAR_HMAKE(vr_kdtrig_resist_hknight);
    VR_CVAR_HMAKE(vr_kdtrig_resist_ogre);
    VR_CVAR_HMAKE(vr_kdtrig_resist_zombie);

#undef VR_CVAR_HMAKE
    VR_CVars_InitGenerated(); // (vr_cvars_gen.qc)
}

// ----------------------------------------------------------------------------
// Per-frame cvar values: read once a frame (StartFrame) for what every item's think asks (dozens
// of items think at 50 Hz), not once an entity.
// ----------------------------------------------------------------------------

float vr_weapondrop_particles_on;  // vr_weapondrop_particles
float vr_forcegrabbable_return;    // vr_forcegrabbable_return_time_deathmatch or _singleplayer
float vr_grab_gibs_level;          // vr_grab_gibs (the force grab's target search asks for every gib in reach)

void() VR_CVars_Frame =
{
    vr_weapondrop_particles_on = cvar_hget(cvarh_vr_weapondrop_particles);
    vr_grab_gibs_level = cvar_hget(cvarh_vr_grab_gibs);
    vr_forcegrabbable_return = deathmatch ? cvar_hget(cvarh_vr_forcegrabbable_return_time_deathmatch)
                                          : cvar_hget(cvarh_vr_forcegrabbable_return_time_singleplayer);
}
