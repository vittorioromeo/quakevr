// vr_dropcap_test.qc -- the cap on dropped single-use enemy weapons' test (Debug > Tests; ROUND21.md, "A cap on
// dropped enemy weapons"; vr_enemyguns.qc VR_EnemyWeapons_MakeRoom). `vr_dropcap_test 1`: ahead of the player, three
// other weapons dropped (a shotgun, a nailgun, a laser cannon: never capped) and a chainsaw thrown straight up, made the
// oldest (it must not go while it flies); then grunts spawned and killed one by one (vr_dropcap_test_n, 0: 60), the
// first one's burst rifle taken into an empty hand (impulse 212's VR_EnemyGuns_TakeNearest: it must stay there). Its
// `dctest:` lines: the most counted lying about at any tick against vr_enemy_weapon_drop_max, the other weapons still
// there, the held one still held, whether the flying chainsaw went while in flight, and the oldest and newest still lying.
// `vr_dropcap_test 2`: just the count lying about and the oldest's and newest's times (after a load: the order kept).

entity vr_dct_runner;    // the test's thinker
float vr_dct_made;       // grunts spawned
float vr_dct_total;      // to spawn
float vr_dct_maxseen;    // the most counted lying about at a tick
float vr_dct_tookone;    // the first burst rifle taken into a hand
float vr_dct_flyfaded;   // the flying chainsaw faded while it flew
float vr_dct_lastspeed;  // its speed at the last tick
float vr_dct_done;       // settling: when the report is due
entity vr_dct_flyer;     // the chainsaw thrown up
entity vr_dct_other1;
entity vr_dct_other2;
entity vr_dct_other3;

// The single-use enemy weapons lying about (counted under the cap); prints the oldest's and newest's times if `xPrint`.
float(float xPrint) VR_DropCapTest_Count =
{
    float n = 0;
    float oldest = 1000000;
    float newest = -1000000;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(VR_EnemyWeapons_Counted(e))
        {
            n = n + 1;
            oldest = min(oldest, e.vr_drop_born);
            newest = max(newest, e.vr_drop_born);
        }
    }
    if(xPrint)
    {
        dprint(sprintf("dctest: lying %g (the most %g), oldest made at %.2f, newest at %.2f, now %.2f\n", n,
            cvar_hget(cvarh_vr_enemy_weapon_drop_max), oldest, newest, time));
    }
    return n;
};

void() VR_DropCapTest_Think =
{
    entity p = find(world, classname, "player");
    self.nextthink = time + 0.1;

    vr_dct_maxseen = max(vr_dct_maxseen, VR_DropCapTest_Count(FALSE));

    // The chainsaw thrown up: gone (fading) while it still flew?
    if(vr_dct_flyer)
    {
        if(vr_dct_flyer.vr_drop_fade && vr_dct_lastspeed > VR_DROPCAP_FLYING && vr_dct_flyer.vr_drop_fade > time - 0.1)
        {
            vr_dct_flyfaded = 1;
        }
        if(vr_dct_flyer.classname != "thrown_weapon" || vr_dct_flyer.vr_drop_fade)
        {
            dprint(sprintf("dctest: the thrown chainsaw went at %.2f (its speed %.0f just before)\n", time, vr_dct_lastspeed));
            vr_dct_flyer = world;
        }
        else
        {
            vr_dct_lastspeed = vlen(vr_dct_flyer.velocity);
        }
    }

    // The grunts that have started (takedamage): killed.
    for(entity m = find(world, classname, "monster_army"); m; m = find(m, classname, "monster_army"))
    {
        if(m.health > 0 && m.takedamage && (m.flags & FL_MONSTER))
        {
            T_Damage(m, p, p, m.health + 1);
        }
    }

    // The first burst rifle lying: taken into an empty hand.
    if(!vr_dct_tookone && find(world, classname, "thrown_weapon"))
    {
        for(entity w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
        {
            if(w.weapon == WID_GRUNTGUN && vlen(w.velocity) < 5 && !vr_dct_tookone)
            {
                entity runner = self;
                self = p;
                VR_EnemyGuns_TakeNearest();
                self = runner;
                vr_dct_tookone = 1;
                dprint(sprintf("dctest: took one: hands %g %g\n", VRGetEntWeapon(p, cVR_MainHand), VRGetEntWeapon(p, cVR_OffHand)));
            }
        }
    }

    if(vr_dct_made < vr_dct_total)
    {
        // One more grunt, ahead (a 4 x 3 grid of spots).
        makevectors('0 1 0' * p.v_viewangle_y);
        float k = vr_dct_made;
        vector at = p.origin + v_forward * (140 + 40 * (k - 3 * floor(k / 3))) + v_right * (-60 + 40 * (floor(k / 3) - 4 * floor(k / 12)));
        entity g = spawn();
        setorigin(g, at);
        g.angles_y = p.v_viewangle_y + 180;
        entity runner2 = self;
        self = g;
        VR_EnemyDispenser_Spawn(0);
        self = runner2;
        vr_dct_made = vr_dct_made + 1;
        return;
    }

    if(!vr_dct_done)
    {
        vr_dct_done = time + 4; // (the last ones' deaths and drops, settled)
        return;
    }
    if(time < vr_dct_done)
    {
        return;
    }

    float others = (vr_dct_other1.classname == "thrown_weapon") + (vr_dct_other2.classname == "thrown_weapon") +
        (vr_dct_other3.classname == "thrown_weapon");
    float held = VRGetEntWeapon(p, cVR_MainHand) == WID_GRUNTGUN || VRGetEntWeapon(p, cVR_OffHand) == WID_GRUNTGUN;
    float most = cvar_hget(cvarh_vr_enemy_weapon_drop_max);
    dprint(sprintf("dctest: grunts %g, the most lying at a tick %g (cap %g), other weapons kept %g of 3, held %g, ",
        vr_dct_made, vr_dct_maxseen, most, others, held));
    dprint(sprintf("the thrown chainsaw went in flight %g\n", vr_dct_flyfaded));
    VR_DropCapTest_Count(TRUE);
    dprint(sprintf("dctest: %s\n", ((most <= 0 || vr_dct_maxseen <= most) && others == 3 && held && !vr_dct_flyfaded) ?
        "PASS" : "FAIL"));
    remove(self);
    vr_dct_runner = world;
};

void() VR_DropCapTest_Frame =
{
    float request = cvar("vr_dropcap_test");
    if(!request)
    {
        return;
    }
    cvar_set("vr_dropcap_test", "0");

    if(request == 2)
    {
        VR_DropCapTest_Count(TRUE);
        return;
    }

    entity p = find(world, classname, "player");
    if(!p || vr_dct_runner)
    {
        dprint("dctest: no player, or already running\n");
        return;
    }
    makevectors('0 1 0' * p.v_viewangle_y);
    vector ahead = p.origin + v_forward * 100;
    vr_dct_other1 = CreateThrownWeapon(p, WID_SHOTGUN, 0, WeaponInst_New(0), ahead + v_right * 30, '0 0 0', '0 0 0',
        '0 0 0', FALSE, FALSE, 0);
    vr_dct_other2 = CreateThrownWeapon(p, WID_NAILGUN, 0, WeaponInst_New(0), ahead, '0 0 0', '0 0 0', '0 0 0', FALSE,
        FALSE, 0);
    vr_dct_other3 = CreateThrownWeapon(p, WID_LASER_CANNON, 0, WeaponInst_New(0), ahead - v_right * 30, '0 0 0', '0 0 0',
        '0 0 0', FALSE, FALSE, 0);
    vr_dct_flyer = CreateThrownWeapon(p, WID_CHAINSAW, 0, WeaponInst_New(50), ahead + '0 0 40', '0 0 0', '0 0 900',
        '0 0 0', FALSE, FALSE, 0);
    vr_dct_flyer.weaponinst.vr_enemy_drop = 1; // (as an ogre's)
    vr_dct_flyer.vr_drop_born = -1; // (the oldest of all)
    VR_EnemyWeapons_MakeRoom(vr_dct_flyer); // (counted from now: as an ogre's drop would be)
    vr_dct_lastspeed = 900;
    dprint(sprintf("dctest: others %s %s %s, a chainsaw thrown up\n", vr_dct_other1.netname, vr_dct_other2.netname,
        vr_dct_other3.netname));

    vr_dct_made = 0;
    vr_dct_total = cvar("vr_dropcap_test_n");
    if(vr_dct_total <= 0)
    {
        vr_dct_total = 60;
    }
    vr_dct_maxseen = 0;
    vr_dct_tookone = 0;
    vr_dct_flyfaded = 0;
    vr_dct_done = 0;
    vr_dct_runner = spawn();
    vr_dct_runner.classname = "vr_dropcap_test";
    vr_dct_runner.think = VR_DropCapTest_Think;
    vr_dct_runner.nextthink = time + 0.1;
};
