// Isolated regression fixtures: not part of the production progs.src.
// prepare_review_regressions.py installs test-only wall-torch hooks 99/98/97.
float(string extension) checkextension = #99;

void() ReviewProbe =
{
    checkextension("DP_QC_SPRINTF"); // engine records the check even when it does not advertise the extension
    cvar_set("vr_melee_positional", "1");
    cvar_set("vr_positional_damage", "1");
    cvar_set("vr_hit_head_priority", "1");
    cvar_set("vr_damage_to_enemies", "1");
    cvar_set("vr_hit_tolerance_melee", "0");
    cvar_set("vr_legshot_mult", "0.6");
    entity z = spawn();
    z.classname = "monster_zombie";
    z.health = 60;
    z.th_pain = zombie_pain;
    z.th_die = zombie_die;
    z.inpain = 2;
    z.flags = FL_MONSTER;
    z.takedamage = DAMAGE_AIM;
    z.solid = SOLID_SLIDEBOX;
    setorigin(z, '5000 5000 5000');
    setsize(z, '-16 -16 -24', '16 16 32');
    // No model needed here: the fallback leg rule gives the same x0.60 as a model-mapped leg.
    float mult = VR_Melee_Positional(z, z.origin - '0 0 16', '-1 0 0');
    dprint(sprintf("REVIEW zombie leg mult %g\n", mult));
    for(float i = 0; i < 16; i++)
    {
        float boosted = VR_Zombie_SawHurt(z, 8 * mult);
        float dealt = boosted;
        T_DamageImpl(z, self, self, dealt, 1);
        dprint(sprintf("REVIEW zombie tick %g boosted %g dealt %g sum %g health %g\n", i+1, boosted, dealt, z.vr_saw_zombie_sum, z.health));
        if(z.health <= 0) { break; }
    }
    dprint(sprintf("REVIEW downed zombie final health %g (must be <=0)\n", z.health));
    if(z.health > 0) { error("REVIEW FAIL: reduced-damage chainsaw never gibbed the downed zombie"); }
    dprint(sprintf("REVIEW body contact mult %g (must stay 1)\n", VR_Melee_Positional(z, z.origin, '-1 0 0')));
    remove(z);

    entity m = spawn();
    m.classname = "monster_army";
    m.health = 1000;
    m.flags = FL_MONSTER;
    m.solid = SOLID_SLIDEBOX;
    setmodel(m, "progs/soldier.mdl");
    setsize(m, '-16 -16 -24', '16 16 32');
    setorigin(m, '5000 5000 5000');
    float found = 0;
    for(float pose = 0; pose < 102; pose++)
    {
        m.frame = pose;
        for(float h = 13; h <= 33; h = h + 2)
        {
            vector a = m.origin + '48 0 0' + '0 0 1' * h;
            vector b = m.origin + '-24 0 0' + '0 0 1' * h;
            float t = hitmodel_segment(m, a, b, 0, HITCLASS_MELEE);
            if(t < 0) { continue; }
            vector at = a + (b-a)*t;
            float region = PositionalRegion(m, at, b-a);
            float shot = PositionalDamageFactor(region);
            float melee = VR_Melee_Positional(m, at, b-a);
            if(melee != shot)
            {
                dprint(sprintf("REVIEW animated disagreement frame %g height %g contact %v rest %v region %g shot %g melee %g\n", pose, h, at-m.origin, hitmodel_rest(m,at)-m.origin, region, shot, melee));
                found++;
            }
        }
    }
    dprint(sprintf("REVIEW animated disagreements %g\n", found));
    if(found) { error("REVIEW FAIL: shot/melee zones disagree at an animated contact"); }
    remove(m);
    if(pointcontents('232 1386 24') != CONTENT_EMPTY) { error("REVIEW FAIL: slipgate is a liquid medium"); }
    dprint(sprintf("REVIEW slipgate effective %g entry %v\n", pointcontents('232 1386 24'), liquidentry('232 1360 24', '232 1410 24')));
    watersplash('232 1384 24', '0 1 0', 12, QVR_SPLASH_SHOT);
    dprint("REVIEW slipgate forced splash must be suppressed\n");
    dprint(sprintf("REVIEW real water effective %g (must be -3)\n", pointcontents('1408 1728 92')));
    watersplash('1408 1728 112', '0 0 -1', 12, QVR_SPLASH_SHOT);

};


void() ReviewTorch =
{
    entity t = spawn();
    t.classname = "review_destination_torch";
    setmodel(t, "progs/flame.mdl");
    setorigin(t, '544 1600 36');
    t.solid = SOLID_NOT;
    t.movetype = MOVETYPE_NONE;
    dprint(sprintf("REVIEW destination torch %g at %v\n", 1, t.origin));
};

void() ReviewExitBlock =
{
    entity e = spawn();
    e.classname = "review_exit_block";
    e.solid = SOLID_BBOX;
    e.movetype = MOVETYPE_NONE;
    setmodel(e, "progs/soldier.mdl");
    setsize(e, '-16 -16 -24', '16 16 32');
    setorigin(e, '544 1540 24');
    dprint("REVIEW blocked exit at 544 1540 24\n");
};
