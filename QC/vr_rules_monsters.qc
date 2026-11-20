// vr_rules_monsters.qc -- our monsters in a map of another ruleset (AD's, Copper's): its delayed spawn, angry spawn,
// spawn effect and telefrag flags, read by their named meanings (vr_spawnflags.qc VR_SF_Bit), one adapter for every
// ruleset (DWELL.md P1b; it was AD's, ARCANE_DIMENSIONS.md 15.1). After AD's bridge (AD's keys); Copper's keys on our
// monsters are vr_copper_bridge.qc's.

// The delayed monster appears: its start run, and, angry, it goes for whoever triggered it.
void() VR_SF_MonsterRunStart =
{
    entity oldact = activator;
    activator = self.MG_monster_activator;
    self.MG_monster_start();
    if(VR_SF_Has(self, VR_SF_MON_ANGRY)) monster_use();
    activator = oldact;
};

// Its trigger fired: it becomes solid and visible where it stood (as MG's deferred ones), telefragging what stands
// there (not with Copper's 64), with the spawn effect (not with 32).
void() VR_SF_MonsterActivate =
{
    AD_SyncActivator();
    if(!self.MG_deferred) return;
    self.MG_deferred = FALSE;
    self.solid = self.MG_monster_solid;
    self.movetype = self.MG_monster_move;
    setmodel(self, self.MG_monster_model);
    setsize(self, self.MG_monster_mins, self.MG_monster_maxs);
    self.flags = self.flags | FL_MONSTER;
    self.MG_monster_activator = activator;
    if(!VR_SF_Has(self, VR_SF_MON_NOTELEFRAG)) spawn_tdeath(self.origin, self);
    if(!VR_SF_Has(self, VR_SF_MON_NOSPAWNFX)) spawn_tfog(self.origin);
    self.think = VR_SF_MonsterRunStart;
    self.nextthink = time + 0.1;
};

// The guard: one of our monsters starts with its behaviour functions set. A NULL one, called by our AI, the engine
// (a think set to it) or another ruleset's code, ends the game on "PR_ExecuteProgram: NULL function" with no QC stack;
// here it is a clear error line naming the monster and the function, at its spawn. FALSE: one is missing.
float(entity e) VR_Rules_MonsterThCheck =
{
    string missing = "";
    if(!e.th_stand) missing = strcat(missing, " th_stand");
    if(!e.th_walk) missing = strcat(missing, " th_walk");
    if(!e.th_run) missing = strcat(missing, " th_run");
    if(!e.th_die) missing = strcat(missing, " th_die");
    if(missing == "")
    {
        return TRUE;
    }
    bprint(sprintf("VR_Rules: QC error: %s at %v has no%s (its spawn function or an adapter left it unset)\n",
        e.classname, e.origin, missing));
    return FALSE;
};

// Another ruleset's path_corner (AD's ad_path_corner, Copper's cu_path_corner) touched by one of our monsters: ours
// (t_movetarget) moves it on. Their touches set `think` to the mod's own stand field (ad_th_stand, cu_th_stand), NULL
// on our monsters: a patrolling wizard of ours on Copper's corners ended d1m7, d1m8 and d2m8 on "NULL function".
// TRUE: done.
float() VR_Rules_PathCornerTouch =
{
    if(!(other.flags & FL_MONSTER) || AD_Run(other) || CU_Run(other))
    {
        return FALSE;
    }
    t_movetarget();
    return TRUE;
};

// walk/fly/swimmonster_start (monsters.qc), in a map whose ruleset has a delayed spawn (AD 64, Copper 16): AD's keys
// (nomonstercount), and the delayed spawn. TRUE: deferred (invisible, not solid, until used).
float(void() start) VR_SF_MonsterPrepare =
{
    VR_Rules_MonsterThCheck(self);
    VR_RN_MonsterNumbers(self); // the ruleset's health for our vanilla monster (vr_rules_numbers.qc)
    if(!VR_SF_Bit(VR_Ruleset(), VR_SF_MON_DELAYED))
    {
        return FALSE; // (Quake's and MG's rulesets: MG_MonsterPrepare's own)
    }
    if(AD_Rules() && self.ad_nomonstercount)
    {
        total_monsters = total_monsters - 1;
    }
    if(!VR_SF_Has(self, VR_SF_MON_DELAYED))
    {
        return FALSE;
    }
    self.MG_monster_start = start;
    self.MG_monster_model = self.model;
    self.MG_monster_mins = self.mins;
    self.MG_monster_maxs = self.maxs;
    self.MG_monster_solid = self.solid;
    self.MG_monster_move = self.movetype;
    self.MG_deferred = TRUE;
    self.use = VR_SF_MonsterActivate;
    self.nextthink = 0;
    self.think = SUB_Null;
    self.takedamage = DAMAGE_NO;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.flags = self.flags - (self.flags & FL_MONSTER);
    setmodel(self, "");
    return TRUE;
};
