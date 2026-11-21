#ifdef QVR_TESTS
// vr_botnav_test.qc -- bots on the navmesh beside their waypoints (vr_bot_navmesh, frikbot/bot_nav.qc; NAVMESH.md 23):
// vr_dm_test 19-20, a deathmatch on e1m1 with the human and a bot (run.sh: `maxplayers 4; deathmatch 1; map e1m1;
// impulse 100; vr_dm_test 19`).
// - 19: the bot at the map's start, the human 160 units ahead in its sight till it has noticed him (its foe or a goal);
//   then he is put past the first room's west door (out of its sight, a door it never used, no waypoint through it) and
//   kept there: with vr_bot_navmesh 0 (FrikBot's straight walk: for contrast, not checked), then 1: it walks to within 96
//   units of him by the navmesh (its vr_botnav_why 1: a player it saw) in VR_BN_WALK frames.
// - 20: vr_bot_navmesh 1, the human kept past that door (never seen): a waypoint chain from the bot's start ahead to a
//   rocket launcher at its end; the bot takes it by its waypoints (target1 a waypoint some frame, the navmesh never used).
// Ends with `dmtest: N passed, M failed`.

float vr_bn_on;      // the running test's number
float vr_bn_s;       // its state
float vr_bn_n;       // frames the state has run
float vr_bn_case;    // 19: the case (0 navmesh off, 1 on)
float vr_bn_nav;     // vr_bot_navmesh before
float vr_bn_used;    // the case's most telling vr_botnav_why seen (19: 1; 20: any)
float vr_bn_way;     // 20: target1 a waypoint some frame
float vr_bn_best;    // 19: the nearest it came (flat units)
float vr_bn_got[2];  // 19: frames to reach him by case (-1 never)
vector vr_bn_start;  // the bot's place at a case's start (the map's info_player_start)
vector vr_bn_fwd;    // ... its facing
vector vr_bn_seen;   // 19: the human's place in its sight
vector vr_bn_past;   // the human's place past the west door
entity vr_bn_item;   // 20: the rocket launcher
float VR_BN_WALK = 1800;  // frames the bot has to reach him or the item (25 s at 72 Hz)
float VR_BN_GIVEUP = 360; // frames a waiting state may last

void() VR_BN_End =
{
    cvar_set("vr_bot_navmesh", ftos(vr_bn_nav));
    if(vr_dmt_human)
    {
        vr_dmt_human.health = 100;
    }
    VR_Test_Verdict();
    cvar_set("vr_dm_test", "0");
    vr_bn_on = 0;
    vr_bn_s = -1;
};

// The bot back at the start, facing as the map's start does, its targets and route forgotten.
void() VR_BN_Reset =
{
    entity b = vr_dmt_bot;
    setorigin(b, vr_bn_start);
    b.velocity = '0 0 0';
    b.angles = b.v_angle = vectoangles(vr_bn_fwd);
    b.health = 100;
    entity os = self;
    self = b;
    ClearMyRoute();
    navbot(b, world, 2);
    self = os;
    b.target1 = b.target2 = b.target3 = b.target4 = b.enemy = world;
    b.last_way = world;
    b.b_aiflags = 0;
    b.keys = 0;
    b.vr_botnav = b.vr_botnav_foe = world;
    b.vr_botnav_why = 0;
    b.search_time = 0;
};

// The human held at `at`, kept alive (the bot fires at him).
void(vector at) VR_BN_Hold =
{
    entity h = vr_dmt_human;
    setorigin(h, at);
    h.velocity = '0 0 0';
    h.health = 1000;
};

float() VR_BN_Noticed =
{
    entity b = vr_dmt_bot, h = vr_dmt_human;
    return (b.enemy == h || b.vr_botnav_foe == h || b.target1 == h || b.target2 == h);
};

void() VR_BN_Door =
{
    entity b = vr_dmt_bot, h = vr_dmt_human;
    if(vr_bn_s == 1) // a case: the bot at the start, the human in its sight
    {
        cvar_set("vr_bot_navmesh", ftos(vr_bn_case));
        VR_BN_Reset();
        VR_BN_Hold(vr_bn_seen);
        vr_bn_used = 0;
        vr_bn_best = 100000;
        vr_bn_s = 2;
        vr_bn_n = 0;
        return;
    }
    if(vr_bn_s == 2) // till it has noticed him
    {
        VR_BN_Hold(vr_bn_seen);
        setorigin(b, vr_bn_start);
        b.velocity = '0 0 0';
        if(!VR_BN_Noticed() && vr_bn_n < VR_BN_GIVEUP)
        {
            return;
        }
        VR_Test_Check(VR_BN_Noticed(), sprintf("navmesh %g: the bot notices the human 160 units ahead (enemy %s, target1 %s) in %g frames",
            vr_bn_case, b.enemy.netname, b.target1.classname, vr_bn_n));
        traceline(vr_bn_start + '0 0 22', vr_bn_past + '0 0 22', TRUE, b);
        VR_Test_Check(trace_fraction < 1, sprintf("past the west door (%s) he is out of its sight from the start", vtos(vr_bn_past)));
        vr_bn_s = 3;
        vr_bn_n = 0;
        return;
    }
    if(vr_bn_s == 3) // he is past the door; it walks to him
    {
        VR_BN_Hold(vr_bn_past);
        b.health = max(b.health, 100);
        if(b.vr_botnav_why == 1)
        {
            vr_bn_used = 1;
        }
        vector d = b.origin - vr_bn_past;
        float dz = fabs(d_z);
        d_z = 0;
        vr_bn_best = min(vr_bn_best, vlen(d));
        if(cvar_hget(cvarh_developer) && !(vr_bn_n % 72))
        {
            dprint(sprintf("botnav: case %g frame %g: at %s, enemy %s, navmesh %g, routing %g, ", vr_bn_case, vr_bn_n,
                vtos(b.origin), b.enemy.netname, cvar_hget(cvarh_vr_bot_navmesh), route_table == b));
            dprint(sprintf("foe %s seen %.2f s ago, why %g, target1 %s, keys %g\n", b.vr_botnav_foe.netname,
                time - b.vr_botnav_seen, b.vr_botnav_why, b.target1.classname, b.keys));
        }
        float near = (vlen(d) <= 96 && dz < 64);
        if(!near && vr_bn_n < VR_BN_WALK)
        {
            return;
        }
        vr_bn_got[vr_bn_case] = near ? vr_bn_n : -1;
        bprint(sprintf("botnav: navmesh %g: %s in %g frames (nearest %.0f units, the navmesh chase used %g), at %s\n",
            vr_bn_case, near ? "reached him" : "never reached him", vr_bn_n, vr_bn_best, vr_bn_used, vtos(b.origin)));
        if(vr_bn_case == 1)
        {
            VR_Test_Check(near && vr_bn_used, sprintf("vr_bot_navmesh 1: the bot walks through the west door to within 96 units "
                "of him by the navmesh (%g frames; nearest %.0f; chase used %g); navmesh 0 for contrast: %g frames",
                vr_bn_n, vr_bn_best, vr_bn_used, vr_bn_got[0]));
            VR_BN_End();
            return;
        }
        vr_bn_case = 1;
        vr_bn_s = 1;
        vr_bn_n = 0;
    }
};

void() VR_BN_Items =
{
    entity b = vr_dmt_bot;
    if(vr_bn_s == 1) // the waypoint chain ahead of the start and the rocket launcher at its end
    {
        cvar_set("vr_bot_navmesh", "1");
        VR_BN_Reset();
        VR_BN_Hold(vr_bn_past);
        tracebox(vr_bn_start, VEC_HULL_MIN, VEC_HULL_MAX, vr_bn_start + vr_bn_fwd * 320, TRUE, b);
        float d = vlen(trace_endpos - vr_bn_start) - 32;
        VR_Test_Check(d >= 128, sprintf("%.0f units clear ahead of the start for the chain", d));
        if(d < 128)
        {
            VR_BN_End();
            return;
        }
        entity w0 = make_waypoint(vr_bn_start);
        entity w1 = make_waypoint(vr_bn_start + vr_bn_fwd * (d / 3));
        entity w2 = make_waypoint(vr_bn_start + vr_bn_fwd * (d * 2 / 3));
        entity w3 = make_waypoint(vr_bn_start + vr_bn_fwd * d);
        LinkWays(w0, w1);
        LinkWays(w1, w2);
        LinkWays(w2, w3);
        entity os = self;
        vr_bn_item = spawn();
        setorigin(vr_bn_item, w3.origin);
        self = vr_bn_item;
        vr_bn_item.classname = "weapon_rocketlauncher";
        weapon_rocketlauncher();
        self = os;
        vr_bn_item.current_way = w3; // (its waypoint: FrikBot's priority adds the route's nearness)
        vr_bn_used = 0;
        vr_bn_way = 0;
        vr_bn_s = 2;
        vr_bn_n = 0;
        return;
    }
    if(vr_bn_s == 2) // till it has the rocket launcher
    {
        VR_BN_Hold(vr_bn_past);
        b.health = max(b.health, 100);
        if(b.vr_botnav)
        {
            vr_bn_used = max(vr_bn_used, b.vr_botnav_why);
        }
        if(b.target1.classname == "waypoint")
        {
            vr_bn_way = 1;
        }
        float got = VR_WeaponUtil_EntHasWeapon(b, WID_ROCKET_LAUNCHER);
        if(!got && vr_bn_n < VR_BN_WALK)
        {
            return;
        }
        VR_Test_Check(got && vr_bn_way && !vr_bn_used, sprintf("vr_bot_navmesh 1, no player near: the bot takes the rocket "
            "launcher by its waypoints (taken %g in %g frames, target1 a waypoint %g, navmesh used %g)", got, vr_bn_n,
            vr_bn_way, vr_bn_used));
        VR_BN_End();
    }
};

void(float ask) VR_BN_Run =
{
    if(vr_bn_on != ask)
    {
        VR_Test_BeginFlags("dmtest", VR_TEST_LOUD | VR_TEST_ECHO);
        vr_bn_on = ask;
        vr_bn_nav = cvar_hget(cvarh_vr_bot_navmesh);
        vr_bn_case = 0;
        vr_bn_got[0] = vr_bn_got[1] = -1;
        vr_bn_s = 0;
        vr_bn_n = 0;
    }
    if(vr_bn_s < 0)
    {
        return;
    }
    vr_bn_n++;
    if(vr_bn_s == 0) // the human and a living bot; the map's start
    {
        float ok = VR_BT_Find(FALSE);
        if(!ok && vr_dmt_human && !vr_dmt_bot && vr_bn_n == 36)
        {
            entity os = self;
            self = vr_dmt_human;
            BotConnect(0, 0, cvar_hget(cvarh_skill)); // (no bot yet: one added, as his impulse 100 does)
            self = os;
        }
        if(!ok && (vr_bn_n < VR_BN_GIVEUP))
        {
            return;
        }
        entity st = find(world, classname, "info_player_start");
        ok = (ok && st != world && mapname == "e1m1");
        VR_Test_Check(ok, sprintf("e1m1 (%s), a human (%s), a living bot (%s), the map's start", mapname,
            vr_dmt_human.netname, vr_dmt_bot.netname));
        if(!ok)
        {
            VR_BN_End();
            return;
        }
        vr_bn_start = st.origin;
        makevectors(st.angles_y * '0 1 0');
        vr_bn_fwd = v_forward;
        vr_bn_seen = vr_bn_start + vr_bn_fwd * 160;
        vr_bn_past = '96 576 25'; // (mp_test.py navfollow's NAVFOLLOW_TARGET: past the west door, x 209-255)
        vr_bn_s = 1;
        vr_bn_n = 0;
    }
    if(ask == 19) { VR_BN_Door(); }
    if(ask == 20) { VR_BN_Items(); }
};

void() VR_BotNavTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if(ask >= 19 && ask <= 20) { VR_BN_Run(ask); }
};
#endif
