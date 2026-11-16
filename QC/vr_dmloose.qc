// vr_dmloose.qc -- deathmatch's loose gibs and dropped weapons capped (MP_PLAN B13b).
//
// Sixteen players dying a few times a second each leave their gibs and heads (lying 30-45 s: VR_Gib_FadeAnew) and the
// weapons they held (10 s in deathmatch: wpnthrow_think) about the map: mp_soak counted ~450 gibs and ~340 weapon props
// awake in Box3D, which took ~60% of the server's frame, their touches most of the QuakeC's. In deathmatch, once a frame,
// the oldest of them go first while there are more than Most Loose Gibs (vr_dm_gibs_max) and Most Dropped Weapons
// (vr_dm_weapons_max); 0: no cap. Nothing in a hand, on a blade or being force-pulled goes. Single player and coop are
// as they were. The small gibs (vr_smallgibs_max) and the limbs (vr_limbs_max) have caps of their own.

float vr_dml_gibs_gone;    // gibs removed by the cap (the tests, vr_dm_test 7)
float vr_dml_weapons_gone; // weapons removed by the cap

// A loose gib or head the cap counts: not a player (a gibbed player's head is the player), nor a body in the queue, nor a
// small gib or a limb (their own caps), nor one held, skewered or being pulled.
float(entity g) VR_DMLoose_GibCounts =
{
    return !(g.flags & FL_CLIENT) && g.classname != "bodyque" && !g.vr_sgib && g.classname != "vr_limb" &&
           g.carry_player == world && g.vr_skewer_p == world && g.fg_state == 0;
};

// A weapon lying (or flying) loose the cap counts: not one being pulled, nor one deathmatch's 10 s already took.
float(entity w) VR_DMLoose_WeaponCounts =
{
    return w.carry_player == world && w.fg_state == 0 && time <= GetThrownWeaponTimeout(w);
};

// The oldest loose gib the cap counts (world: none), and how many there are (vr_dml_count).
float vr_dml_count;
entity() VR_DMLoose_OldestGib =
{
    entity oldest = world;
    vr_dml_count = 0;
    for(entity g = findflags(world, vr_gib, 1); g; g = findflags(g, vr_gib, 1))
    {
        if(VR_DMLoose_GibCounts(g))
        {
            vr_dml_count++;
            if(oldest == world || g.vr_gib_made < oldest.vr_gib_made)
            {
                oldest = g;
            }
        }
    }
    return oldest;
};

// The oldest loose weapon (its deathmatch removal time the soonest: each is made with it 10 s on), and how many.
entity() VR_DMLoose_OldestWeapon =
{
    entity oldest = world;
    vr_dml_count = 0;
    for(entity w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
    {
        if(VR_DMLoose_WeaponCounts(w))
        {
            vr_dml_count++;
            if(oldest == world || GetThrownWeaponTimeout(w) < GetThrownWeaponTimeout(oldest))
            {
                oldest = w;
            }
        }
    }
    return oldest;
};

// StartFrame: in deathmatch, the oldest over each cap go (at the frame's start: nothing is using them).
void() VR_DMLoose_Frame =
{
    if(!deathmatch)
    {
        return;
    }
    float most = floor(cvar_hget(cvarh_vr_dm_gibs_max));
    if(most > 0)
    {
        entity g = VR_DMLoose_OldestGib();
        while(g && vr_dml_count > most)
        {
            VR_Head_StopFlies(g);
            g.vr_gib = FALSE;
            remove(g);
            vr_dml_gibs_gone++;
            g = VR_DMLoose_OldestGib();
        }
    }
    most = floor(cvar_hget(cvarh_vr_dm_weapons_max));
    if(most > 0)
    {
        entity w = VR_DMLoose_OldestWeapon();
        while(w && vr_dml_count > most)
        {
            VR_Saw_PropStop(w); // (as wpnthrow_think's deathmatch removal)
            remove(w);
            vr_dml_weapons_gone++;
            w = VR_DMLoose_OldestWeapon();
        }
    }
};
