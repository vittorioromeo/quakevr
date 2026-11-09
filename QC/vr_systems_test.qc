#ifdef QVR_TESTS
// vr_systems_test.qc -- the shared systems' tests and dumps (vr_systems_test <n>, Debug > Systems Tests;
// DYNAMIC_SPAWNS.md, "Prerequisites available"):
//   1 every entity's material answers, one line each (the material table's consumers: hits, fire, impacts), then
//     "systems_test: matdump <n> entities": the before/after proof that the table changed no answer;
//   2 every entity's status (vr_status.qc) and wall mount (vr_mount.qc), then "systems_test: status <n> entities".

// One entity's consumers' answers.
void(entity e) VR_SysTest_MatLine =
{
    float m = VR_MaterialOf(e);
    dprint(sprintf("matdump: %s %s at %v: mat %g fleshy %g", e.classname, e.model, e.origin, m, VR_Material_Fleshy(m)));
    dprint(sprintf(" burncan %g burnwood %g hitmetal %g cratewood %g\n", VR_Burn_Can(e), VR_Burn_Wood(e), VR_Hit_Metal(e),
        VR_Crate_Wooden(e)));
};

void() VR_SysTest_MatDump =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.classname != "")
        {
            VR_SysTest_MatLine(e);
            n = n + 1;
        }
        e = nextent(e);
    }
    dprint(sprintf("systems_test: matdump %g entities\n", n));
};

void() VR_SysTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_systems_test);
    if(!ask)
    {
        return;
    }
    cvar_set("vr_systems_test", "0");
    if(ask == 1)
    {
        VR_SysTest_MatDump();
    }
};
#endif
