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
#endif
