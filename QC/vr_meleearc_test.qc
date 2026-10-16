// The melee arc test (vr_test_meleearc; Debug > Tests > Holding Enemies; Misc/quakevr/melee_arc_test.sh; ROUND22.md,
// "Melee arc"): the nearest live monster is set D units from the player on its present side and set after him: mode 1
// facing him, 2 facing away (its back to him), 3 facing him and, as its melee attack starts (its first frame: the
// swing's wind-up), he steps aside (to its right, 100 degrees off its facing, as far as he was). The request is D * 10 + mode (D 56 when the request is the mode
// alone: 1001 is mode 1 from 100 units, a knight's run swing). His health is held at 1000 (no god: blows must land)
// and every loss counted as a hit; after 4 seconds one "meleearc:" line: hits, damage, the first hit's time and the
// monster's angle off its facing then, and with mode 3 the hits in the 1 s after the step (the swing he stepped out
// of; it may come after him and strike again later).
entity vr_mat_mon;
float vr_mat_mode;
float vr_mat_start;
float vr_mat_hits;
float vr_mat_dmg;
float vr_mat_first;
float vr_mat_first_off;
float vr_mat_stepped; // when he stepped aside (mode 3; 0: not yet)
float vr_mat_after; // hits since
float vr_mat_blows; // blows the arc weighed (VR_MeleeArc_Hits), and how many landed
float vr_mat_landed;
float vr_mat_blowtime;

// self's angle off its facing to the player (VR_MeleeArc_Off as self).
float(entity m, entity p) VR_MeleeArcTest_Off =
{
    entity oself = self, oenemy = m.enemy;
    self = m;
    self.enemy = p;
    float off = VR_MeleeArc_Off();
    self.enemy = oenemy;
    self = oself;
    return off;
};

void(entity p, float request) VR_MeleeArcTest_Start =
{
    float mode = request - floor(request / 10) * 10;
    float dist = request >= 10 ? floor(request / 10) : 56;
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
        bprint("meleearc: no monster: FAIL\n");
        return;
    }
    vector d = best.origin - p.origin;
    d_z = 0;
    d = d == '0 0 0' ? '1 0 0' : normalize(d);
    vector o = p.origin + d * dist;
    o_z = best.origin_z;
    setorigin(best, o);
    float yaw = vectoyaw(-1 * d) + (mode == 2 ? 180 : 0);
    best.angles_y = anglemod(yaw);
    best.ideal_yaw = best.angles_y;
    p.health = 1000;
    p.armorvalue = 0;
    p.armortype = 0;
    if(best.classname == "monster_sword")
    {
        best.delay = 0; // (the phantom swordsman waits its delay, 10 s, once woken: sword_pause)
    }
    entity oself = self;
    self = best;
    if(!AD_TestWake(p)) // AD support: AD's monsters by AD's AI
    {
        self.enemy = p;
        FoundTarget();
    }
    self = oself;
    best.angles_y = anglemod(yaw); // (as set: waking may turn it)
    vr_mat_mon = best;
    vr_mat_mode = mode;
    vr_mat_start = time;
    vr_mat_hits = vr_mat_dmg = vr_mat_after = vr_mat_stepped = vr_mat_blows = vr_mat_landed = 0;
    vr_mat_first = -1;
    vr_mat_first_off = -1;
    vr_mat_blowtime = vr_meleearc_last_time;
    bprint(sprintf("meleearc: start %s mode %g dist %g arc %g (%g deg, turn %g)\n", best.classname, mode, dist,
        cvar("vr_enemy_melee_arc"), cvar("vr_enemy_melee_arc_angle"), cvar("vr_enemy_melee_turn")));
};

void() VR_MeleeArcTest_Frame =
{
    float request = cvar("vr_test_meleearc");
    entity p = find(world, classname, "player");
    if(request)
    {
        cvar_set("vr_test_meleearc", "0");
        if(p)
        {
            VR_MeleeArcTest_Start(p, request);
        }
    }
    if(!vr_mat_mode || !p)
    {
        return;
    }
    entity m = vr_mat_mon;
    if(vr_meleearc_last_time != vr_mat_blowtime)
    {
        vr_mat_blowtime = vr_meleearc_last_time;
        vr_mat_blows++;
        vr_mat_landed += vr_meleearc_last_hit;
    }
    if(p.health < 1000)
    {
        vr_mat_hits++;
        vr_mat_dmg += 1000 - p.health;
        if(vr_mat_first < 0)
        {
            vr_mat_first = time - vr_mat_start;
            vr_mat_first_off = VR_MeleeArcTest_Off(m, p);
        }
        if(vr_mat_stepped && time - vr_mat_stepped < 1) // (the swing he stepped out of)
        {
            vr_mat_after++;
        }
        p.health = 1000;
    }
    if(vr_mat_mode == 3 && !vr_mat_stepped && m.health > 0 && m.vr_melee_time > vr_mat_start)
    {
        // He steps aside, to its right (away from a knight's run past him, against an ogre's sweep): 100 degrees off
        // its facing, as far as he was (level).
        vector d = p.origin - m.origin;
        d_z = 0;
        makevectors('0 1 0' * (m.angles_y - 100));
        vector o = m.origin + v_forward * vlen(d);
        o_z = p.origin_z;
        setorigin(p, o);
        vr_mat_stepped = time;
        bprint(sprintf("meleearc: stepped aside at %.2f (%.0f deg off)\n", time - vr_mat_start, VR_MeleeArcTest_Off(m, p)));
    }
    if(time - vr_mat_start >= 4 || m.health <= 0)
    {
        // (two prints: a builtin takes at most 8 arguments)
        bprint(sprintf("meleearc: result %s mode %g arc %g: hits %g dmg %.0f first %.2f off %.0f", m.classname, vr_mat_mode,
            cvar("vr_enemy_melee_arc"), vr_mat_hits, vr_mat_dmg, vr_mat_first, vr_mat_first_off));
        bprint(sprintf(" blows %g landed %g after-step %g\n", vr_mat_blows, vr_mat_landed, vr_mat_after));
        vr_mat_mode = 0;
    }
};
