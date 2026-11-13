#ifdef QVR_TESTS
// vr_pickup_test 15 (vr_meleeonly_bot_test.qc): FrikBot bots under Melee Only (vr_run_meleeonly.qc). A bot connected with
// Melee Only on and the dagger as the run's melee weapon: it starts with the dagger and no gun, draws the dagger; given a
// shotgun and a rocket launcher with ammunition it keeps the dagger (bot_pick_weapon: VR_Run_RefusesFire); a map's
// shotgun pickup, a box of shells and a thrown burst rifle are no goals (priority_for_thing: VR_Run_RefusesEnt), the
// rifle not snatched; a thrown axe is snatched; a grunt in reach ahead: it swings the dagger (bot_fight_style). Melee
// Only off: it picks the rocket launcher and the shotgun pickup is a goal again. Needs a free client slot: run with
// `maxplayers 2; deathmatch 0` before the map. Verdict "meleeonlybots: N passed, M failed".

void() VR_MeleeOnlyBotTest_Run =
{
    entity pl = self;
    VR_Test_BeginFlags("meleeonlybots", VR_TEST_LOUD);
    float was = cvar_hget(cvarh_vr_run_meleeonly);
    float wasMelee = cvar_hget(cvarh_vr_run_startmelee);
    cvar_set("vr_run_meleeonly", "1");
    cvar_set("vr_run_startmelee", ftos(RUN_SM_DAGGER));
    float f = ClientNextAvailable();
    VR_Test_Check(f != -1, "a client slot free for a bot (maxplayers 2 or more)");
    if(f == -1)
    {
        cvar_set("vr_run_meleeonly", ftos(was));
        cvar_set("vr_run_startmelee", ftos(wasMelee));
        VR_Test_Verdict();
        return;
    }
    BotConnect(0, 0, 1);
    entity bot = GetClientEntity(f);
    self = bot;

    // The start: the run's melee weapon, no gun.
    VR_Test_Check(bot.holsterweapon2 == WID_SWORD && VR_ADMelee_Kind(bot.holsterweaponflags2) == VR_SWORDKIND_DAGGER,
        "the bot starts with the run's dagger");
    VR_Test_Check(!(bot.items & IT_SHOTGUN) && bot.holsterweapon3 == WID_FIST, "the bot starts with no gun");
    bot_weapon_switch(-1);
    VR_Test_Check(bot.weapon == WID_SWORD && VR_ADMelee_Kind(bot.weaponflags) == VR_SWORDKIND_DAGGER,
        "the bot draws its dagger");

    // Guns held anyway (a cheat): never chosen.
    VR_WeaponUtil_EntAddWeapon(bot, WID_SHOTGUN);
    VR_WeaponUtil_EntAddWeapon(bot, WID_ROCKET_LAUNCHER);
    bot.ammo_shells = 25;
    bot.ammo_rockets = 10;
    VR_Test_Check(bot_pick_weapon(-1) == WID_SWORD && bot_pick_weapon(300) == WID_SWORD && bot_pick_weapon(40) == WID_SWORD,
        "given a shotgun and a rocket launcher, it keeps the dagger");

    // Its goals: no gun nor ammunition sought, none snatched; a melee weapon still is.
    entity e = VR_MOT_Make(bot, 1);
    VR_Test_Check(e != world && priority_for_thing(e) == 0, "a map's shotgun pickup is no goal");
    if(e != world) { remove(e); }
    e = VR_MOT_Make(bot, 3);
    VR_Test_Check(e != world && priority_for_thing(e) == 0, "a box of shells is no goal");
    if(e != world) { remove(e); }
    e = VR_MOT_Make(bot, 0);
    float pr = priority_for_thing(e);
    VR_Test_Check(pr == 0 && e.model != string_null && bot.weapon != WID_GRUNTGUN, "a thrown burst rifle near it: not snatched");
    if(e.model != string_null) { remove(e); }
    e = VR_MOT_Make(bot, 4);
    pr = priority_for_thing(e);
    VR_Test_Check(e.model == string_null || bot.weapon == WID_AXE, "a thrown axe near it: snatched");
    if(e.model != string_null) { remove(e); }
    bot_weapon_switch(-1);

    // A grunt in reach ahead: the dagger swung at it.
    makevectors(bot.v_angle_y * '0 1 0');
    entity g = VR_Test_Spawn(bot, 0, bot.origin + v_forward * 48, anglemod(bot.v_angle_y + 180));
    VR_Test_Check(g != world, "a grunt spawned ahead of the bot");
    if(g != world)
    {
        if(!g.takedamage && g.think)
        {
            self = g;
            g.think(); // (its start now, not a frame later: it takes damage from then on)
            self = bot;
        }
        vector a = vectoangles(g.origin - bot.origin);
        a_x = 0;
        bot.v_angle = bot.b_angle = a;
        bot.angles = a;
        bot.enemy = g;
        bot.button0 = FALSE;
        bot_fight_style();
        VR_Test_Check(bot.button0 && bot.weapon == WID_SWORD, "a grunt in reach: it swings the dagger");
        if(!bot.button0)
        {
            bprint(sprintf("meleeonlybots: grunt health %g takedamage %g visible %g dist %g weapon %g enemy %g\n", g.health,
                g.takedamage, fisible(g), vlen(realorigin(g) - bot.origin), bot.weapon, bot.enemy == g));
        }
        bot.enemy = world;
        remove(g);
    }

    // Off: guns again.
    cvar_set("vr_run_meleeonly", "0");
    VR_Test_Check(bot_pick_weapon(-1) == WID_ROCKET_LAUNCHER, "off: it picks the rocket launcher");
    e = VR_MOT_Make(bot, 1);
    VR_Test_Check(e != world && priority_for_thing(e) > 0, "off: a map's shotgun pickup is a goal");
    if(e != world) { remove(e); }

    self = pl;
    cvar_set("vr_run_meleeonly", ftos(was));
    cvar_set("vr_run_startmelee", ftos(wasMelee));
    VR_Test_Verdict();
};
#endif
