// A whole map's monsters at once (vr_test_monsters; Debug > Tests > Whole-Map Monsters; the benchmarks' full-count
// scenarios, BENCHMARKS.md "Dawn of the Machine"): 1 report, 2 wake every live monster (each hunts the player, as if it
// had seen you), 3 kill every live monster with just enough damage (deaths, corpses and ragdolls as a fight's: up to
// vr_ragdoll_max at once, the rest corpses), 4 bring in every monster still waiting for its trigger (the official
// campaigns' deferred monsters, MG_MonsterActivate: the map's full count at once). Prints one "monsterstest:" line
// (bprint: any developer level; waiting: deferred monsters not yet brought in).
void(float xRequest) VR_MonstersTest_Run =
{
    entity p = find(world, classname, "player");
    float live = 0, dead = 0, awake = 0, waiting = 0, acted = 0;
    entity oself = self, oother = other, oact = activator;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.MG_deferred)
        {
            if(xRequest == 4)
            {
                self = e;
                activator = p;
                MG_MonsterActivate();
                acted++;
            }
            else
            {
                waiting++;
            }
            continue;
        }
        if(!(e.flags & FL_MONSTER))
        {
            continue;
        }
        if(e.health <= 0 || e.deadflag != DEAD_NO)
        {
            dead++;
            continue;
        }
        if(p && xRequest == 2 && e.enemy != p && e.th_run)
        {
            self = e;
            self.enemy = p;
            FoundTarget();
            acted++;
        }
        else if(p && xRequest == 3 && e.takedamage)
        {
            self = oself;
            T_Damage(e, p, p, e.health + 1);
            acted++;
        }
        live++;
        if(e.enemy)
        {
            awake++;
        }
    }
    self = oself;
    other = oother;
    activator = oact;
    // (a builtin takes at most 8 parameters: the skill is the console's)
    bprint(sprintf("monsterstest: request %g map %s live %g awake %g dead %g waiting %g acted %g\n", xRequest, mapname, live,
                   awake, dead, waiting, acted));
};

void() VR_MonstersTest_Frame =
{
    float request = cvar("vr_test_monsters");
    if(!request)
    {
        return;
    }
    cvar_set("vr_test_monsters", "0");
    VR_MonstersTest_Run(request);
};
