//
//
//
// ----------------------------------------------------------------------------
// Mutator: All Elites (vr_run_allelites; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// Every armed humanoid spawns an elite (vr_attrs.qc: the elite's stats, its dye and marker), in the campaigns and in
// Arena Mode: the map's, a replacement's, Enemy Count's copies, Randomized Enemies' swaps, the arena's waves. Never the
// tools' (the toolgun, the tests, the dummies: vr_agr_hold; the toolgun's page has its own Elite), deathmatch's.

float(entity e) VR_RunAllElites_Forces =
{
    if(!cvar_hget(cvarh_vr_run_allelites) || !e.vr_ag || deathmatch)
    {
        return FALSE;
    }
    return vr_agr_hold <= 0 || vr_ar_hold_roll; // (the arena's preset waves hold the replacements, not their elites)
};
