// vr_marksman_test.qc -- the marksman ogre's test aid (Debug > Tests; ROUND21.md, "The marksman ogre outside Honey"):
// `vr_marksman_test <n>` on the ogre nearest the player (a marksman: vr_test_spawn 20, impulse 241 puts one ahead; any
// ogre, to compare), its `mkstest:` lines:
//   1 what it is: its classname, model (Honey's progs/mogre.mdl or id's progs/ogre.mdl), whether it is Honey's
//     (VR_IsHoneyMarksman), its health, enemy, frame, the ogres' grenades thrown so far, the player's health
//   2 killed (by the player, just past its health: not gibbed)
//   3 its body: dead, a ragdoll, its model

float vr_mkstest_grenades; // the ogres' grenades seen (OgreFireGrenade's ogre_grenade), counted each frame by 1's probe
entity vr_mkstest_lastnade;

entity() VR_MarksmanTest_Nearest =
{
    entity best = world;
    float bestDist = 4000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(substring(e.classname, 0, 12) == "monster_ogre" && vlen(e.origin - self.origin) < bestDist) // (an ogre: to compare)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
    }
    return best;
};

void() VR_MarksmanTest_Frame =
{
    // Grenades: a new one (the newest found) counted.
    entity nade = find(world, classname, "ogre_grenade");
    if(nade && nade != vr_mkstest_lastnade && substring(nade.owner.classname, 0, 12) == "monster_ogre")
    {
        vr_mkstest_lastnade = nade;
        vr_mkstest_grenades = vr_mkstest_grenades + 1;
    }

    float request = cvar("vr_marksman_test");
    if(!request)
    {
        return;
    }
    cvar_set("vr_marksman_test", "0");

    entity player = find(world, classname, "player");
    entity oldSelf = self;
    self = player;
    entity m = VR_MarksmanTest_Nearest();
    self = oldSelf;
    if(m == world)
    {
        dprint("mkstest: no marksman ogre\n");
        return;
    }

    if(request == 1)
    {
        dprint(sprintf("mkstest: %s model %s honey %g health %g enemy %s ", m.classname, m.model, VR_IsHoneyMarksman(m),
            m.health, m.enemy.classname));
        dprint(sprintf("frame %g dist %.0f grenades %g player health %g\n", m.frame, vlen(m.origin - player.origin),
            vr_mkstest_grenades, player.health));
    }
    else if(request == 2)
    {
        T_Damage(m, player, player, m.health + 1);
        dprint(sprintf("mkstest: killed: health %g\n", m.health));
    }
    else if(request == 3)
    {
        dprint(sprintf("mkstest: body: health %g deadflag %g ragdoll %g model %s\n", m.health, m.deadflag, isragdoll(m),
            m.model));
    }
};
