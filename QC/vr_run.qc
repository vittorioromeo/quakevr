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
// - VR_Run_ItemSpawned: StartItem (self: a map's item; phase 2's randomized pickups);
// - VR_Run_MapSpawned: the engine, a new map's entities all spawned (randomized pickups' swaps);
// - VR_Run_Respawn: respawn, a dead single player (Ironman's restart).
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

void() VR_Run_ItemSpawned =
{
    VR_RunPickups_ItemSpawned();
};

// The engine's VR_OnSpawnServerSpawned: every entity of a newly loaded map spawned (not a saved game's), precaches still
// open. `held`: the weapons (IT_ bits) the player enters with, 0 in a new game. Randomized Pickups swaps the map's items.
void(float held) VR_Run_MapSpawned =
{
    VR_RunPickups_MapSpawned(held);
};

// respawn (client.qc), a dead single player's: TRUE when a mutator took it over (Ironman: the level restarted).
float() VR_Run_Respawn =
{
    return VR_RunIronman_Respawn();
};

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
    float inside = ((time - vr_run_hub_touched) < 0.5);
    vr_run_hub_touched = time;
    if(!inside) localcmd("vr_runsetup hub\n");
    return TRUE;
};

// The engine's `vr_run_check <startweapon> <glasscannon> <meleeonly> <gore>` (vr_runsetup.cpp; the mutators' test):
// the player's loadout as Starting Weapon `w` gives it, a hit's damage as Glass Cannon `g` makes it, guns refused as
// Melee Only `m` says. 1: as expected (each miss printed).
float(float w, float g, float m) VR_Run_TestCheck =
{
    entity p = VR_Player();
    if(!p) { dprint("vr_run_check: no player\n"); return FALSE; }
    float ok = TRUE;
    float load;
    if(w == RUN_SW_SHOTGUN) load = ((p.items & IT_SHOTGUN) && (p.holsterweapon3 == WID_SHOTGUN));
    else if(w == RUN_SW_REVOLVER)
        load = ((p.holsterweapon3 == WID_REVOLVER) && (p.hipnotic_items & HIP_IT_REVOLVER) && !(p.items & IT_SHOTGUN));
    else if(w == RUN_SW_AXE) load = ((p.items & IT_AXE) && !(p.items & IT_SHOTGUN) && (p.holsterweapon3 == WID_FIST));
    else load = (!(p.items & (IT_AXE | IT_SHOTGUN)) && (p.holsterweapon2 == WID_FIST) && (p.holsterweapon3 == WID_FIST));
    if(!load) { ok = FALSE; bprint(sprintf("vr_run_check: loadout not start weapon %g (items %g, holsters %g %g)\n", w, p.items, p.holsterweapon2, p.holsterweapon3)); }
    if((VR_Run_Damage(p, world, 10) != (g ? 20 : 10)) || (VR_Run_Damage(world, p, 10) != 10))
    { ok = FALSE; bprint(sprintf("vr_run_check: a 10 hit does %g, glass cannon %g\n", VR_Run_Damage(p, world, 10), g)); }
    if((VR_Run_RefusesFire(WID_SHOTGUN) != m) || (VR_Run_RefusesPickup(WID_REVOLVER) != m) || VR_Run_RefusesFire(WID_AXE))
    { ok = FALSE; bprint(sprintf("vr_run_check: guns refused %g, melee only %g\n", VR_Run_RefusesFire(WID_SHOTGUN), m)); }
    return ok;
};
