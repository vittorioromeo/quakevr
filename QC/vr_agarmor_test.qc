// vr_agarmor_test.qc -- armour for armed humanoids' test (vr_agarmor.qc; Debug > Tests; ROUND23.md, "Armour for armed
// humanoids, P1"). `vr_agarmor_test 1`: two armed grunts ahead of the player (one armoured, one not) hit through T_Damage
// at their plates' places: the absorb math by both rules, a headshot with and without the helmet (and its pop), a
// break (the bit off, the debris a prop you can't force-grab, gone after its fade), a shotgun's pellets over two
// pieces, a cut stopped by a plate then let through once it broke, the unarmoured grunt's damage untouched, a gibbing
// shedding every plate, and the skill rolls over 200 spawns each. Ends on "agarmor test: N passed, M failed".
// `vr_agarmor_test 2`: three armed grunts ahead in full green, yellow and red sets (the eyeshots).

entity vr_art_a, vr_art_b; // the armoured grunt, the bare one
float vr_art_state;       // 0 idle, 1 the checks, 2 waiting for the debris to fade
float vr_art_pass, vr_art_fail;
float vr_art_until;       // state 2's give-up time
float vr_art_rule, vr_art_helm, vr_art_cuts, vr_art_dtime; // the settings put back after

void(float ok, string what) VR_ART_Check =
{
    if(ok)
    {
        vr_art_pass = vr_art_pass + 1;
        dprint(sprintf("agarmor test: PASS %s\n", what));
    }
    else
    {
        vr_art_fail = vr_art_fail + 1;
        dprint(sprintf("agarmor test: FAIL %s\n", what));
    }
};

entity(entity p, float side) VR_ART_Spawn =
{
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    entity oldself = self;
    self = spawn();
    setorigin(self, p.origin + fw * 128 + v_right * side);
    self.angles_y = vectoyaw(p.origin - self.origin);
    self.weapon = -1; // (its fists)
    VR_AM_Spawn(0);
    entity g = self;
    self = oldself;
    g.vr_ar_type = 0;
    g.vr_ar_mask = 0;
    g.health = 1000;
    return g;
};

// A hit of `d` at `p` (PositionalDamage's `part`): the health it took.
float(entity g, vector p, float part, float d) VR_ART_Hit =
{
    float h0 = g.health;
    vr_hitpart_target = g;
    vr_hitpart_time = time;
    vr_hitpart_pos = p;
    vr_hitpart = part;
    T_Damage(g, world, world, d);
    return h0 - g.health;
};

float(float m) VR_ART_Count =
{
    float n = 0;
    float i;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        n = n + ((m & VR_AgArmor_Bit(i)) != 0);
    }
    return n;
};

void() VR_ART_Run =
{
    entity a = vr_art_a;
    float i, d, hp, n;
    float f0 = a.frame; // (its pain frames move its arms: the places found in this one, so put back before each hit)
    vector pc = limbpiece(a, 0, 10);
    vector ph = limbpiece(a, 1, 10);
    vector pp = limbpiece(a, 2, 10);
    float c0 = ragdollbone(a, pc, QVR_ARMOR_PIECE_AT);
    float c1 = ragdollbone(a, ph, QVR_ARMOR_PIECE_AT);
    float c2 = ragdollbone(a, pp, QVR_ARMOR_PIECE_AT);
    VR_ART_Check(c0 == 0 && c1 == 1 && c2 == 2, sprintf("the pieces found at their places (cuirass %g, helmet %g, pauldron %g)", c0, c1, c2));

    // The player's rule: green saves ceil(0.3 x 10) = 3 from the cuirass's pool.
    cvar_set("vr_agarmor_rule", "0");
    VR_AgArmor_Give(a, 1, 1023);
    hp = a.vr_ar_hp[0];
    d = VR_ART_Hit(a, pc, 0, 10);
    VR_ART_Check(d == 7 && a.vr_ar_hp[0] == hp - 3, sprintf("rule 0: 10 on green cuirass takes %g (7), pool %g -> %g", d, hp, a.vr_ar_hp[0]));
    VR_AgArmor_Give(a, 3, 1023);
    d = VR_ART_Hit(a, pc, 0, 10);
    VR_ART_Check(d == 2, sprintf("rule 0: 10 on red takes %g (2)", d));

    // A plate blocks all: nothing through, then a hit past its pool breaks it.
    cvar_set("vr_agarmor_rule", "1");
    VR_AgArmor_Give(a, 1, 1023);
    hp = a.vr_ar_hp[0];
    d = VR_ART_Hit(a, pc, 0, 10);
    VR_ART_Check(d == 0 && a.vr_ar_hp[0] == hp - 10, sprintf("rule 1: 10 on the cuirass takes %g (0), pool %g", d, a.vr_ar_hp[0]));
    float made = vr_ar_debris_made;
    hp = a.vr_ar_hp[0];
    d = VR_ART_Hit(a, pc, 0, 60);
    VR_ART_Check(d == 60 - hp && !VR_AgArmor_On(a, 0) && a.vr_ar_mask == 1022,
        sprintf("rule 1: 60 past a pool of %g takes %g, the cuirass broken (mask %g)", hp, d, a.vr_ar_mask));
    entity deb = find(world, classname, "vr_ar_debris");
    VR_ART_Check(vr_ar_debris_made == made + 1 && deb != world && deb.model == "progs/vrgrunt.mdl#plate0" && deb.vr_rigid == 1 &&
        deb.movetype == MOVETYPE_BOUNCE && !(deb.flags & FL_FORCEGRABBABLE),
        sprintf("the broken cuirass falls as a physics prop (%s, rigid %g), not force-grabbable", deb.model, deb.vr_rigid));
    cvar_set("vr_agarmor_rule", "0");

    // Headshots: the helmet halves the multiplier's extra and stops the pop; without it, all of it and the pop stands.
    float m = cvar_hget(cvarh_vr_headshot_mult);
    float halve = cvar_hget(cvarh_vr_positional_damage) && m > 1;
    float dm = halve ? 10 * (1 + (m - 1) * 0.5) : 10 * m;
    VR_AgArmor_Give(a, 1, 1023);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = -1;
    d = VR_ART_Hit(a, ph, 1, 10 * m);
    VR_ART_Check(d == ceil(dm - ceil(0.3 * dm)) && vr_decap_ent == world,
        sprintf("headshot of %g on a helmet takes %g (%g), the pop stopped", 10 * m, d, ceil(dm - ceil(0.3 * dm))));
    VR_AgArmor_Give(a, 1, 1023 - 2);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = -1;
    d = VR_ART_Hit(a, ph, 1, 10 * m);
    VR_ART_Check(d == ceil(10 * m) && vr_decap_ent == a, sprintf("headshot of %g with no helmet takes %g, the pop stands", 10 * m, d));
    vr_decap_ent = world;

    // A shotgun's pellets: three on the cuirass, three on the left pauldron (4 each): both pools take ceil(0.3 x 12).
    VR_AgArmor_Give(a, 1, 1023);
    float h0 = a.vr_ar_hp[0];
    float h2 = a.vr_ar_hp[2];
    a.frame = f0;
    ClearMultiDamage();
    for(i = 0; i < 6; i++)
    {
        vr_hitpart_target = a;
        vr_hitpart_time = time;
        vr_hitpart_pos = (i < 3) ? pc : pp;
        vr_hitpart = 0;
        AddMultiDamage(a, 4);
    }
    float hb = a.health;
    ApplyMultiDamage();
    VR_ART_Check(a.vr_ar_hp[0] == h0 - 4 && a.vr_ar_hp[2] == h2 - 4 && hb - a.health == 16,
        sprintf("pellets: cuirass %g -> %g, pauldron %g -> %g, health took %g (16)", h0, a.vr_ar_hp[0], h2, a.vr_ar_hp[2], hb - a.health));

    // A blade's cut under the pauldron: stopped; once it broke, let through.
    VR_AgArmor_Give(a, 1, 1023);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = 0;
    vr_decap_at = pp;
    a.frame = f0;
    VR_ART_Hit(a, pp, 2, 1);
    VR_ART_Check(vr_decap_ent == world, "a cut under the unbroken pauldron stopped");
    a.vr_ar_hp[2] = 1;
    a.frame = f0;
    VR_ART_Hit(a, pp, 2, 10);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = 0;
    vr_decap_at = pp;
    a.frame = f0;
    VR_ART_Hit(a, pp, 2, 1);
    VR_ART_Check(!VR_AgArmor_On(a, 2) && vr_decap_ent == a, "a cut once the pauldron broke let through");
    vr_decap_ent = world;

    // The bare grunt: the whole of it.
    d = VR_ART_Hit(vr_art_b, limbpiece(a, 0, 10) - a.origin + vr_art_b.origin, 0, 10);
    float d2 = VR_ART_Hit(vr_art_b, vr_art_b.origin, 1, 13);
    VR_ART_Check(d == 10 && d2 == 13 && vr_art_b.vr_ar_type == 0, sprintf("an unarmoured grunt takes all of it (%g of 10, %g of 13)", d, d2));

    // A gibbing sheds every plate left.
    VR_AgArmor_Give(a, 2, 1023 - 1 - 16);
    made = vr_ar_debris_made;
    VR_AgArmor_Shed(a);
    VR_ART_Check(vr_ar_debris_made == made + 8 && a.vr_ar_mask == 0, sprintf("gibbed: %g plates shed (8)", vr_ar_debris_made - made));

    // The skill rolls: 200 spawns each.
    float sk;
    for(sk = 0; sk < 4; sk++)
    {
        string base = strcat("vr_agarmor_skill", ftos(sk));
        float chance = cvar(base);
        float best = cvar(strcat(base, "_type"));
        float want = cvar(strcat(base, "_pieces"));
        float got = 0;
        float bad = 0;
        for(i = 0; i < 200; i++)
        {
            VR_AgArmor_Roll(sk, a.origin + [i * 37, i * 11, 0]);
            if(vr_ar_roll_type > 0)
            {
                got = got + 1;
                n = VR_ART_Count(vr_ar_roll_mask);
                bad = bad + (vr_ar_roll_type > best || n != want || !(vr_ar_roll_mask & 1));
            }
        }
        VR_ART_Check(fabs(got / 200 - chance) <= 0.05 && bad == 0,
            sprintf("skill %g: %g of 200 armoured (chance %g), %g of them off the cap or the count", sk, got, chance, bad));
    }
};

void() VR_AgArmor_TestFrame =
{
    if(vr_art_state == 0)
    {
        if(cvar("vr_agarmor_test") == 0)
        {
            return;
        }
        float request = cvar("vr_agarmor_test");
        cvar_set("vr_agarmor_test", "0");
        entity p = find(world, classname, "player");
        if(!p)
        {
            return;
        }
        if(request == 2)
        {
            // (the eyeshots: green, yellow and red full sets side by side, facing the player)
            VR_AgArmor_Give(VR_ART_Spawn(p, -44), 1, 1023);
            VR_AgArmor_Give(VR_ART_Spawn(p, 0), 2, 1023);
            VR_AgArmor_Give(VR_ART_Spawn(p, 44), 3, 1023);
            return;
        }
        vr_art_pass = 0;
        vr_art_fail = 0;
        vr_art_rule = cvar("vr_agarmor_rule");
        vr_art_helm = cvar("vr_agarmor_helmet_head");
        vr_art_cuts = cvar("vr_agarmor_block_cuts");
        vr_art_dtime = cvar("vr_agarmor_debris_time");
        cvar_set("vr_agarmor_helmet_head", "1");
        cvar_set("vr_agarmor_block_cuts", "1");
        cvar_set("vr_agarmor_debris_time", "0.5");
        vr_art_a = VR_ART_Spawn(p, -40);
        vr_art_b = VR_ART_Spawn(p, 40);
        vr_art_state = 1;
        return; // (their poses first drawn)
    }
    if(vr_art_state == 1)
    {
        VR_ART_Run();
        vr_art_state = 2;
        vr_art_until = time + 5;
        return;
    }
    // The debris fades out (vr_agarmor_debris_time 0.5, then a second's fade).
    if(find(world, classname, "vr_ar_debris") != world && time < vr_art_until)
    {
        return;
    }
    VR_ART_Check(find(world, classname, "vr_ar_debris") == world, "the debris gone after its fade (else timed out)");
    remove(vr_art_a);
    remove(vr_art_b);
    cvar_set("vr_agarmor_rule", ftos(vr_art_rule));
    cvar_set("vr_agarmor_helmet_head", ftos(vr_art_helm));
    cvar_set("vr_agarmor_block_cuts", ftos(vr_art_cuts));
    cvar_set("vr_agarmor_debris_time", ftos(vr_art_dtime));
    vr_art_state = 0;
    dprint(sprintf("agarmor test: %g passed, %g failed\n", vr_art_pass, vr_art_fail));
};
