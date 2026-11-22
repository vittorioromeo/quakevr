#ifdef QVR_TESTS
// vr_quoth_test.qc -- Quoth support (docs/vr-port/QUOTH.md): vr_ad_test 211 (quoth_maps.sh, on kellbase1, whose exit
// lock is a spawner; any map works): an entity used by its targetname2 and by its targetname3 through the aliases; the
// map's first spawner named its thing from spawnfunction (the engine) and makes it when used; a made spawner of two
// grunts makes both, each carrying its target. Ends with "quothkeys: N passed, M failed".

float vr_qt_uses;
float vr_qt_before;
entity vr_qt_map;  // the map's spawner (or world)
entity vr_qt_made; // the made grunt spawner

void() VR_QT_CountUse = { vr_qt_uses = vr_qt_uses + 1; };
float vr_qt_more;
void() VR_QT_CountMore = { vr_qt_more = vr_qt_more + 1; };

float() VR_QT_Grunts =
{
    local entity e;
    local float n;
    for (e = find(world, classname, "monster_army"); e; e = find(e, classname, "monster_army"))
        if (e.flags & FL_MONSTER && e.health > 0 && e.target == "vr_qt_wave")
            n = n + 1;
    return n;
};

void() VR_QT_SpawnersDone =
{
    if (vr_qt_map)
        VR_Test_Check(vr_qt_map.cnt == 0 && vr_qt_map.enemy && vr_qt_map.enemy.modelindex,
            sprintf("the map's spawner made its %g %s", vr_qt_map.count, vr_qt_map.spawnclassname));
    VR_Test_Check(VR_QT_Grunts() - vr_qt_before == 2,
        sprintf("the made spawner made 2 grunts carrying its target (%g)", VR_QT_Grunts() - vr_qt_before));
    VR_Test_Verdict();
    remove(self);
};

void(entity s) VR_QT_Use =
{
    local entity oself;
    oself = self;
    self = s;
    s.use();
    self = oself;
};

void(entity p) VR_Quoth_Test =
{
    local entity t, u, oself, w, s;
    VR_Test_BeginFlags("quothkeys", VR_TEST_LOUD); // (not ECHO: its localcmd would queue after a script's quit)
    t = spawn();
    t.classname = "vr_qt_target";
    t.use = VR_QT_CountUse;
    t.targetname2 = "vr_qt_second";
    t.targetname3 = "vr_qt_third";
    vr_qt_uses = 0;
    vr_quoth_aliased = 0; // (this map's aliases once more: the old ones renamed first, so each entity keeps one)
    for (u = find(world, classname, "vr_quoth_alias"); u; u = find(u, classname, "vr_quoth_alias"))
        u.classname = "vr_qt_oldalias";
    for (u = find(world, classname, "vr_qt_oldalias"); u; u = find(u, classname, "vr_qt_oldalias"))
    {
        u.owner.targetname2 = "";
        u.owner.targetname3 = "";
    }
    w = spawn(); // (fired by its target3 and target4 through its relays)
    w.classname = "vr_qt_more";
    w.targetname = "vr_qt_more";
    w.use = VR_QT_CountMore;
    s = spawn();
    s.classname = "vr_qt_source";
    s.ad_target3 = s.ad_target4 = "vr_qt_more";
    vr_qt_more = 0;
    VR_Quoth_Frame();
    oself = self;
    self = s;
    activator = ad_activator = p;
    ad_SUB_UseTargets();
    self = oself;
    VR_Test_Check(vr_qt_more == 2, sprintf("its target3 and target4 both used (%g)", vr_qt_more));
    u = spawn();
    u.target = "vr_qt_second";
    oself = self;
    self = u;
    activator = p;
    SUB_UseTargets();
    VR_Test_Check(vr_qt_uses == 1, sprintf("used by its targetname2 once (%g)", vr_qt_uses));
    u.target = "vr_qt_third";
    SUB_UseTargets();
    self = oself;
    VR_Test_Check(vr_qt_uses == 2, sprintf("used by its targetname3 once (%g in all)", vr_qt_uses - 1));
    if (mapname == "kellbase1") // (its count3 is known only by its targetname3: the engine made that its name)
    {
        u = find(world, targetname, "count3");
        VR_Test_Check(u.classname == "trigger_counter" && u.targetname3 == "",
            sprintf("count3, named only by targetname3, kept as its targetname (%s)", u.classname));
    }
    vr_qt_map = find(world, classname, "info_multispawn");
    if (vr_qt_map)
    {
        VR_Test_Check(vr_qt_map.spawnfunction && vr_qt_map.spawnclassname != "" && vr_qt_map.spawnclassname != "monster",
            sprintf("the map's spawner named its thing from spawnfunction (%s)", vr_qt_map.spawnclassname));
        VR_QT_Use(vr_qt_map);
    }
    vr_qt_made = spawn();
    vr_qt_made.classname = "info_multispawn";
    vr_qt_made.spawnfunction = monster_army;
    vr_qt_made.spawnclassname = "monster_army";
    vr_qt_made.count = 2;
    vr_qt_made.target = "vr_qt_wave";
    makevectors(p.angles);
    setorigin(vr_qt_made, p.origin + v_forward * 160 + '0 0 8');
    oself = self;
    self = vr_qt_made;
    info_multispawn();
    self = oself;
    vr_qt_before = VR_QT_Grunts();
    VR_QT_Use(vr_qt_made);
    w = spawn();
    w.think = VR_QT_SpawnersDone;
    w.nextthink = time + 0.3 * 2 + 0.5;
};

// vr_ad_test 212 (quoth_play.sh): a map's flow driven as a player would, round by round (one a second): every trigger
// a player walks into touched by him (trigger_once/multiple and the shim's point triggers; never exits, teleports,
// hurts, pushes or jumps), every button he can press pressed once (touched, or shot if it takes damage; one still
// locked is tried again next round), every monster awake (solid, alive) killed by him, so its death fires its target
// (counters, waves, spawners). A goal targetname per map (its last stage) is watched by a sentinel that shares the name; ends when
// it is used (then two more rounds) or after 90 rounds (a FAIL: "timed out"). Ends with "quothplay: N passed, M failed".

float vr_qp_round;
float vr_qp_killed;
float vr_qp_touched;
float vr_qp_pressed; // buttons pressed (their .vr_qp_mark 3)
float vr_qp_goalused;
float vr_qp_after;   // rounds run since the goal was used
float vr_qp_made;    // monsters the spawners made (their .vr_qp_mark)
entity vr_qp_player;
.float vr_qp_mark;   // 1: a monster counted; 2: a trigger touched once (trigger_once: no more); 3: a button pressed

void() VR_QP_GoalUse = { vr_qp_goalused = vr_qp_goalused + 1; };

string() VR_QP_Goal =
{
    if (mapname == "ne_marb") return "keyblockmarb"; // its four waves' counters (wave1..wave4, keyblock)
    if (mapname == "metmon") return "phase4_relay";  // its stages' counters, the final power count
    if (mapname == "lpqsp1") return "voreo";         // its point trigger's target; hordes on the way
    if (mapname == "kellbase1") return "exitlock";   // the 16 kills that open its exit (the plasma gun's spawner)
    if (mapname == "e1m1quoth") return "ramproom_door";
    return "";
};

void() VR_QP_Round =
{
    local entity e, oself, oother;
    local float awake;
    if (vr_qp_player.classname != "player")
    {
        // (asked for while the level was still changing: the player found once he is in)
        vr_qp_player = VR_Player();
        if (!vr_qp_player)
        {
            self.nextthink = time + 0.5;
            return;
        }
    }
    vr_qp_player.flags = vr_qp_player.flags | FL_GODMODE;
    vr_qp_round = vr_qp_round + 1;
    // Monsters the spawners made since the last round (no spawn mark yet) and those placed (counted on the first round).
    for (e = nextent(world); e; e = nextent(e))
        if (e.flags & FL_MONSTER && !e.vr_qp_mark)
        {
            e.vr_qp_mark = 1;
            if (vr_qp_round > 1)
                vr_qp_made = vr_qp_made + 1;
        }
    // Walk into every trigger a player can.
    oself = self;
    oother = other;
    for (e = nextent(world); e; e = nextent(e))
        if (e.touch && e.vr_qp_mark != 2 && e.solid == SOLID_TRIGGER &&
            (e.classname == "trigger_once" || e.classname == "trigger_multiple" || e.classname == "trigger_once_point" ||
             e.classname == "trigger_secret" || e.classname == "trigger_secret_point"))
        {
            if (e.classname == "trigger_once" || e.classname == "trigger_once_point" || e.classname == "trigger_secret_point")
                e.vr_qp_mark = 2;
            self = e;
            other = vr_qp_player;
            activator = vr_qp_player;
            e.touch();
            vr_qp_touched = vr_qp_touched + 1;
        }
    // Press every button a player can, once: touched, or shot (one that takes damage is never touched). One that does
    // not move (locked until a stage unlocks it: AD's entity state) is tried again next round.
    for (e = find(world, classname, "func_button"); e; e = find(e, classname, "func_button"))
        if (e.vr_qp_mark != 3 && e.state == STATE_BOTTOM && (e.touch || e.takedamage))
        {
            if (e.takedamage)
            {
                self = oself;
                T_Damage(e, vr_qp_player, vr_qp_player, e.health + 1);
            }
            else
            {
                // (a hand's press: a button's own touch is a foot's, only for one pressed by stepping on it)
                self = e;
                other = vr_qp_player;
                activator = vr_qp_player;
                if (e.handtouch)
                    e.handtouch();
                else
                    e.touch();
            }
            if (e.state == STATE_UP)
            {
                e.vr_qp_mark = 3;
                vr_qp_pressed = vr_qp_pressed + 1;
            }
        }
    self = oself;
    other = oother;
    // Kill every monster awake.
    for (e = nextent(world); e; e = nextent(e))
        if (e.flags & FL_MONSTER && e.health > 0 && e.takedamage && e.solid != SOLID_NOT && e.modelindex)
        {
            awake = awake + 1;
            T_Damage(e, vr_qp_player, vr_qp_player, e.health + 100);
            vr_qp_killed = vr_qp_killed + 1;
        }
    if (vr_qp_goalused)
        vr_qp_after = vr_qp_after + 1;
    if (vr_qp_after >= 2 || vr_qp_round >= 90 || (VR_QP_Goal() == "" && vr_qp_round >= 30))
    {
        VR_Test_Check(vr_qp_killed > 0,
            sprintf("%s: %g monsters killed in %g rounds, %g made by spawners, %g triggers touched, %g buttons pressed",
            mapname, vr_qp_killed, vr_qp_round, vr_qp_made, vr_qp_touched, vr_qp_pressed));
        if (VR_QP_Goal() != "")
            VR_Test_Check(vr_qp_goalused > 0, sprintf("%s: its last stage (%s) %s", mapname, VR_QP_Goal(),
                vr_qp_goalused ? sprintf("reached in round %g", vr_qp_round - vr_qp_after) : "timed out"));
        if (VR_QP_Goal() != "" && !vr_qp_goalused) // (where the flow stopped: each counter's uses still to come, and
        {                                           // whatever targets the goal)
            for (e = find(world, classname, "trigger_counter"); e; e = find(e, classname, "trigger_counter"))
                bprint(sprintf("quothplay: counter %s (%s) %g of %g left, state %g\n", e.targetname, e.target, e.count,
                    e.height, e.ad_estate));
            for (e = find(world, target, VR_QP_Goal()); e; e = find(e, target, VR_QP_Goal()))
                bprint(sprintf("quothplay: source %s %s: solid %g, touch %g, use %g, state %g, size %v\n", e.classname,
                    e.targetname, e.solid, e.touch != SUB_Null && e.touch != __NULL__, e.use != __NULL__, e.ad_estate,
                    e.size));
        }
        VR_Test_Check(vr_qp_player.health > 0, "the player alive (god)");
        VR_Test_Verdict();
        localcmd("qp_next\n"); // (quoth_play.sh's next step, its alias; elsewhere an unknown command)
        remove(self);
        return;
    }
    self.nextthink = time + 1;
};

void(entity p) VR_Quoth_Play =
{
    local entity s, w;
    VR_Test_BeginFlags("quothplay", VR_TEST_LOUD);
    vr_qp_player = p;
    vr_qp_round = vr_qp_killed = vr_qp_touched = vr_qp_pressed = vr_qp_goalused = vr_qp_after = vr_qp_made = 0;
    if (VR_QP_Goal() != "")
    {
        s = spawn();
        s.classname = "vr_qp_goal";
        s.targetname = VR_QP_Goal();
        s.use = VR_QP_GoalUse;
    }
    w = spawn();
    w.classname = "vr_qp_driver";
    w.think = VR_QP_Round;
    w.nextthink = time + 0.5;
};

// ---- vr_ad_test 223 (kellbase1; quothfield_test.sh): his note kellbase1 2026-11-22 16:04 ("an energy field ... I
// cannot see ... an enemy behind it ... tries to path through it constantly and gets hit"): the map's hurting
// togglewall draws its particle field (its emitter made, emitting with you near), and an armed enforcer behind it,
// chasing you on its other side for 20 s of game time, touches it at most twice. Ends "quothfield: N passed, M failed".
entity vr_qf_wall;
entity vr_qf_foe;
entity vr_qf_player;
float vr_qf_end;
float vr_qf_hits0;
float vr_qf_sparks0;

// `at` dropped to the floor below it, as a 24-unit-high standing box's origin.
vector(vector at) VR_QF_Floor =
{
    traceline(at, at - '0 0 256', TRUE, world);
    return trace_endpos + '0 0 24.03';
};

void() VR_QF_Think =
{
    if (time < vr_qf_end)
    {
        if (vr_qf_foe.health > 0 && vr_qf_foe.enemy != vr_qf_player)
        {
            local entity os = self;
            self = vr_qf_foe;
            self.enemy = vr_qf_player;
            FoundTarget();
            self = os;
        }
        self.nextthink = time + 0.1;
        return;
    }
    local float hits = vr_twall_monhits - vr_qf_hits0;
    VR_Test_Check(hits <= 2, sprintf("the enforcer behind it touched it %g times in 20 s (at most 2), health %g, at %v",
        hits, vr_qf_foe.health, vr_qf_foe.origin));
    VR_Test_Check(vr_qf_player.health > 0, "the player alive (god)");
    local entity fx = find(world, classname, "vr_twall_fx");
    VR_Test_Check(fx && fx.owner == vr_qf_wall && fx.color == 47 && fx.count == 50,
        sprintf("its particle field made (Quoth's spawnflag 2: colour %g, %g sparks a tenth)", fx.color, fx.count));
    VR_Test_Check(vr_twall_sparks - vr_qf_sparks0 > 1000, sprintf("its sparks drawn with you near: %g in 20 s",
        vr_twall_sparks - vr_qf_sparks0));
    VR_Test_Verdict();
    remove(self);
};

void(entity p) VR_Quoth_FieldTest =
{
    local entity e, w;
    local vector c, sz, n;
    VR_Test_BeginFlags("quothfield", VR_TEST_LOUD);
    vr_qf_player = p;
    vr_qf_wall = world;
    for (e = nextent(world); e; e = nextent(e))
        if (e.ad_classtype == ad_CT_FUNCTOGWALL && e.dmg > 0 && e.ad_estate == ad_ESTATE_ON)
            vr_qf_wall = e;
    if (!vr_qf_wall)
    {
        VR_Test_Check(FALSE, "a hurting togglewall on this map (kellbase1)");
        VR_Test_Verdict();
        return;
    }
    c = (vr_qf_wall.absmin + vr_qf_wall.absmax) * 0.5;
    sz = vr_qf_wall.absmax - vr_qf_wall.absmin;
    n = '0 0 1';
    if (sz_x <= sz_y && sz_x <= sz_z)
        n = '1 0 0';
    else if (sz_y <= sz_z)
        n = '0 1 0';
    setorigin(p, VR_QF_Floor(c + n * 96));
    p.angles = p.v_angle = vectoangles(-n);
    p.fixangle = TRUE;
    e = spawn();
    setorigin(e, VR_QF_Floor(c - n * 96));
    e.angles_y = vectoyaw(n);
    e.weapon = WID_ENFRIFLE;
    w = self;
    self = e;
    VR_AM_Spawn(VR_AM_ENFORCER);
    self = w;
    e.health = 1000;
    vr_qf_foe = e;
    vr_qf_hits0 = vr_twall_monhits;
    vr_qf_sparks0 = vr_twall_sparks;
    vr_qf_end = time + 20;
    bprint(sprintf("quothfield: wall at %v size %v, you at %v, the enforcer at %v\n", c, sz, p.origin, e.origin));
    w = spawn();
    w.classname = "vr_qf_driver";
    w.think = VR_QF_Think;
    w.nextthink = time + 1;
};
#endif
