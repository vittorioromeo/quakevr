// vr_copper_bridge.qc -- Copper's rules in a Copper map (DWELL.md P1b; declarations in vr_copper_defs.qc). After
// Copper's imported files (QC/copper, cu_<name>), so it sees them.
//
// Vittorio's decision (DWELL.md 11.1): our weapons and our vanilla monsters everywhere, with Copper's keys, spawnflags
// and numbers; Copper's code runs the world logic (triggers, doors, trains, targets: vr_rules.cpp spawns its version of
// those classnames). So:
// - damage: Copper's T_Damage on one of ours (the player, our monsters, props) is our T_Damage (VR_CU_DamageOurs, a
//   reviewed patch); ours on one of Copper's (its explobox, shootable doors and buttons) is Copper's (CU_Damage);
// - the world: Copper's world setup after ours (its globals, light styles, key style, fog keys) and what its own
//   StartFrame did (CU_Frame);
// - our monsters: Copper's delayed, angry and quiet spawns (vr_rules_monsters.qc) and its coop bits
//   (VR_SF_SpawnInhibited), read through the spawnflag table (vr_spawnflags.qc);
// - fog: Copper's fog commands go through our svc_fog path (MG_SetFog, a reviewed patch), as MG's and AD's;
// - target_autosave: our autosave (VR_CU_Autosave, a reviewed patch).

// Copper's code owns `e`'s damage and death: its own damageable entities (not a player, none of our monsters).
float(entity e) CU_Run =
{
    return !(e.flags & FL_CLIENT) && (e.cu_th_die || e.cu_th_pain);
};

float(entity targ, entity inflictor, entity attacker, float damage) VR_CU_DamageOurs =
{
    if(CU_Run(targ))
    {
        return FALSE;
    }
    if(targ.takedamage && damage > 0)
    {
        T_Damage(targ, inflictor, attacker, damage);
    }
    return TRUE;
};

float(entity targ, entity inflictor, entity attacker, float damage) CU_Damage =
{
    if(!targ.takedamage)
    {
        return FALSE;
    }
    cu_T_Damage(targ, inflictor, attacker, damage, 0);
    return TRUE;
};

void() CU_WorldSpawn =
{
    if(!CU_Rules())
    {
        return;
    }
    cu_worldspawn(); // (self is the world)
};

void() CU_Frame =
{
    if(!CU_Rules())
    {
        return;
    }
    if(cu_initCleanup)
    {
        cu_initCleanup = cu_initCleanup - 1; // (a loaded game's settling frames: Copper's skill buttons wait for them)
    }
    if(cu_skill != skill)
    {
        cu_skill = skill;
        entity oself = self;
        entity b = cu_skill_head;
        while(b)
        {
            self = b;
            cu_skillbutton_activate();
            b = b.owner;
        }
        self = oself;
    }
    cu_framecount = cu_framecount + 1;
};

void() VR_CU_Autosave =
{
    if(!deathmatch && !coop)
    {
        localcmd("vr_autosave_now\n");
    }
};
