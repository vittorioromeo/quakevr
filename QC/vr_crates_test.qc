#ifdef QVR_TESTS
// vr_crates's test harness (moved out of vr_crates.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// A test crate (vr_test_spawn 107, 108, 109: Debug > Tests > Thing): `kind` at `at`, turned by `yaw`; `stacked`: a small
// one on it, turned 20 degrees and off its middle by 3 units.
entity(vector at, float kind, float yaw, float stacked) VR_Crate_TestSpawn =
{
    entity e = spawn();
    setmodel(e, kind == 2 ? "progs/vr_crate2.mdl" : "progs/vr_crate1.mdl");
    e.angles = '0 1 0' * yaw;
    setorigin(e, at);
    crateplace(e);
    VR_Crate_Setup(e, kind);
    dprint(sprintf("test spawn: crate at %v, box %v .. %v, health %.0f\n", e.origin, e.mins, e.maxs, e.health));
    if(stacked)
    {
        entity top = spawn();
        setmodel(top, "progs/vr_crate1.mdl");
        top.angles = '0 1 0' * (yaw + 20);
        setorigin(top, at + '3 0 0');
        crateplace(top);
        // Its bottom on the lower one's top (their boxes: absmin and absmax are a unit wider each way, which left a gap
        // of 2 units the asleep top never fell across: NOTES.md vrfiringrange_2026-10-01_11-44-23).
        setorigin(top, [top.origin_x, top.origin_y, e.origin_z + e.maxs_z - top.mins_z + 0.05]);
        VR_Crate_Setup(top, 1);
        dprint(sprintf("test spawn: a crate on it at %v\n", top.origin));
    }
    return e;
};

// The climbing tests' crates (vr_test_spawn 120..124: vr_climb.cpp, "Holds on props"), square to `yaw` (a face towards
// you): `what` 1 a large crate, 2 a large one on a large one, 3 a large one lying on its side (tipped over about the way
// you face, a 48 x 40 top at 40), 4 a small one on a large one, 5 a small one.
entity(vector at, float yaw, float what) VR_Crate_ClimbTestSpawn =
{
    entity e = spawn();
    setmodel(e, what == 5 ? "progs/vr_crate1.mdl" : "progs/vr_crate2.mdl");
    e.angles = '0 1 0' * yaw;
    setorigin(e, at);
    crateplace(e);
    if(what == 3)
    {
        e.angles_z = 90;
        setorigin(e, e.origin - '0 0 3.5'); // (lying: 20 under its middle, not 24; it drops the half unit)
    }
    VR_Crate_Setup(e, what == 5 ? 1 : 2);
    dprint(sprintf("test spawn: climb crate at %v, angles %v, box %v .. %v\n", e.origin, e.angles, e.mins, e.maxs));
    if(what == 2 || what == 4)
    {
        entity top = spawn();
        setmodel(top, what == 2 ? "progs/vr_crate2.mdl" : "progs/vr_crate1.mdl");
        top.angles = e.angles;
        setorigin(top, at);
        crateplace(top);
        setorigin(top, [e.origin_x, e.origin_y, e.origin_z + e.maxs_z - top.mins_z + 0.05]);
        VR_Crate_Setup(top, what == 2 ? 2 : 1);
        dprint(sprintf("test spawn: a crate on it at %v, box %v .. %v\n", top.origin, top.mins, top.maxs));
    }
    return e;
};
#endif
