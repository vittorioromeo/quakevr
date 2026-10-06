// vr_limbs_test.qc -- limb gore's tests (vr_limbs.qc; docs/vr-port/ROUND21.md, "Limb gore").
//
// `vr_limb_test <n>` (Debug > Gore Tests > Limb Gore), on the monster (or corpse) nearest you. 1 every limb of its
// model hung in a row before you (Spawn Its Limbs: the limb models made from its own); 2 a corpse cut apart limb by
// limb, the ends first, down to the torso, then its head; 3 the same, whole limbs first; 4 a sword's killing slash at
// a limb (health 1); 5 a fist's (a pop by chance: vr_decap_pop_roll); 6 a shotgun blast at a limb that kills; 7 a
// lightning bolt's; 8 the chance multipliers' rates (2000 rolls at chance 0.5, the head's and a limb's); 9 an explosion
// beside a limb that kills (health 1); 10 a body gibbed (its limbs thrown: vr_gib_limbs); 11 twice Most Limbs thrown
// (the cap); 12 a sword's slash at a limb at full health (a zombie dies of it); 13 where each limb's middle maps
// (ragdolllimb); 14 the main hand put on the last limb thrown (then grip it: vr_mock_button main grip 1); 15 whether a
// hand holds it; 16 its body's speed; 17 the limbs lying about (where, how fast: none fallen out). `limbtest:` lines.

float() VR_Limb_TestCount =
{
    float n = 0;
    for(entity e = find(world, classname, "vr_limb"); e; e = find(e, classname, "vr_limb"))
    {
        n++;
    }
    return n;
};

float() VR_Limb_TestSmallGibs =
{
    float n = 0;
    for(entity e = find(world, classname, "vr_smallgib"); e; e = find(e, classname, "vr_smallgib"))
    {
        n++;
    }
    return n;
};

float vr_limb_test_sgibs; // small gibs before the test's blow

float() VR_Limb_TestFountains =
{
    float n = 0;
    for(entity e = find(world, classname, "vr_decap_fountain"); e; e = find(e, classname, "vr_decap_fountain"))
    {
        n = n + (e.vr_limb_bone >= 0 ? 1 : 0);
    }
    return n;
};

// The nearest monster with limbs (alive: `xLive` 1; dead 0; either -1) within 400 units.
entity(float xLive) VR_Limb_TestTarget =
{
    entity best = world;
    float bestDist = vr_gore_test_only != world ? 999999 : 400; // (a crowd's test: its monster in turn)
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.vr_gib || (xLive == 1 && e.health <= 0) || (xLive == 0 && e.health > 0) ||
           (vr_gore_test_only != world && e != vr_gore_test_only) || ragdolllimbs(e, 0) == 0)
        {
            continue;
        }
        float d = vlen(e.origin - self.origin);
        if(d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
};

// The limb a test strikes: an arm's lower part if it has one (the elbow), else its first limb joint.
float(entity m) VR_Limb_TestBone =
{
    float top = VR_Limb_TopJoints(m);
    float all = ragdolllimbs(m, 0);
    for(float b = 1; b < 16; b++)
    {
        if(top & VR_Limb_Bit(b))
        {
            for(float c = b + 1; c < 16; c++)
            {
                if((all & VR_Limb_Bit(c)) && ragdolllimbs(m, 5, c) == b)
                {
                    return c;
                }
            }
            return b;
        }
    }
    return -1;
};

void(entity m, string xWhat, float xCuts, float xPops, float xLimbs, float xFountains) VR_Limb_TestReport =
{
    vector parts = isragdoll(m) ? ragdollcut(m, 7) : '0 0 0';
    VR_Gore_Print(sprintf("limbtest: %s on %s: cut %g popped %g, limbs %g -> %g, fountains +%g", xWhat, m.classname,
        vr_limb_cuts - xCuts, vr_limb_pops - xPops, xLimbs, VR_Limb_TestCount(), VR_Limb_TestFountains() - xFountains));
    VR_Gore_Print(sprintf(", health %g ragdoll %g parts %g cut %g gib %g small gibs +%g\n", m.health, isragdoll(m), parts_z, parts_x,
        m.vr_gib, VR_Limb_TestSmallGibs() - vr_limb_test_sgibs));
    if(vr_limb_cuts > xCuts && vr_limb_last != world)
    {
        entity h = vr_limb_last;
        VR_Gore_Print(sprintf("limbtest: limb %s at %v, %.0f u/s, spin %.1f, gib %g rigid %g solid %g\n", h.model, h.origin,
            vlen(h.velocity), vlen(h.vr_spin), h.vr_gib, h.vr_rigid, h.solid));
    }
};

void(float xTest) VR_Limb_Test =
{
    float cuts = vr_limb_cuts;
    float pops = vr_limb_pops;
    float limbs = VR_Limb_TestCount();
    float fountains = VR_Limb_TestFountains();
    vr_limb_test_sgibs = VR_Limb_TestSmallGibs();
    float b;
    float all;
    vector parts;
    entity m;
    makevectors('0 1 0' * self.v_angle_y);
    if(xTest == 8)
    {
        float oldLimb = vr_decap_limb;
        vr_decap_by = world;
        for(float k = 0; k < 2; k++)
        {
            float won = 0;
            for(float i = 0; i < 2000; i++)
            {
                vr_decap_limb = k ? 3 : -1;
                vr_decap_chance = 0.5;
                vr_decap_why = "test";
                won = won + VR_Decap_Roll(self);
            }
            VR_Gore_Print(sprintf("limbtest: 8 %s at chance 0.5 x scale %g: %g of 2000 (%.3f)\n", k ? "limb" : "head",
                cvar_hget(k ? cvarh_vr_limbs_chance_scale : cvarh_vr_decap_chance_scale), won, won / 2000));
        }
        vr_decap_limb = oldLimb;
        return;
    }
    if(xTest == 20 || xTest == 21)
    {
        // (As an enemy enforcer's bolt strikes: Laser_Touch, from a shooter of its own 64 units off: of ten ways in (level
        // all round, from above and below), the first whose line meets the head (or the limb) first.)
        m = VR_Limb_TestTarget(1);
        if(m == world)
        {
            bprint(sprintf("limbtest: %g: no living monster with limbs near\n", xTest));
            return;
        }
        float lb = (xTest == 20) ? ragdolllimbs(m, 3) : VR_Limb_TestBone(m);
        vector la = (xTest == 20) ? limbplace(m, lb, 3) : limbplace(m, lb, 0);
        float lpops = vr_decap_pops;
        float lrolls = vr_decap_rolls;
        m.health = 1;
        entity shooter = spawn();
        shooter.classname = "monster_enforcer";
        setorigin(shooter, la);
        entity oself = self;
        self = shooter;
        entity bolt = LaunchLaser(shooter.origin, '1 0 0');
        self = bolt;
        vector bd = '1 0 0';
        float clear = FALSE;
        for(float w = 0; w < 10 && !clear; w++)
        {
            float wa = w * 0.785398;
            bd_x = (w < 8) ? cos(wa) : 0;
            bd_y = (w < 8) ? sin(wa) : 0;
            bd_z = (w < 8) ? 0 : ((w == 8) ? -1 : 1);
            VR_TouchTrace(la - bd * 64, la + bd * 16);
            clear = ((trace_ent == m) && ((xTest == 20) ? VR_Decap_OnHead(m, trace_endpos, bd) : (VR_Limb_At(m, trace_endpos) == lb && !VR_Decap_OnHead(m, trace_endpos, bd))));
        }
        setorigin(shooter, la - bd * 64);
        setorigin(bolt, trace_endpos - bd * 2);
        bolt.velocity = bd * 600;
        other = m;
        dprint(sprintf("limbtest: %g bolt going %v (%s): its line meets %s at %v (limb %g)\n", xTest, bd, clear ? "clear" : "no clear line",
            trace_ent.classname, trace_endpos, ragdolllimb(m, trace_endpos)));
        Laser_Touch();
        self = oself;
        remove(shooter);
        bprint(sprintf("limbtest: %g laser at %s's %s (joint %g, maps to %g): health %g", xTest, m.classname,
            (xTest == 20) ? "head" : "limb", lb, ragdolllimb(m, la), m.health));
        bprint(sprintf(", head popped %g, limbs popped %g, rolls %g\n", vr_decap_pops - lpops, vr_limb_pops - pops,
            vr_decap_rolls - lrolls));
        return;
    }
    if(xTest == 17)
    {
        // (The limbs lying about: each one's model, where it is, how fast: none fallen out of the world.)
        for(entity e = find(world, classname, "vr_limb"); e; e = find(e, classname, "vr_limb"))
        {
            string inSolid = "";
            if(pointcontents(e.origin) == CONTENT_SOLID)
            {
                inSolid = " (in the world's solid)";
            }
            VR_Gore_Print(sprintf("limbtest: 17 %s at %v, %.0f u/s%s\n", e.model, e.origin, vlen(e.velocity), inSolid));
        }
        return;
    }
    if(xTest == 16)
    {
        // (The nearest dead monster's pelvis: how fast its body goes a few frames after the blow: vr_limbs_body_speed.)
        m = VR_Limb_TestTarget(0);
        if(m == world)
        {
            m = VR_Decap_TestTarget(FALSE);
        }
        VR_Gore_Print(sprintf("limbtest: 16 %s's body at %.0f u/s (ragdoll %g)\n", m.classname, vlen(limbplace(m, 0, 2)), isragdoll(m)));
        return;
    }
    if(xTest == 14 || xTest == 15)
    {
        entity h = vr_limb_last;
        if(h == world || h.classname != "vr_limb")
        {
            VR_Gore_Print("limbtest: no limb thrown\n");
            return;
        }
        if(xTest == 14)
        {
            // (It brought before you, within the mock hand's reach, lying still; the hand on it.)
            // (Hung before you at chest height, as Spawn Its Limbs hangs them: the mock hand doesn't reach the floor.)
            setorigin(h, self.origin + v_forward * 16 + '0 0 8');
            h.velocity = '0 0 0';
            h.vr_spin = '0 0 0';
            h.movetype = MOVETYPE_NONE;
            h.vr_rigid = 0;
            VR_Gore_Print(sprintf("limbtest: 14 %s brought before you at %v (you at %v)\n", h.model, h.origin, self.origin));
        }
        else
        {
            VR_Gore_Print(sprintf("limbtest: 15 %s held %g (by %s), at %v, a hand %.1f units from it\n", h.model,
                h.carry_player != world, h.carry_player.classname, h.origin, min(vlen(VRGetHandPos(cVR_MainHand) - h.origin),
                vlen(VRGetHandPos(cVR_OffHand) - h.origin))));
        }
        return;
    }
    if(xTest == 11)
    {
        float most = cvar_hget(cvarh_vr_limbs_max);
        m = VR_Limb_TestTarget(-1);
        if(m == world)
        {
            VR_Gore_Print("limbtest: 11: no monster near\n");
            return;
        }
        b = VR_Limb_TestBone(m);
        for(float j = 0; j < most * 2; j++)
        {
            VR_Limb_Throw(m, limbmodel(m, b), b, self.origin + v_forward * 64 + '0 0 40' + v_right * (j * 4 - most * 4), '0 0 0',
                '0 0 50', '0 0 0');
        }
        VR_Gore_Print(sprintf("limbtest: 11 threw %g limbs, Most Limbs %g: %g lie about\n", most * 2, most, VR_Limb_TestCount()));
        return;
    }
    m = VR_Limb_TestTarget((xTest == 2 || xTest == 3) ? 0 : ((xTest == 1 || xTest == 13) ? -1 : 1));
    if(m == world)
    {
        VR_Gore_Print(sprintf("limbtest: %g: no monster with limbs near\n", xTest));
        return;
    }
    if(xTest == 1)
    {
        all = ragdolllimbs(m, 0);
        float n = 0;
        for(b = 1; b < 16; b++)
        {
            if(!(all & VR_Limb_Bit(b)))
            {
                continue;
            }
            string lm = limbmodel(m, b);
            VR_Gore_Print(sprintf("limbtest: 1 %s limb %g: %s\n", m.classname, b, lm));
            if(lm != "")
            {
                entity t = VR_Limb_Throw(m, lm, b, self.origin + self.view_ofs + v_forward * 48 + v_right * (n * 12 - 50) - '0 0 6', '0 0 0',
                    '0 0 0', '0 0 0');
                t.movetype = MOVETYPE_NONE; // (they hang there to be looked at)
                t.vr_rigid = 0;
                n++;
            }
        }
        return;
    }
    if(xTest == 13)
    {
        all = ragdolllimbs(m, 0);
        for(b = 1; b < 16; b++)
        {
            if(all & VR_Limb_Bit(b))
            {
                vector p = limbplace(m, b, 3);
                VR_Gore_Print(sprintf("limbtest: 13 %s limb %g middle %v maps to %g\n", m.classname, b, p, ragdolllimb(m, p)));
            }
        }
        return;
    }
    vector vel = v_forward * 300;
    if(xTest == 2 || xTest == 3)
    {
        // Down to the torso: the ends first (the highest joint left), or whole limbs (the top joints).
        for(float step = 0; step < 16; step++)
        {
            all = ragdolllimbs(m, 0);
            if(!all)
            {
                break;
            }
            float pick = -1;
            float pool = (xTest == 3) ? VR_Limb_TopJoints(m) : all;
            for(b = 1; b < 16; b++)
            {
                if(pool & VR_Limb_Bit(b))
                {
                    pick = b;
                    if(xTest == 3)
                    {
                        break;
                    }
                }
            }
            float ok = VR_Limb_Cut(m, self, vel, QVR_DECAP_SWING, pick, FALSE);
            parts = ragdollcut(m, 7);
            VR_Gore_Print(sprintf("limbtest: %g cut limb %g: %g, parts left %g, cut bones %g, model %s\n", xTest, pick, ok, parts_z,
                parts_x, vr_limb_last.model));
            if(!ok)
            {
                break;
            }
        }
        float headOk = VR_Decap_Allowed(m) ? VR_Decap_Cut(m, self, vel, QVR_DECAP_SWING) : 0;
        parts = ragdollcut(m, 7);
        VR_Gore_Print(sprintf("limbtest: %g done: head %g, parts left %g (the torso), limbs left %g\n", xTest, headOk, parts_z,
            ragdolllimbs(m, 0)));
        VR_Limb_TestReport(m, "dismembered", cuts, pops, limbs, fountains);
        return;
    }
    b = VR_Limb_TestBone(m);
    vector at = limbplace(m, b, 3); // (a point on its surface)
    float mapped = ragdolllimb(m, at);
    string what = "";
    if(xTest == 10)
    {
        what = "gibbed";
        T_Damage(m, self, self, 1000);
    }
    else if(xTest == 9)
    {
        what = "an explosion by a limb";
        m.health = 1;
        entity blast = spawn();
        setorigin(blast, at + normalize(at - (m.origin + '0 0 8')) * 12);
        T_RadiusDamage(blast, self, 60, world);
        remove(blast);
    }
    else if(xTest == 6 || xTest == 7)
    {
        what = (xTest == 6) ? "a shotgun blast at a limb" : "a lightning bolt at a limb";
        m.health = 1;
        if(VR_Limb_IsZombie(m))
        {
            m.health = 60;
        }
        VR_Decap_TestShot(xTest == 6 ? QVR_DECAP_SHOTGUN : QVR_DECAP_LIGHTNING, at);
    }
    else if(xTest == 5)
    {
        what = "a fist at a limb";
        m.health = 1;
        VR_Decap_TestBlow(at, VR_MTHING_FIST, VR_MKIND_POMMEL, 0.7, 0, 8);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        T_Damage_VRMelee(m, self, self, 30);
        VR_ClearHitKind();
    }
    else
    {
        what = (xTest == 12) ? "a slash at a limb at full health" : "a sword's slash at a limb";
        if(xTest != 12)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, VR_MTHING_SWORD, VR_MKIND_SLASH, 0.7, 0, 8);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        T_Damage_VRMelee(m, self, self, (xTest == 12) ? 10 : 30);
        VR_ClearHitKind();
    }
    VR_Gore_Print(sprintf("limbtest: %g at limb %g (maps to %g)\n", xTest, b, mapped));
    VR_Limb_TestReport(m, what, cuts, pops, limbs, fountains);
};

// Every server frame (StartFrame): a limb test asked for (vr_limb_test).
void() VR_Limb_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_limb_test);
    if(!ask)
    {
        return;
    }
    cvar_set("vr_limb_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(cvar("vr_gore_test_crowd") > 0)
    {
        VR_Gore_TestCrowd(ask, FALSE);
        self = oself;
        return;
    }
    VR_Limb_Test(ask);
    self = oself;
};

// vr_limb_test or vr_decap_test (`xDecap`) `xTest` on every monster within vr_gore_test_crowd units of you (`self`),
// each in turn as if it were the nearest, in one frame, quietly: one `goretest: crowd` line. (A blow's new entities
// are not monsters; one killed or gibbed by a blow before its turn is passed over by the test's own check.)
void(float xTest, float xDecap) VR_Gore_TestCrowd =
{
    float radius = cvar("vr_gore_test_crowd");
    float cuts = vr_limb_cuts;
    float pops = vr_limb_pops;
    float heads = vr_decap_count;
    float headPops = vr_decap_pops;
    float n = 0;
    vr_gore_test_quiet = TRUE;
    for(entity m = nextent(world); m; m = nextent(m))
    {
        if(!(m.flags & FL_MONSTER) || vlen(m.origin - self.origin) > radius)
        {
            continue;
        }
        vr_gore_test_only = m;
        if(xDecap)
        {
            VR_Decap_Test(xTest);
        }
        else
        {
            VR_Limb_Test(xTest);
        }
        n++;
    }
    vr_gore_test_only = world;
    vr_gore_test_quiet = FALSE;
    bprint(sprintf("goretest: crowd %s %g on %g monsters within %g: limbs cut %g popped %g, heads cut %g",
        xDecap ? "vr_decap_test" : "vr_limb_test", xTest, n, radius, vr_limb_cuts - cuts, vr_limb_pops - pops,
        vr_decap_count - heads));
    bprint(sprintf(" popped %g; limbs lying %g\n", vr_decap_pops - headPops, VR_Limb_TestCount()));
};
