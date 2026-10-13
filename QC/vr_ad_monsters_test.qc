// Arcane Dimensions' monsters' tests (vr_ad_mtest N, developer 1; Debug > Tests > Arcane Dimensions Monsters):
// docs/vr-port/ARCANE_DIMENSIONS.md section 17. One "admtest:" line each. With the debug spawner (vr_test_spawn 200..,
// impulse 241: vr_ad_monsters.qc VR_AD_DispenserSpawn) and the other tests' commands (vr_knockdown_test, vr_decap_test,
// vr_limbs_test, vr_ragdoll_list) the scripts check each model: Misc/quakevr/ad_monsters_test.sh.
//   1 the map's AD monsters by class: how many, registered (vr_ad_mon), rigged (canragdoll), alive
//   2 the nearest AD monster's state: class, model, its data, its ragdoll, knockdown, head, limbs, corpse, solid, frame
//   3 the nearest killed by a blow of its health (dies whole); 4 hit for 1000 (a corpse: gibbed); 5 the nearest awake
//   one's neck snapped (VR_FoeGrab_NeckSnap)

.float vr_ad_tested; // (AD_MT_Report: counted with its class)

void() AD_MT_Report =
{
    entity e, f;
    float n = 0;
    for(e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || !e.ad_classtype || e.vr_ad_tested)
        {
            continue;
        }
        float count = 0, reg = 0, rig = 0, alive = 0;
        for(f = e; f; f = nextent(f))
        {
            if((f.flags & FL_MONSTER) && f.ad_classtype && f.classname == e.classname)
            {
                f.vr_ad_tested = 1;
                count++;
                if(f.vr_ad_mon) { reg++; }
                if(canragdoll(f)) { rig++; }
                if(f.health > 0) { alive++; }
            }
        }
        n++;
        dprint(sprintf("admtest: %s %g registered %g rigged %g alive %g model %s\n", e.classname, count, reg, rig, alive,
            e.model));
    }
    for(e = nextent(world); e; e = nextent(e))
    {
        e.vr_ad_tested = 0;
    }
    dprint(sprintf("admtest: %g AD monster classes\n", n));
};

// The AD monster nearest the player (drawn: not gibbed away), alive or dead.
entity() AD_MT_Find =
{
    entity p = find(world, classname, "player");
    entity best = world;
    float bestd = 100000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if((e.flags & FL_MONSTER) && e.ad_classtype && e.modelindex && vlen(e.origin - p.origin) < bestd)
        {
            best = e;
            bestd = vlen(e.origin - p.origin);
        }
    }
    if(best == world)
    {
        dprint("admtest: no AD monster\n");
    }
    return best;
};

void() AD_MT_Nearest =
{
    entity p = find(world, classname, "player");
    entity m = AD_MT_Find();
    if(m == world)
    {
        return;
    }
    float head = VR_AD_PositionalHead(m);
    dprint(sprintf("admtest: near %s model %s mon %g head %g", m.classname, m.model, m.vr_ad_mon, head));
    dprint(sprintf(" (%g %g %g) headmdl %s", m.vr_ad_headf, m.vr_ad_headu, m.vr_ad_headr, VR_Decap_HeadModel(m)));
    dprint(sprintf(" corpse %g kd %g rig %g\n", m.vr_ad_corpseh != 0, m.vr_kd_chance_h != 0, canragdoll(m)));
    dprint(sprintf("admtest: state health %g ragdoll %g knockdown %g headless %g limbcut %g", m.health, isragdoll(m),
        m.vr_knockdown, m.vr_headless, m.vr_limbcut));
    dprint(sprintf(" corpse %g dying %g solid %g frame %g gibbed %g", m.vr_corpse, m.vr_corpse_dying, m.solid, m.frame,
        m.ad_gibbed));
    dprint(sprintf(" flags %g away %.0f\n", m.flags, vlen(m.origin - p.origin)));
};

void() AD_MonstersTestFrame =
{
    float request = cvar("vr_ad_mtest");
    if(!request)
    {
        return;
    }
    cvar_set("vr_ad_mtest", "0");
    if(request == 1)
    {
        AD_MT_Report();
    }
    else if(request == 2)
    {
        AD_MT_Nearest();
    }
    else if(request == 3 || request == 4)
    {
        // 3: the nearest killed by a blow of its health (it dies whole); 4: hit for 1000 (a corpse gibbed: gone).
        entity p = find(world, classname, "player");
        entity m = AD_MT_Find();
        if(m != world)
        {
            entity oldSelf = self;
            self = p;
            T_Damage(m, p, p, request == 3 ? max(1, m.health) : 1000);
            self = oldSelf;
            dprint(sprintf("admtest: %s hit: health %g corpse %g gibbed %g drawn %g\n", m.classname, m.health, m.vr_corpse,
                m.ad_gibbed, m.modelindex != 0));
        }
    }
    else if(request == 5)
    {
        // 5: the nearest's neck snapped by your hands (VR_FoeGrab_NeckSnap: AD's Killed, VR_AD_NeckSnapKilled).
        // (the nearest awake one: a map's dormant monster, not yet triggered, can't be hurt)
        entity p5 = find(world, classname, "player");
        entity m5 = world;
        for(entity e5 = nextent(world); e5; e5 = nextent(e5))
        {
            if((e5.flags & FL_MONSTER) && e5.ad_classtype && e5.health > 0 && e5.takedamage &&
               (m5 == world || vlen(e5.origin - p5.origin) < vlen(m5.origin - p5.origin)))
            {
                m5 = e5;
            }
        }
        if(m5 != world)
        {
            float before = killed_monsters;
            entity oldSelf5 = self;
            self = p5;
            float r = VR_FoeGrab_NeckSnap(m5, 1, 0);
            self = oldSelf5;
            dprint(sprintf("admtest: %s neck snapped: result %g health %g deadflag %g killed %g\n", m5.classname, r,
                m5.health, m5.deadflag, killed_monsters - before));
        }
    }
};
