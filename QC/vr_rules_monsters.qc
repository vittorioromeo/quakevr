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

// walk/fly/swimmonster_start (monsters.qc), in a map whose ruleset has a delayed spawn (AD 64, Copper 16): AD's keys
// (nomonstercount), and the delayed spawn. TRUE: deferred (invisible, not solid, until used).
float(void() start) VR_SF_MonsterPrepare =
{
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
