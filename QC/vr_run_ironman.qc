//
//
//
// ----------------------------------------------------------------------------
// Mutator: Ironman (vr_run_ironman; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// No saving: the engine refuses every save (save, quicksave, the autosaves: host_cmd.c's save asks
// VR_RunIronmanRefusesSave; host.c tries no autosave), greys the menus' Save and opens no death menu (no saves to load).
// A death (respawn, VR_Run_Respawn) restarts the level he is on from its start: "restart fresh", never the last save
// (Ironwail's autoload), with the loadout he entered it with.
// What "the start" is (decided): the start of the current level, as he entered it (a changelevel's, a new game's), not
// the campaign's or the episode's first map; a hub (id's start, a pack's hall) is a level like any other.
// The loadout: the engine's level-start parms (parm1..16 in its client record, parm17..72 beside them: vr_progs.cpp),
// set as he enters the level and written into every saved game: the restart gives them back (Quake's own single-player
// restart). Not a QC global: the restart loads the progs again, so a global would not outlive it.
float() VR_RunIronman_Respawn =
{
    if(!cvar_hget(cvarh_vr_run_ironman) || coop || deathmatch)
    {
        return FALSE;
    }
    if(!vr_restart_queued) // (once: PlayerDeathThink asks every frame the button is held)
    {
        vr_restart_queued = TRUE;
        dprint(sprintf("Ironman: %s from its start\n", mapname));
        localcmd("restart fresh\n");
    }
    return TRUE;
};
