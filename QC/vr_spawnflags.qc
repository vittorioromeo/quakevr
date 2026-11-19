// vr_spawnflags.qc -- whose meaning a map's spawnflags have (docs/vr-port/ARCANE_DIMENSIONS.md, section 16, "Spawnflags";
// decision 19). Early in progs.src: Quake VR's files and AD's bridge read flags through it.
//
// A spawnflag bit means what the ruleset the map (or the entity) was made for says: Quake's (with Hipnotic's, Rogue's
// and Honey's own meanings on their own classnames: id1's ruleset, the same bits as ever), Machine Games' (Dimension of
// the Past, of the Machine, Dawn of the Machine) or Arcane Dimensions'. The same bit is a statue in Rogue, a trigger
// spawn in Hipnotic, AD's "no spawn effect" on a monster and MG's own. So:
// - one ruleset a map (VR_Ruleset, vr_packutil.qc: the engine's, its campaign's or its signals') and one an
//   entity (VR_SF_Ruleset: AD's own entities AD's in any map, their own classnames; the rest the map's);
// - flags with a meaning shared by every ruleset (AMBUSH 1 on monsters, the skill and deathmatch bits the engine reads)
//   are read as they are;
// - the Quake-family meanings (Rogue's statues, Hipnotic's and Honey's flags, id's monster variants) are read only
//   where the ruleset is Quake's: the existing raw tests are gated by VR_OtherFlags (76 gates, unchanged);
// - every other ruleset's meaning is named (VR_SF_*) and read by VR_SF_Has, which answers from that ruleset's table
//   (VR_SF_Bit): a name a ruleset has no flag for is never set there. AD's flags on Quake VR's own entities (its
//   delayed and angry monster spawns, its items' STARTOFF and RESPAWN, its exits' inventory reset) are read so, only
//   in AD maps.
// Backwards compatible: Quake's, Hipnotic's, Rogue's and MG's maps read exactly the bits they did (their rulesets'
// tables add nothing); the A/B spawn census proves it (DWELL.md section 8). A new ruleset (Quoth, Copper) is a row in
// the engine's table (vr_rules.cpp) and a column here, no gate anywhere else.

// The named meanings of rulesets other than Quake's (VR_SF_Has).
float VR_SF_MON_DELAYED = 1;    // a monster waits, unseen, for its trigger to appear (AD 64, Copper 16)
float VR_SF_MON_ANGRY = 2;      // and appears hunting whoever triggered it (AD 128, Copper 128)
float VR_SF_MON_NOSPAWNFX = 3;  // and appears without the teleport flash and sound (AD 32, Copper 32)
float VR_SF_STARTOFF = 4;       // an item or an exit starts off, until triggered (AD 64)
float VR_SF_ITEM_RESPAWN = 5;   // an item comes back after it is taken (AD 16)
float VR_SF_EXIT_RESETINV = 6;  // an exit resets the inventory (AD's trigger_changelevel 2)
float VR_SF_COOP_ONLY = 7;      // an entity spawns only in coop (MG 32768, Copper 4096: VR_SF_SpawnInhibited)
float VR_SF_NOT_COOP = 8;       // and not in coop (MG 131072, Copper 8192)
float VR_SF_MON_NOTELEFRAG = 9; // a delayed monster appears without telefragging what stands there (Copper 64)

// Whose meaning entity `e`'s spawnflags have: AD's own entities (AD's code runs them: its classnames) AD's in any map;
// the rest the map's.
float(entity e) VR_SF_Ruleset =
{
    if(AD_Run(e))
    {
        return VR_RULES_AD;
    }
    return VR_Ruleset();
};

// The bit meaning `m` has in ruleset `r` (VR_RULES_*; 0: none there).
float(float r, float m) VR_SF_Bit =
{
    if(r == VR_RULES_AD) // (QC/ad/ad_defscustom.qc: MON_SPAWN_*, ENT_STARTOFF, ITEM_RESPAWN; ad_triggers.qc TRIG_CMAPRESETINV)
    {
        if(m == VR_SF_MON_DELAYED) return 64;
        if(m == VR_SF_MON_ANGRY) return 128;
        if(m == VR_SF_MON_NOSPAWNFX) return 32;
        if(m == VR_SF_STARTOFF) return 64;
        if(m == VR_SF_ITEM_RESPAWN) return 16;
        if(m == VR_SF_EXIT_RESETINV) return 2;
    }
    else if(r == VR_RULES_MG) // (the re-release's monsters.qc/subs.qc: not in single player, not in coop)
    {
        if(m == VR_SF_COOP_ONLY) return 32768;
        if(m == VR_SF_NOT_COOP) return 131072;
    }
    else if(r == VR_RULES_COPPER) // (Copper's constants.qc SPAWN_*; inert until Copper's maps are detected: DWELL.md P1b)
    {
        if(m == VR_SF_MON_DELAYED) return 16;
        if(m == VR_SF_MON_ANGRY) return 128;
        if(m == VR_SF_MON_NOSPAWNFX) return 32;
        if(m == VR_SF_MON_NOTELEFRAG) return 64;
        if(m == VR_SF_COOP_ONLY) return 4096;
        if(m == VR_SF_NOT_COOP) return 8192;
    }
    return 0;
};

// The one spawn filter (called first in a spawn function): an entity the map's ruleset keeps out of this game is
// removed (TRUE): its coop bits (VR_SF_COOP_ONLY, VR_SF_NOT_COOP), and Dawn of the Machine's rune rule (vr_mg3_defs.qc).
// The engine already dropped the skill and deathmatch bits every ruleset shares (ED_LoadFromFile).
float() VR_SF_SpawnInhibited =
{
    if(MG3_RuneRemoved()) return TRUE; // (MG3's campaign only)
    float r = VR_Ruleset();
    if((!coop && (self.spawnflags & VR_SF_Bit(r, VR_SF_COOP_ONLY))) || (coop && (self.spawnflags & VR_SF_Bit(r, VR_SF_NOT_COOP))))
    {
        remove(self);
        return TRUE;
    }
    return FALSE;
};

// Whether entity `e`'s spawnflags carry meaning `m` (VR_SF_*) by its ruleset.
float(entity e, float m) VR_SF_Has =
{
    float b = VR_SF_Bit(VR_SF_Ruleset(e), m);
    return b != 0 && (e.spawnflags & b) != 0;
};

// The map's spawnflags mean another ruleset's flags (MG's, AD's): the Quake-family meanings Quake VR's monsters and
// entities give them are off (statues, trigger spawns, silent wake-ups...): the 76 VR_OtherFlags gates (decision 19).
float() VR_OtherFlags =
{
    return VR_Ruleset() != VR_RULES_ID;
};
