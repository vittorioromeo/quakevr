// ----------------------------------------------------------------------------
// CVar handles: `cvar_hget(cvarh_<name>)` reads a cvar or a tuning constant by handle (an index read; cvar("<name>")
// looks the name up every call). Writing cvarh_<name> in the QC is its whole declaration: qvr_checks cvars_qc (Tools/qvr_checks)
// writes every used handle's global and its make into vr_cvars_gen.qc (run it after adding or removing a use; the
// kit's build.sh fails while it is stale, or on a name that is no cvar or tunable). The handles are made when a map
// loads (OnSpawnServerBeforeLoad, before any entity spawns; again after a save's globals are read: OnLoadGame).
// ----------------------------------------------------------------------------

float vr_cvar_handles_bad; // handles made for a name the engine does not have (each a "cvar_hmake: unknown" line)

// One handle made: -1 (and counted) for an unknown name; every QC test then fails (VR_Test_Begin).
float(string name) VR_CVar_Make =
{
    float h = cvar_hmake(name);
    if(h < 0)
    {
        vr_cvar_handles_bad = vr_cvar_handles_bad + 1;
    }
    return h;
};

#include "vr_cvars_gen.qc"

void() VR_CVars_InitAllHandles =
{
    cvar_hclear();
    vr_cvar_handles_bad = 0;
    VR_CVars_InitGenerated(); // (vr_cvars_gen.qc)
};

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
