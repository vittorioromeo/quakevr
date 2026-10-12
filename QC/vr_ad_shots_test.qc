// Actual flight regression: vr_ad_test 50..57 (AD/vanilla, live/corpse, pellet/laser).
// scratch1: 0 head, 1 upper arm, 2 torso, 3 shin; scratch2: 0 killing blow, 1 damage-only.
// scratch4: small MOVETYPE_FLY pellets; scratch3: real SG/SSG pellet count (0 one probe).
// 58 speed, 59 grenade, 60 rocket. These probes leave production damage/fuse paths intact.
void(entity attacker, float damage) AD_ShotsPain = {};
void() AD_ShotsReport =
{
    entity g = self.enemy;
    bprint(sprintf("adshots-test: mode %g region %g health %g damage %g head %g limbs %g gib %g\n",
        self.weapon, self.count, g.health, self.dmg - g.health, g.vr_headless,
        (ragdolllimbs(g, 2) & 2047), g.vr_gib || g.ad_gibbed));
    remove(g);
    remove(self);
};
void() AD_ShotsFire =
{
    entity report = self, p = self.owner, g = self.enemy;
    self = p;
    vector at = g.origin + '0 0 12';
    if(report.count == 0)
    {
        VR_Decap_HeadZone(g);
        makevectors(g.angles);
        at = g.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp;
        if(isragdoll(g)) at = ragdollcut(g, 6);
    }
    if(report.count == 1) at = limbplace(g, 5, 3);
    if(report.count == 3) at = limbplace(g, 8, 3);
    vector dir = report.count == 2 ? '1 0 0' : '-1 0 0';
    if(isragdoll(g) && report.count == 2) at = limbplace(g, 1, 3);
    float distance = isragdoll(g) ? 20 : 64;
    vector from = at - dir * distance;
    if(isragdoll(g))
    {
        float found = FALSE;
        for(float elevation = 0; elevation < 3 && !found; elevation++)
        {
            for(float angle = 0; angle < 16 && !found; angle++)
            {
                makevectors('0 1 0' * angle * 22.5);
                vector away = normalize(v_forward + '0 0 1' * elevation);
                vector trial = at + away * distance;
                vector flight = normalize(at - trial);
                traceline(trial, at + flight * 8, MOVE_HITMODEL | MOVE_HITGIBS, p);
                if(trace_ent != g || trace_startsolid) continue;
                float head = VR_Decap_OnHead(g, trace_endpos, flight);
                float limb = VR_Limb_At(g, trace_endpos);
                if((report.count == 0 && head) || (report.count == 1 && limb == 5) ||
                   (report.count == 2 && !head && limb < 0))
                {
                    from = trial;
                    dir = flight;
                    found = TRUE;
                }
            }
        }
        if(!found) bprint(sprintf("adshots-test: FAIL no clear corpse region %g\n", report.count));
    }
    traceline(from, at + dir * 16, MOVE_HITMODEL | MOVE_HITGIBS, p);
    bprint(sprintf("adshots-test: trace model %s hit %s frac %.2f size %v\n", g.model, trace_ent.classname, trace_fraction, g.size));
    if(report.weapon < 54)
    {
        vr_sgib_blastserial++;
        float oldclass = p.ad_classtype, oldsize = ad_playerprojsize;
        if(cvar("scratch4"))
        {
            p.ad_classtype = ad_CT_PLAYER;
            ad_playerprojsize = 1;
        }
        float count = cvar("scratch3");
        if(count > 0)
        {
            VRGiveEntWeapon(p, cVR_MainHand, count > 6 ? WID_SUPER_SHOTGUN : WID_SHOTGUN, 0, 30);
            float spread = cvar("vr_ad_pellet_spread");
            cvar_set("vr_ad_pellet_spread", "0");
            AD_FirePellets(cVR_MainHand, count, dir, '0 0 0', from);
            cvar_set("vr_ad_pellet_spread", ftos(spread));
        }
        else
        {
            ad_Launch_Bullet(from, dir, ad_CT_PROJ_SG, 1000);
            ad_newmis.vr_ad_shotserial = vr_sgib_blastserial;
            ad_newmis.vr_ad_shotweapon = QVR_DECAP_SHOTGUN;
            ad_newmis.vr_ad_shotpellets = 1;
            ad_newmis.vr_ad_shotfrom = from;
            ad_newmis.vr_ad_shothand = cVR_MainHand;
        }
        p.ad_classtype = oldclass;
        ad_playerprojsize = oldsize;
        bprint(sprintf("adshots-test: pellet movetype %g shot %g pellets %g\n", ad_newmis.movetype,
            ad_newmis.vr_ad_shotweapon, ad_newmis.vr_ad_shotpellets));
    }
    else
    {
        entity laser = spawn();
        laser.owner = p;
        laser.classname = "rifle_laser";
        laser.movetype = MOVETYPE_FLYMISSILE;
        laser.solid = SOLID_BBOX;
        laser.velocity = dir * 1000;
        laser.angles = vectoangles(dir);
        laser.vr_laser_damage = 18;
        laser.touch = VR_RifleLaser_Touch;
        laser.think = SUB_Remove;
        laser.nextthink = time + 1;
        setmodel(laser, "progs/laser.mdl");
        setsize(laser, '0 0 0', '0 0 0');
        setorigin(laser, from);
    }
    bprint(sprintf("adshots-test: flight mode %g region %g at %v bone %g\n", report.weapon, report.count, at, VR_Limb_At(g, at)));
    self = report;
    report.think = AD_ShotsReport;
    report.nextthink = time + 0.25;
};
void(float request, entity p) AD_ShotsTest =
{
    entity os = self;
    self = p;
    if(request <= 57)
    {
        float ad = request == 50 || request == 51 || request == 54 || request == 55;
        float corpse = request == 51 || request == 53 || request == 55 || request == 57;
        vector at = p.origin + (corpse ? '200 0 0' : '200 0 80');
        entity g = AD_TestSpawn("monster_army", ad ? ad_monster_army : monster_army, at);
        if(g.mdl != "") setmodel(g, g.mdl);
        setsize(g, '-16 -16 -24', '16 16 32');
        g.flags = g.flags | FL_MONSTER | FL_ONGROUND;
        g.angles = '0 0 0';
        g.frame = 0;
        g.solid = SOLID_SLIDEBOX;
        g.movetype = MOVETYPE_STEP;
        g.think = SUB_Null;
        g.nextthink = -1;
        if(ad) g.ad_th_pain = AD_TestPelletPain;
        g.th_pain = AD_ShotsPain;
        g.nextthink2 = 0;
        g.think2 = SUB_Null;
        g.takedamage = DAMAGE_AIM;
        g.health = cvar("scratch2") ? 10000 : 1;
        if(corpse)
        {
            T_Damage(g, g, p, g.health + 1);
        }
        setorigin(g, g.origin);
        entity r = spawn();
        r.enemy = g;
        r.owner = p;
        r.weapon = request;
        r.count = cvar("scratch1");
        r.dmg = g.health;
        r.think = AD_ShotsFire;
        r.nextthink = time + (corpse ? 2.5 : 0.1);
    }
    else if(request == 58)
    {
        VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, 6);
        AD_FirePellets(cVR_MainHand, 1, '1 0 0', '0 0 0', p.origin + '40 0 80');
        float playerSpeed = vlen(ad_newmis.velocity);
        entity shooter = spawn();
        shooter.flags = FL_MONSTER;
        self = shooter;
        AD_EnemyPellets(1, '1 0 0', '0 0 0', p.origin + '40 0 120');
        bprint(sprintf("adshots-test: speeds player %.0f enemy %.0f\n", playerSpeed, vlen(ad_newmis.velocity)));
        remove(shooter);
    }
    else
    {
        entity shooter = spawn();
        shooter.flags = FL_MONSTER;
        shooter.enemy = p;
        shooter.health = 100;
        self = shooter;
        if(request == 59)
        {
            ad_Launch_Grenade(p.origin + '100 0 80', '0 0 100', '100 100 100', ad_CT_PROJ_GLMON);
            entity g = ad_newmis;
            bprint(sprintf("adshots-test: grenade model %s catch %g force %g rigid %g shoot %g size %v",
                g.model, g.vr_grenade, (g.flags & FL_FORCEGRABBABLE) != 0, g.vr_rigid, g.vr_shootnade,
                drawnbounds(g, 1) - drawnbounds(g, 0)));
            bprint(sprintf(" hull %v fuse %.2f\n", g.size, g.vr_gren_fuse - time));
            self = p;
            float bat = VR_GrenShot_Damage(g, p, p, 10, TRUE);
            bprint(sprintf("adshots-test: grenade bat %g speed %.1f spent %g\n", bat, vlen(g.velocity), g.vr_shot_off));
            VR_Carry_Start(p, cVR_MainHand, g);
            bprint(sprintf("adshots-test: grenade grabbed %g\n", g.carry_player == p));
        }
        else
        {
            ad_Launch_Missile(p.origin + '200 0 100', '0 1 0', '0 0 0', ad_CT_PROJ_ARMY, 200);
            entity rocket = ad_newmis;
            bprint(sprintf("adshots-test: rocket shoot %g radius %.1f damage %g steering %g\n",
                rocket.vr_shootnade, rocket.vr_shot_radius, rocket.takedamage, rocket.ad_attack_rage));
            self = p;
            VR_Test_Fire(10, rocket.origin);
            bprint(sprintf("adshots-test: rocket intercepted %g callback %g\n", rocket.vr_shot_off,
                rocket.vr_shot_boom == VR_AD_RocketBoom));
        }
        shooter.think = SUB_Remove;
        shooter.nextthink = time + 5;
    }
    self = os;
};
