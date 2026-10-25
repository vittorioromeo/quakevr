#ifdef QVR_TESTS
// The aim cone test (vr_test_aimcone; Debug > Tests > Holding Enemies; Misc/quakevr/aim_cone_test.sh; ROUND22.md,
// "Ranged aim cone"): the request is D * 1000 + A (A alone: D 300). The nearest live monster is set D units from the
// player, facing him and after him, and its ranged attack starts at once (its th_missile; AD's by AD's). Until it has
// fought 2.5 seconds it is turned each frame so he is A degrees to its right, plus its own turn speed (yaw_speed: a
// frame's ai_face turns that much before its shot), so each shot finds him about A degrees off: circling it faster than
// it turns. His health is held at 1000 and every loss counted as a hit; then one "aimcone: result" line: the shots
// weighed (VR_AimCone_Bend), the first's time, its angle off and how far it was bent, the waits and their length.
entity vr_act_mon;
float vr_act_angle;
float vr_act_dist;
float vr_act_start;
float vr_act_hits;
float vr_act_dmg;
float vr_act_shots;      // shots weighed since the start (vr_aimcone_last_time changes)
float vr_act_shottime;
float vr_act_first;      // the first shot's time (from the start), its angle off its facing, and how far it was bent
float vr_act_first_off;
float vr_act_first_bent;
float vr_act_waits0;     // vr_aimcone_waits at the start

// He is held `angle` degrees to m's right of its facing, plus m's turn speed: m is turned (moving him round it would
// put him in walls, and Quake's unstick would put him back).
void(entity m, entity p, float angle, float dist) VR_AimConeTest_Place =
{
    m.angles_y = anglemod(vectoyaw(p.origin - m.origin) + angle + m.yaw_speed);
};

void(entity p, float request) VR_AimConeTest_Start =
{
    float angle = request - floor(request / 1000) * 1000;
    float dist = request >= 1000 ? floor(request / 1000) : 300;
    entity best = world;
    float bestd = 100000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && e.deadflag == DEAD_NO && vlen(e.origin - p.origin) < bestd)
        {
            best = e;
            bestd = vlen(e.origin - p.origin);
        }
    }
    if(!best)
    {
        bprint("aimcone: no monster: FAIL\n");
        return;
    }
    vector d = best.origin - p.origin;
    d_z = 0;
    d = d == '0 0 0' ? '1 0 0' : normalize(d);
    vector o = p.origin + d * dist;
    o_z = best.origin_z;
    setorigin(best, o);
    best.angles_y = anglemod(vectoyaw(-1 * d));
    best.ideal_yaw = best.angles_y;
    p.health = 1000;
    p.armorvalue = 0;
    p.armortype = 0;
    entity oself = self;
    self = best;
    float ad = AD_TestWake(p); // AD support: AD's monsters by AD's AI
    if(!ad)
    {
        self.enemy = p;
        FoundTarget();
    }
    self.angles_y = anglemod(vectoyaw(-1 * d)); // (as set: waking may turn it)
    if(ad && AD_HasMissile(self))
    {
        self.ad_th_missile();
    }
    else if(self.th_missile)
    {
        self.th_missile();
    }
    else
    {
        bprint(sprintf("aimcone: %s has no ranged attack: FAIL\n", self.classname));
    }
    self = oself;
    vr_act_mon = best;
    vr_act_angle = angle;
    vr_act_dist = dist;
    vr_act_start = time;
    vr_act_hits = vr_act_dmg = vr_act_shots = 0;
    vr_act_first = vr_act_first_off = vr_act_first_bent = -1;
    vr_act_shottime = vr_aimcone_last_time;
    vr_act_waits0 = vr_aimcone_waits;
    VR_AimConeTest_Place(best, p, angle, dist);
    bprint(sprintf("aimcone: start %s angle %g dist %g on %g (aim %g fire %g delay %g)\n", best.classname, angle, dist,
        cvar("vr_enemy_aim_cone"), cvar("vr_enemy_aim_angle"), cvar("vr_enemy_aim_fire"), cvar("vr_enemy_aim_delay")));
};

void() VR_AimConeTest_Frame =
{
    float request = cvar("vr_test_aimcone");
    entity p = find(world, classname, "player");
    if(request)
    {
        cvar_set("vr_test_aimcone", "0");
        if(p)
        {
            VR_AimConeTest_Start(p, request);
        }
    }
    if(!vr_act_mon || !p)
    {
        return;
    }
    entity m = vr_act_mon;
    if(vr_aimcone_last_time != vr_act_shottime)
    {
        vr_act_shottime = vr_aimcone_last_time;
        vr_act_shots++;
        if(cvar("vr_enemy_aim_debug"))
        {
            vector d = p.origin - m.origin;
            bprint(sprintf("aimcone: test shot at %.2f: he is %.0f deg off its facing now, %.0f away, its think %s\n",
                time - vr_act_start, fabs(VR_AimCone_Yaw(m, d)), vlen(d), m.think == VR_AimCone_HoldThink ? "hold" : "frame"));
        }
        if(vr_act_first < 0)
        {
            vr_act_first = time - vr_act_start;
            vr_act_first_off = vr_aimcone_last_off;
            vr_act_first_bent = vr_aimcone_last_bent;
        }
    }
    if(p.health < 1000)
    {
        vr_act_hits++;
        vr_act_dmg += 1000 - p.health;
        p.health = 1000;
    }
    if(time - vr_act_start >= 2.5 || m.health <= 0)
    {
        // (two prints: a builtin takes at most 8 arguments)
        bprint(sprintf("aimcone: result %s angle %g dist %g on %g: shots %g first %.2f off %.0f", m.classname,
            vr_act_angle, vr_act_dist, cvar("vr_enemy_aim_cone"), vr_act_shots, vr_act_first, vr_act_first_off));
        bprint(sprintf(" bent %.0f waits %g (last %.2f s) hits %g dmg %.0f\n", vr_act_first_bent,
            vr_aimcone_waits - vr_act_waits0,
            vr_aimcone_waits > vr_act_waits0 ? vr_aimcone_last_wait : 0, vr_act_hits, vr_act_dmg));
        vr_act_mon = world;
        return;
    }
    VR_AimConeTest_Place(m, p, vr_act_angle, vr_act_dist);
};
#endif
