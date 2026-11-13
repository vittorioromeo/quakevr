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

// The engine's ED_LoadFromFile, a map's monster_ about to spawn while Enemy Count or Randomized Enemies is on (self: its
// keys parsed, its spawn function not yet run): Randomized Enemies' swap first (its classname), then how many copies of
// it Enemy Count asks for (0..2). Once the whole map has spawned, each copy is placed by VR_Run_MonsterCopy (self: the
// copy, the same keys parsed again; other: its monster; k 1..) before its spawn function runs (FALSE: no room, no copy).
// Not in the tutorial.
float() VR_Run_MonsterSpawning =
{
    if(VR_GameUtil_InTutorialMap()) return 0;
    self.vr_run_mapclass = self.classname;
    VR_RunEnemyRandom_Swap();
    self.vr_run_kind = self.classname;
    self.vr_run_kindflags = self.spawnflags;
    return VR_RunEnemyCount_Copies();
};

float(float k) VR_Run_MonsterCopy =
{
    entity orig = other;
    if(orig.vr_run_kind == "") return FALSE; // (gone: its edict another's now)
    self.vr_run_mapclass = self.classname;
    if(self.classname != orig.vr_run_kind)
    {
        VR_RunEnemyRandom_Become(orig.vr_run_kind, orig.vr_run_kindflags); // (its monster's swap, on the same keys)
    }
    self.vr_run_kind = self.classname;
    return VR_RunEnemyCount_Place(k);
};

// The engine's `vr_run_enemies_check <enemycount> <enemyrandom>` (vr_runsetup.cpp; the enemy mutators' test): the
// running map's monsters as those choices spawn them. 1: as expected (each miss printed).
float(float count, float random) VR_Run_EnemiesCheck =
{
    float ok = VR_RunEnemyRandom_Check(random);
    return VR_RunEnemyCount_Check(count) && ok;
};

float() VR_Run_EnemiesChains = { return VR_RunEnemyCount_Chains(); };

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
    float inside = ((time - vr_run_hub_touched) < 0.5);
    vr_run_hub_touched = time;
    if(!inside) localcmd("vr_runsetup hub\n");
    return TRUE;
};

// The engine's `vr_run_check <startgun> <startmelee> <glasscannon> <meleeonly> <gore>` (vr_runsetup.cpp; the mutators' test):
// the player's loadout as the Starting gun and melee weapon (`w` = gun * 10 + melee) give it, a hit's damage as Glass Cannon `g` makes it, guns refused as
// Melee Only `m` says. 1: as expected (each miss printed).
float(float w, float g, float m) VR_Run_TestCheck =
{
    entity p = VR_Player();
    if(!p) { dprint("vr_run_check: no player\n"); return FALSE; }
    float ok = TRUE;
        float sg = floor(w / 10);
    float sm = w - sg * 10;
    float load = TRUE;
    float noShotgun = !(p.items & IT_SHOTGUN);
    float noAxe = !(p.items & IT_AXE);
    float gunHolster = p.holsterweapon3;
    float meleeHolster = p.holsterweapon2;
    if(sg == RUN_SG_SHOTGUN)
    {
        load = ((!noShotgun) && (gunHolster == WID_SHOTGUN));
    }
    else if(sg == RUN_SG_NAILPISTOL)
    {
        load = (noShotgun && (gunHolster == WID_NAILPISTOL) && ((p.hipnotic_items & HIP_IT_NAILPISTOL) != 0));
        if(p.holsterweaponinst3.wi_clip != VR_NailPistol_MagSize()) load = FALSE;
        if(p.ammo_nails < 50) load = FALSE;
    }
    else if(sg == RUN_SG_REVOLVER)
    {
        load = (noShotgun && (gunHolster == WID_REVOLVER) && ((p.hipnotic_items & HIP_IT_REVOLVER) != 0));
    }
    else
    {
        load = (noShotgun && (gunHolster == WID_FIST));
    }
    if(sm == RUN_SM_AXE)
    {
        if(noAxe || meleeHolster != WID_AXE) load = FALSE;
    }
    else if(sm == RUN_SM_DAGGER)
    {
        if(!noAxe || meleeHolster != WID_SWORD || VR_ADMelee_Kind(p.holsterweaponflags2) != VR_SWORDKIND_DAGGER) load = FALSE;
    }
    else
    {
        if(!noAxe || meleeHolster != WID_FIST) load = FALSE;
    }
    if(!load) { ok = FALSE; bprint(sprintf("vr_run_check: loadout not start gun/melee %g (items %g, holsters %g %g)\n", w, p.items, p.holsterweapon2, p.holsterweapon3)); }
    if((VR_Run_Damage(p, world, 10) != (g ? 20 : 10)) || (VR_Run_Damage(world, p, 10) != 10))
    { ok = FALSE; bprint(sprintf("vr_run_check: a 10 hit does %g, glass cannon %g\n", VR_Run_Damage(p, world, 10), g)); }
    if((VR_Run_RefusesFire(WID_SHOTGUN) != m) || (VR_Run_RefusesPickup(WID_REVOLVER) != m) || VR_Run_RefusesFire(WID_AXE))
    { ok = FALSE; bprint(sprintf("vr_run_check: guns refused %g, melee only %g\n", VR_Run_RefusesFire(WID_SHOTGUN), m)); }
    return ok;
};
