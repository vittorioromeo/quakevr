#ifdef QVR_TESTS
// vr_airskewer_test.qc -- `vr_embed_test 48` (throwfix): a gib tossed up can be skewered in the air (his note
// vrtesthall_2026-11-15_03-19-26: "if I unscrew a gib and then I throw it ... I'm able to hit it and shoot it and destroy it, but
// it doesn't seem like I'm able to skewer it again while it's in the air ... as long as it's a direct forceful hit with the point
// of the blade"). The sword in the main hand (the mock's), a gib the player threw hard (his own in its throw: his blows passed
// it), near the top of its rise just past the blade's tip; there the hand drives the blade at it:
//   0  thrown, a fast thrust along the blade (5 m/s): on the blade;
//   1  thrown, a slow push along it (0.6 m/s): not on it;
//   2  thrown, a fast swing across it (5 m/s, the blade's side): not on it;
//   3  lying in the air (not thrown), a fast thrust: on it (the resting gib's rules, the same).
// Ends `airskewer: N passed, M failed`.

float VR_AS_CASES = 4;
float VR_AS_THROW = 9;  // m/s: the gib thrown this fast (past VR_THROW_MISSILE_SPEED: a hard throw, his in its throw) ...
float VR_AS_RISE = 20;  // ... met near the top of its rise: units still to rise
float vr_as_owned;   // the thrown gib his as it flies (as his throw: VR_Blow_Strikable passed it)
float vr_as_case;
vector vr_as_rest;   // the main hand's place at the start (each case starts there)
vector vr_as_way;    // the blade's line as the case starts (grip to tip)
vector vr_as_side;   // level, square to it
vector vr_as_tip;
float vr_as_glide;   // the drive's time (s)
entity vr_as_g;

void() VR_AS_Begin =
{
    VR_Test_BeginFlags("airskewer", VR_TEST_LOUD);
    cvar_set("vr_embed", "1");
    localcmd("tune vr_embed_skewer_most 3\n");
    localcmd("vr_mock_hand_glide 0\n+grabmain\n");
    vr_as_case = 0;
    vr_as_g = world;
    vr_et_on = 48;
    vr_et_case = 0;
    VR_ET_To(0);
};

// The gib for the case: thrown by the player at the blade, its rise's top at `xAt`, or hanging there.
entity(entity p, vector xAt, float xThrown) VR_AS_Gib =
{
    entity g = spawn();
    setmodel(g, "progs/gib1.mdl");
    setsize(g, '0 0 0', '0 0 0');
    g.think = SUB_Remove;
    g.nextthink = time + 10;
    VR_MakeGibGrabbable(g);
    VR_Gib_MakeDestroyable(g, 1);
    setorigin(g, xAt - (g.mins + g.maxs) * 0.5);
    g.movetype = MOVETYPE_NONE;
    if(xThrown)
    {
        // As VR_Carry_Release lets it go from the other hand: loose, thrown hard (a missile, his own in its throw:
        // VR_Carry_Throw; a gib flying into the blade or his hands is no longer his, so the thrust must meet it first), then
        // as such a throw is near the top of a lob: rising slowly to the top of its rise at `xAt`, VR_AS_RISE units under it.
        g.carry_player = p;
        VR_Carry_Loose(g);
        VR_Carry_Throw(g, '0 0 1' * (VR_AS_THROW * VRMetersToUnits()));
        VR_Gib_Released(g, vlen(g.velocity), cVR_OffHand);
        float gs = (g.gravity > 0) ? g.gravity : 1;
        setorigin(g, g.origin - '0 0 1' * VR_AS_RISE);
        g.velocity = '0 0 1' * sqrt(2 * cvar_hget(cvarh_sv_gravity) * gs * VR_AS_RISE);
        vr_as_owned = (g.owner == p);
    }
    return g;
};

void() VR_AS_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    entity b = VR_Blow(cVR_MainHand);
    float cm = VRMetersToUnits();
    if(vr_et_s == 0 && VR_HandGrabUtil_IsHandGrabbing(p, cVR_MainHand))
    {
        VR_EH_Give(WID_SWORD, 0);
        vr_as_rest = VRGetHandPos(cVR_MainHand);
        VR_ET_To(1);
    }
    else if(vr_et_s == 1)
    {
        // Each case: the hand back at its start, still, the blade's line read afresh.
        localcmd(sprintf("vr_mock_hand_glide 0\nvr_mock_hand_to main %v\n", vr_as_rest));
        b.mh_far = '0 0 0';
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > 0.3 && b.mh_far != '0 0 0') // (settled: its line from this frame's melee)
    {
        vr_as_way = normalize(b.mh_far - b.mh_grip);
        vr_as_side = normalize([vr_as_way_y, -vr_as_way_x, 0]);
        vr_as_tip = b.mh_far;
        vector at = vr_as_tip + vr_as_way * 14 + ((vr_as_case == 2) ? vr_as_side * 18 : '0 0 0');
        vr_as_g = VR_AS_Gib(p, at, vr_as_case != 3);
        VR_ET_To(3);
    }
    // (A gib is armed a moment after it is made, VR_Gib_MakeDestroyable: struck before, the blow passes it. The hanging one waited
    // for nothing and the thrust raced its arming: 1 run in 6 not on the blade.)
    else if(vr_et_s == 3 && ((vr_as_g.takedamage && (vr_as_case == 3 || vr_as_g.velocity_z <= 0)) || t > 1))
    {
        // At the top of its rise: the drive.
        vector mid = vr_as_g.origin + (vr_as_g.mins + vr_as_g.maxs) * 0.5;
        vector move;
        float speed = 5;
        if(vr_as_case == 2)
        {
            move = vr_as_side * -36;
        }
        else
        {
            move = (mid - vr_as_tip) + vr_as_way * 16;
            speed = (vr_as_case == 1) ? 0.6 : 5;
        }
        vr_as_glide = vlen(move) / (speed * cm);
        localcmd(sprintf("vr_mock_hand_glide %g\nvr_mock_hand_to main %v\n", vr_as_glide, VRGetHandPos(cVR_MainHand) + move));
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && (vr_as_g.vr_skewer_p == p || t > vr_as_glide + 0.3))
    {
        entity g = vr_as_g;
        float on = (g.vr_skewer_p == p) && g.vr_gib && g.model != string_null;
        float want = (vr_as_case == 0) || (vr_as_case == 3);
        string what = (vr_as_case == 0) ? "thrown, a fast thrust" : ((vr_as_case == 1) ? "thrown, a slow push" :
                      ((vr_as_case == 2) ? "thrown, a fast swing across" : "hanging, a fast thrust"));
        VR_Test_Check(on == want && (vr_as_case == 3 || vr_as_owned), sprintf("case %g, %s (%.2f s; his throw %g): %s", vr_as_case, what,
            vr_as_glide, vr_as_owned, on ? "on the blade" : "not on it"));
        vr_as_owned = 0;
        if(g.vr_skewer_p != world)
        {
            VR_Skewer_Off(g);
        }
        if(g.model != string_null || g.vr_gib)
        {
            remove(g);
        }
        vr_as_g = world;
        vr_as_case = vr_as_case + 1;
        VR_ET_To((vr_as_case >= VR_AS_CASES) ? 5 : 1);
    }
    else if(vr_et_s == 5)
    {
        localcmd("vr_mock_hand_glide 0\n");
        VR_Test_Verdict();
        vr_et_on = 0;
    }
};
#endif
