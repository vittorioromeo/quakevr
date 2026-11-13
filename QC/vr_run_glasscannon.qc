//
//
//
// ----------------------------------------------------------------------------
// Mutator: Glass Cannon (vr_run_glasscannon; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// The player and the monsters take double damage (T_DamageImpl's start, so a hand's blow too: VR_Run_Damage): every hit, blast, fall and burn.
// Props, doors and other breakables take their usual.

float(entity targ, float damage) VR_RunGlassCannon_Damage =
{
    if(!cvar_hget(cvarh_vr_run_glasscannon)) return damage;
    if(!(targ.flags & (FL_CLIENT | FL_MONSTER))) return damage;
    return damage * 2;
};
