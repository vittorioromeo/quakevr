//
//
//
// ----------------------------------------------------------------------------
// The run's mutators: the hooks the game calls (docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// A run's choices are the engine's (vr_runsetup.cpp's registry): each mutator's value is its cvar vr_run_<id>, set
// when the run starts, kept across changelevel, written into each saved game and set back on its load. A mutator's
// gameplay is a file of its own (vr_run_<id>.qc) answering the hooks below; a new one adds its call to the hooks it
// needs. The hooks:
// - VR_Run_NewParms: SetNewParms's end (a new game's loadout: parm1.., the holsters);
// - VR_Run_Damage: T_Damage's start (the damage a hit does);
// - VR_Run_RefusesPickup: weapon_touch (a weapon not taken);
// - VR_Run_RefusesFire: W_AttackImpl's start (a weapon that does not fire);
// - VR_Run_MonsterSpawned: walk/fly/swimmonster_start (self: a map's monster, its class's spawn function run; phase 2's
//   enemy count and randomized enemies);
// - VR_Run_ItemSpawned: StartItem (self: a map's item; phase 2's randomized pickups).
// Saved games: the engine's `// qvr_run` line holds every choice; a mutator with QC state of its own keeps it in a
// saved field or global (they are saved) and reads its cvar again after a load.

void() VR_Run_NewParms =
{
    if(VR_GameUtil_InTutorialMap()) return; // (the tutorial's own loadout)
    VR_RunStartWeapon_NewParms();
};

float(entity targ, entity attacker, float damage) VR_Run_Damage =
{
    return VR_RunGlassCannon_Damage(targ, damage);
};

float(float wid) VR_Run_RefusesPickup =
{
    return VR_RunMeleeOnly_Refuses(wid);
};

float(float wid) VR_Run_RefusesFire =
{
    return VR_RunMeleeOnly_Refuses(wid);
};

void() VR_Run_MonsterSpawned = {};

void() VR_Run_ItemSpawned = {};

// The hub's teleporter to the start map (changelevel_touch): the Run Setup screen first (`vr_runsetup hub`); its Start
// sets vr_run_hubgo and the teleporter lets the player through. TRUE: not this time (asked, or still standing in it
// after Back: asked again once he has stepped out). vr_runsetup_ask 0: never asked.
float vr_run_hub_touched; // the last frame the teleporter was touched while it asked

float(entity trig) VR_Run_HubAsks =
{
    if(mapname != "vrstart" || trig.map != "start" || deathmatch || coop) return FALSE;
    if(cvar_hget(cvarh_vr_run_hubgo))
    {
        cvar_set("vr_run_hubgo", "0");
        return FALSE;
    }
    if(!cvar_hget(cvarh_vr_runsetup_ask)) return FALSE;
    float inside = time - vr_run_hub_touched < 0.5;
    vr_run_hub_touched = time;
    if(!inside) localcmd("vr_runsetup hub\n");
    return TRUE;
};
