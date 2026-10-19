// vr_projhit_test.qc -- every projectile's one path (weapons.qc "Projectile hits", VR_Proj_Begin; ROUND22.md,
// "Projectiles: one path"): a projectile of each kind fired at a body, whatever its game.
//
// `vr_ad_test 130` (an AD-mounted game: -game ad; a quiet map, ad_dm1), scratch1 the kind (VR_Test_Fire's: 0 a rocket,
// 1 a nail, 2 a grenade, 3 a super nail, 4 an enforcer's laser, 5 Hipnotic's laser cannon bolt, 6 AD's crossbow bolt, 7 AD's
// plasma rifle bolt, 8 AD's nail, 9 Rogue's lava nail, 11 AD's enforcer laser, 12 Rogue's plasma bolt, 13 Rogue's
// multi-rocket), scratch2 the case:
//   0 a grunt's head (health 1: the kill; vr_decap_pop_roll 0 makes any chance sure): "projhit: ... headless 1" a pop;
//   1 a corpse's arm (vr_corpse_health_mult small: one hit wears it off): "limbs N" (non-zero: popped);
//   2 a corpse's torso: "moved D" the ragdoll's torso moved by the hit (a push; the same corpse unhit moves ~0);
//   3 a grunt knocked down (a living, active ragdoll), at its torso: "health H" (hit: below 100; a grenade gone off on
//     it: dead, 0 or less).
// "proj-path N": the shared path's calls (VR_Proj_Begin) since it was fired.
// Wall torches: `vr_test_walltorch_shot <kind>` (vr_walltorch.qc) on a map with one (e1m1), the same kinds.

entity vr_projtest_g;      // the body
entity vr_projtest_m;      // the projectile
vector vr_projtest_mark;   // the torso's place when fired
float vr_projtest_hits0;   // VR_Proj_Begin's count when fired

// A kind's projectile (VR_Test_FireFrom's 5..: self the shooter) from `from` along `dir`; world for none.
entity(float xKind, vector from, vector dir) VR_ProjTest_Launch =
{
    entity m = world;
    float speed = 800;
    if(xKind == 5)
    {
        m = HIP_LaunchLaser(from, dir, '0 0 0', 0);
    }
    else if(xKind == 6 || xKind == 8 || xKind == 11)
    {
        // AD's crossbow bolt (as the crossbow knight's dropped crossbow: VR_ADGun_FireCrossbow), AD's nail and AD's laser
        // (ad_Touch_Projectile: a monster's, or AD's own guns').
        float t = xKind == 6 ? ad_CT_PROJ_BOLT1 : (xKind == 8 ? ad_CT_PROJ_NG : ad_CT_PROJ_LASER);
        ad_launch_projectile(from, dir, t, speed);
        m = ad_newmis;
        if(xKind == 6)
        {
            m.touch = VR_ADGun_BoltTouch;
            m.vr_adgun_wid = WID_ADCROSSBOW;
            m.vr_adgun_dmg = VR_ADGun_Value(WID_ADCROSSBOW, "damage");
        }
    }
    else if(xKind == 7)
    {
        ad_launch_plasma(from, dir, ad_CT_MONARMYPLASMA, speed); // (the plasma soldier's dropped rifle's: VR_ADGun_FirePlasma)
        m = ad_newmis;
        m.touch = VR_ADGun_PlasmaTouch;
        m.vr_adgun_wid = WID_ADPLASMA;
        m.vr_adgun_dmg = VR_ADGun_Value(WID_ADPLASMA, "damage");
        m.vr_adgun_splash = 0;
    }
    else if(xKind == 9)
    {
        m = launch_lava_spike(from, dir);
    }
    else if(xKind == 12)
    {
        m = launch_plasma(from, dir);
    }
    if(m == world)
    {
        bprint(sprintf("projhit: kind %g: no such projectile\n", xKind));
        return world;
    }
    setorigin(m, from);
    m.velocity = dir * max(speed, vlen(m.velocity));
    m.angles = vectoangles(m.velocity);
    return m;
};

// The way to `xAt` on `g` from `xDist` units off that meets `g` there first: its head (xPart 0), limb `xPart` (> 0), or
// its torso (-1), from round about and above it. TRUE and into vr_projtest_from / _dir; FALSE if none.
vector vr_projtest_from, vr_projtest_dir;
float(entity g, vector xAt, float xPart, float xDist, entity p) VR_ProjTest_Way =
{
    for(float elevation = 0; elevation < 3; elevation++)
    {
        for(float angle = 0; angle < 16; angle++)
        {
            makevectors('0 1 0' * angle * 22.5);
            vector trial = xAt + normalize(v_forward + '0 0 0.6' * elevation) * xDist;
            vector flight = normalize(xAt - trial);
            traceline(trial, xAt + flight * 8, MOVE_HITMODEL | MOVE_HITGIBS, p);
            if(trace_ent != g || trace_startsolid)
            {
                continue;
            }
            float head = VR_Decap_OnHead(g, trace_endpos, flight);
            float limb = VR_Limb_At(g, trace_endpos);
            if((xPart == 0 && head) || (xPart > 0 && limb == xPart && !head) || (xPart < 0 && !head && limb < 0))
            {
                vr_projtest_from = trial;
                vr_projtest_dir = flight;
                return TRUE;
            }
        }
    }
    return FALSE;
};

void() VR_ProjTest_Report =
{
    entity g = vr_projtest_g;
    float moved = vlen(limbplace(g, 1, 3) - vr_projtest_mark);
    bprint(sprintf("projhit: kind %g (%s) case %g: health %g headless %g", self.weapon, self.netname, self.count,
        g.health, g.vr_headless));
    bprint(sprintf(" limbs %g moved %.2f gib %g proj-path %g\n", (ragdolllimbs(g, 2) & 2047), moved, g.vr_gib,
        vr_proj_hits - vr_projtest_hits0));
    remove(self);
};

void() VR_ProjTest_Fire =
{
    entity report = self, p = self.owner, g = vr_projtest_g;
    float part = report.count == 0 ? 0 : (report.count == 1 ? 5 : -1);
    vector at;
    if(report.count == 0)
    {
        VR_Decap_HeadZone(g);
        makevectors(g.angles);
        at = g.origin + v_forward * xHeadForward + VR_HeadSide(v_forward) + '0 0 1' * xHeadUp;
    }
    else
    {
        at = limbplace(g, part > 0 ? part : 1, 3);
    }
    self = p;
    if(!VR_ProjTest_Way(g, at, part, 48, p))
    {
        bprint(sprintf("projhit: kind %g case %g: no clear way to the part: FAIL\n", report.weapon, report.count));
        self = report;
        remove(report);
        return;
    }
    vr_projtest_mark = limbplace(g, 1, 3);
    vr_projtest_hits0 = vr_proj_hits;
    vr_projtest_m = VR_Test_FireFrom(report.weapon, vr_projtest_from, vr_projtest_dir);
    report.netname = vr_projtest_m.classname;
    self = report;
    report.think = VR_ProjTest_Report;
    report.nextthink = time + 0.6;
};

void(float request, entity p) AD_ProjHitTest =
{
    float kind = cvar("scratch1");
    float scenario = cvar("scratch2");
    entity os = self;
    if(vr_projtest_g != world && !vr_projtest_g.vr_gib && vr_projtest_g.classname == "monster_army")
    {
        VR_Knockdown_RemoveBody(vr_projtest_g); // (the last case's body out of the way)
    }
    self = p;
    makevectors(p.angles);
    entity g = AD_TestSpawn("monster_army", monster_army, p.origin + v_forward * 160 + '0 0 8');
    self = os;
    // (As the AD shots test's: a grunt standing still, its box, its model, hit as one, AD_ShotsTest.)
    if(g.mdl != "")
    {
        setmodel(g, g.mdl);
    }
    setsize(g, '-16 -16 -24', '16 16 32');
    g.flags = g.flags | FL_MONSTER | FL_ONGROUND;
    g.angles = '0 0 0';
    g.frame = 0;
    g.solid = SOLID_SLIDEBOX;
    g.movetype = MOVETYPE_STEP;
    g.takedamage = DAMAGE_AIM;
    g.nextthink2 = 0;
    g.think2 = SUB_Null;
    setorigin(g, g.origin);
    g.health = scenario == 0 ? 1 : 100;
    g.think = SUB_Null;
    g.nextthink = -1;
    g.th_pain = AD_ShotsPain;
    vr_projtest_g = g;
    float wait = 0.1;
    cvar_set("vr_corpse_nogib", scenario == 2 ? "1" : "0"); // (the push's corpse kept whole: its motion measured)
    if(scenario == 1 || scenario == 2)
    {
        T_Damage(g, g, p, g.health + 1); // (a corpse: a ragdoll in a moment)
        wait = 2.5;
    }
    else if(scenario == 3)
    {
        self = p;
        float down = (canragdoll(g) && VR_Knockdown_Start(g));
        self = os;
        bprint(sprintf("projhit: knocked down %g\n", down));
        wait = 1.0;
    }
    entity r = spawn();
    r.owner = p;
    r.weapon = kind;
    r.count = scenario;
    r.think = VR_ProjTest_Fire;
    r.nextthink = time + wait;
};
