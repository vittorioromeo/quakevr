#ifdef QVR_TESTS
// buttons's test harness (moved out of buttons.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// Test aid `vr_test_dispenser <n>` (console): the map's n-th monster dispenser (1: the first in the map) used, as its
// button would (the activator: the player). Prints `test dispenser:` with its targetname, its yaw ("angle") and the
// monster's classname, yaw and ideal_yaw (it faces the dispenser's angle).
void() VR_TestDispenser_Frame =
{
    float want = cvar("vr_test_dispenser");
    if(want < 1)
    {
        return;
    }
    cvar_set("vr_test_dispenser", "0");
    float n = 0;
    entity e = find(world, classname, "func_enemy_dispenser");
    while(e)
    {
        n = n + 1;
        if(n == want)
        {
            break;
        }
        e = find(e, classname, "func_enemy_dispenser");
    }
    if(!e)
    {
        dprint(sprintf("test dispenser: the map has %g, not %g\n", n, want));
        return;
    }
    entity oldself = self;
    entity oldactivator = activator;
    self = e;
    activator = VR_Player();
    func_enemy_dispenser_use();
    if(self != e)
    {
        dprint(sprintf("test dispenser: %s (yaw %g) made %s yaw %g ideal_yaw %g\n", e.targetname, e.angles_y,
            self.classname, self.angles_y, self.ideal_yaw));
    }
    else
    {
        dprint(sprintf("test dispenser: %s made nothing\n", e.targetname));
    }
    self = oldself;
    activator = oldactivator;
};
#endif
