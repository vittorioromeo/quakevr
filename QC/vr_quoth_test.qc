#ifdef QVR_TESTS
// vr_quoth_test.qc -- Quoth support (docs/vr-port/QUOTH.md): vr_ad_test 211 (quoth_maps.sh, on kellbase1, whose exit
// lock is a spawner; any map works): an entity used by its targetname2 through the alias; the map's first spawner
// named its thing from spawnfunction (the engine) and makes it when used; a made spawner of two grunts makes both,
// each carrying its target. Ends with "quothkeys: N passed, M failed".

float vr_qt_uses;
float vr_qt_before;
entity vr_qt_map;  // the map's spawner (or world)
entity vr_qt_made; // the made grunt spawner

void() VR_QT_CountUse = { vr_qt_uses = vr_qt_uses + 1; };

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
    local entity t, u, oself, w;
    VR_Test_BeginFlags("quothkeys", VR_TEST_LOUD); // (not ECHO: its localcmd would queue after a script's quit)
    t = spawn();
    t.classname = "vr_qt_target";
    t.use = VR_QT_CountUse;
    t.targetname2 = "vr_qt_second";
    vr_qt_uses = 0;
    vr_quoth_aliased = 0; // (this map's aliases once more: the old ones renamed first, so each entity keeps one)
    for (u = find(world, classname, "vr_quoth_alias"); u; u = find(u, classname, "vr_quoth_alias"))
        u.classname = "vr_qt_oldalias";
    for (u = find(world, classname, "vr_qt_oldalias"); u; u = find(u, classname, "vr_qt_oldalias"))
        u.owner.targetname2 = "";
    VR_Quoth_Frame();
    u = spawn();
    u.target = "vr_qt_second";
    oself = self;
    self = u;
    activator = p;
    SUB_UseTargets();
    self = oself;
    VR_Test_Check(vr_qt_uses == 1, sprintf("used by its targetname2 once (%g)", vr_qt_uses));
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
// hurts, pushes or jumps), every monster awake (solid, alive) killed by him, so its death fires its target (counters,
// waves, spawners). A goal targetname per map (its last stage) is watched by a sentinel that shares the name; ends when
// it is used (then two more rounds) or after 90 rounds (a FAIL: "timed out"). Ends with "quothplay: N passed, M failed".

float vr_qp_round;
float vr_qp_killed;
float vr_qp_touched;
float vr_qp_goalused;
float vr_qp_after;   // rounds run since the goal was used
float vr_qp_made;    // monsters the spawners made (their .vr_qp_mark)
entity vr_qp_player;
.float vr_qp_mark;   // 1: a monster counted; 2: a trigger touched once (trigger_once: no more)

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
        VR_Test_Check(vr_qp_killed > 0, sprintf("%s: %g monsters killed in %g rounds, %g made by spawners, %g triggers touched",
            mapname, vr_qp_killed, vr_qp_round, vr_qp_made, vr_qp_touched));
        if (VR_QP_Goal() != "")
            VR_Test_Check(vr_qp_goalused > 0, sprintf("%s: its last stage (%s) %s", mapname, VR_QP_Goal(),
                vr_qp_goalused ? sprintf("reached in round %g", vr_qp_round - vr_qp_after) : "timed out"));
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
    vr_qp_round = vr_qp_killed = vr_qp_touched = vr_qp_goalused = vr_qp_after = vr_qp_made = 0;
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
#endif
