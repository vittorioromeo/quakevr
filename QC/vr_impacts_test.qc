#ifdef QVR_TESTS
// vr_impact_qc_test: every hit ladder (TraceAttack's bullet, spawn_touchblood's nail, VR_HitBlood's blow,
// VR_HitNonFlesh's projectile pellet, VR_HitBloodSplash's splash, VR_Blunt_HitLayer's and VR_Melee_HitSoundOn's
// sounds) on every material (the world, wood, metal, flesh, armour, a wall torch, a grenade), each a target in a row 96
// units ahead of the player, with vr_debug_impacts on: each effect the server sends is printed ("impfx p2 ...",
// "impfx snd ..."), so that two builds' logs compare line by line (the impact table's move, design_review.md proposal
// 4; ROUND23.md "One impact-effect table"). 1-8: that ladder alone (the eyeshots: 1 bullet, 2 nail, 3 blow, 4 pellet,
// 5 splash, 6 blunt layer, 7 melee hit sounds, 8 the chainsaw's tick: VR_Saw_HitFx); 9: all of them, then
// "imptest: N calls" (the verdict line: 56). Seed sv_random_seed before the map for a log that repeats.

float VR_IMPTEST_TARGETS = 7;

// The test's target `i` (0 the world, then wood, metal, flesh, armour, a torch, a grenade): a bare entity of the
// class each material's predicate knows, nothing drawn, placed `xAt`.
entity(float i, vector xAt) VR_ImpTest_Target =
{
    if(i == 0)
    {
        return world;
    }
    entity e = spawn();
    e.classname = i == 1 ? "vr_crate_piece" : (i == 2 ? "misc_explobox" : (i == 3 ? "vr_imptest_flesh" :
                  (i == 4 ? "monster_knight" : (i == 5 ? "light_torch_small_walltorch" : "vr_imptest_grenade"))));
    if(i == 6)
    {
        e.vr_shootnade = 1;
    }
    e.takedamage = DAMAGE_YES;
    setorigin(e, xAt);
    return e;
};

float vr_imptest_calls;

// Ladder `k` (1-7) on target `t` at `at`, the shot coming from `xFrom` (the player).
void(float k, entity t, vector at, entity xFrom) VR_ImpTest_One =
{
    vector dir = normalize(at - xFrom.origin);
    vr_imptest_calls++;
    bprint(sprintf("imptest: ladder %g on %s\n", k, t == world ? "world" : t.classname));
    entity oself = self;
    if(k == 1)
    {
        ClearMultiDamage();
        trace_ent = t;
        trace_endpos = at;
        trace_plane_normal = -dir;
        TraceAttack(12, dir);
        ClearMultiDamage();
    }
    else if(k == 2)
    {
        entity m = spawn();
        m.classname = "vr_imptest_nail";
        setorigin(m, at - dir * 4);
        m.velocity = dir * 1000;
        trace_plane_normal = -dir;
        self = m;
        other = t;
        spawn_touchblood(9);
        self = oself;
        remove(m);
    }
    else if(k == 3)
    {
        VR_HitBlood(t, at, dir * -200, 20);
    }
    else if(k == 4)
    {
        float fl = VR_HitNonFlesh(t, at, dir, 15);
        bprint(sprintf("imptest: nonflesh %g\n", fl));
    }
    else if(k == 5)
    {
        VR_HitBloodSplash(t, at, 25);
    }
    else if(k == 6)
    {
        VR_Blunt_HitLayer(xFrom, CHAN_AUTO, t);
    }
    else if(k == 7)
    {
        bprint(sprintf("imptest: melee sound blade %s, blunt %s\n", VR_Melee_HitSoundOn(t, "player/axhit1.wav", TRUE),
            VR_Melee_HitSoundOn(t, "fisthit.wav", FALSE)));
    }
    else if(k == 8)
    {
        VR_Saw_HitFx(t, VR_MaterialOf(t), at, 4, 1);
    }
};

// Every server frame (StartFrame): the test asked for (vr_impact_qc_test).
void() VR_ImpTest_Frame =
{
    float ask = cvar("vr_impact_qc_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_impact_qc_test", "0");
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    float dbg = cvar("vr_debug_impacts");
    cvar_set("vr_debug_impacts", "1");
    makevectors('0 1 0' * p.angles_y);
    vector fwd = v_forward;
    vector right = v_right;
    entity targets[7];
    vector spots[7];
    float i, k;
    for(i = 0; i < VR_IMPTEST_TARGETS; i++)
    {
        spots[i] = p.origin + fwd * 96 + right * (i - 3) * 20 + '0 0 18';
        targets[i] = VR_ImpTest_Target(i, spots[i]);
    }
    vr_imptest_calls = 0;
    for(k = 1; k <= 8; k++)
    {
        if(ask == 9 || ask == k)
        {
            for(i = 0; i < VR_IMPTEST_TARGETS; i++)
            {
                makevectors('0 1 0' * p.angles_y);
                VR_ImpTest_One(k, targets[i], spots[i], p);
            }
        }
    }
    for(i = 1; i < VR_IMPTEST_TARGETS; i++)
    {
        remove(targets[i]);
    }
    cvar_set("vr_debug_impacts", ftos(dbg));
    bprint(sprintf("imptest: %g calls\n", vr_imptest_calls));
};
#endif
