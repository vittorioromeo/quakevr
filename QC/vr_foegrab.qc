// vr_foegrab.qc -- holding enemies (engine vr_foegrab.cpp, which holds them; ROUND21.md, "Holding enemies"; Combat >
// Holding Enemies). The engine calls these with self the player:
// - VR_FoeGrab_Strength(m): one hand's hold on m, 0..1 (two hands' together: 1 - (1 - a)(1 - b));
// - VR_FoeGrab_Taken(m, hand): a hand took hold of m: it is touched (it spots you, as the stealth AI's touch rule; asleep
//   or idle, it wakes hostile), and the hand feels it;
// - VR_FoeGrab_Released(m, hand, why): the hand let go (why: 0 the grip let go, 1 pulled too far, 2 it broke free, 3 it
//   died, was knocked down or moved away): a buzz when it was torn from the hand.
// The fields (.vr_foegrab_hands, .vr_foegrab_hold, .vr_foegrab_letgo) are in vr_handgrabutil.qc; the shove's part is in
// vr_enemyshove.qc (VR_EnemyShove_Hit), the melee's (no blows from a holding hand) in vr_melee.qc.

// How big m is to a hand holding it: 0 small (a grunt, an enforcer, a zombie, a knight, a mummy, a dog...), 1 big (a fiend,
// an ogre), 2 huge (a shambler, a vore, and bigger).
float(entity m) VR_FoeGrab_Size =
{
    string c = m.classname;
    if(c == "monster_shalrath" || c == "monster_super_wrath")
    {
        return 2; // (a vore stands as tall as an ogre, and roots itself)
    }
    float mass = VR_Grapple_MonsterMass(m);
    if(mass <= 150)
    {
        return 0;
    }
    if(mass <= 300)
    {
        return 1;
    }
    return 2;
};

float(entity m, float xHand, float xUnused) VR_FoeGrab_Strength =
{
    float s;
    if(cvar_hget(cvarh_vr_foegrab_by_mass))
    {
        float light = cvar_hget(cvarh_vr_foegrab_mass_light);
        float heavy = max(light + 1, cvar_hget(cvarh_vr_foegrab_mass_heavy));
        s = (heavy - VR_Grapple_MonsterMass(m)) / (heavy - light);
    }
    else
    {
        float size = VR_FoeGrab_Size(m);
        s = size == 0 ? cvar_hget(cvarh_vr_foegrab_strength_small) :
            (size == 1 ? cvar_hget(cvarh_vr_foegrab_strength_medium) : cvar_hget(cvarh_vr_foegrab_strength_large));
    }
    return min(1, max(0, s));
};

float(entity e) VR_IsDummy;

float(entity m, float xHand, float xUnused) VR_FoeGrab_Taken =
{
    haptic(xHand, 0, 0.08, 120, 0.6);
    // (not the training dummy, nor anything with no run to hunt with: FoundTarget would set its think to a null th_run,
    // "NULL function": NOTES.md vrfiringrange_2026-10-08_22-28-01)
    if(m.enemy == world && !VR_IsDummy(m) && m.th_run)
    {
        if(VR_Stealth_Applies(m))
        {
            VR_Stealth_Log(m, "touched (held)");
        }
        VR_Stealth_Spot(m, self);
    }
    return 0;
};

float(entity m, float xHand, float xWhy) VR_FoeGrab_Released =
{
    if(xWhy == 1 || xWhy == 2)
    {
        haptic(xHand, 0, 0.12, 60, 0.7); // torn from the hand
    }
    return 0;
};
