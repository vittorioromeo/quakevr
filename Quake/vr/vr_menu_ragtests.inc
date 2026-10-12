// vr_menu_ragtests.inc -- Debug > Tests > Ragdoll Behaviours (vr_ragtest.cpp, vr_ragdoll_act; ROUND22.md, "Ragdoll tests,
// tuning, self-collision"): each active-ragdoll behaviour set going on the nearest grunt, enforcer, knight, death knight,
// ogre or zombie standing, its rows vr_ragtest.cpp's table's. Included by vr_menu.cpp (inside its namespace).

[[nodiscard]] za::Vector<Item> pageRagdollTests()
{
    za::Vector<Item> items{
        header("Set Up"),
        command("A Grunt Ahead", "vr_test_spawn 0; vr_test_spawn_dist 110; impulse 241")
            .help("A grunt 110 units ahead, facing you: the tests below act on the nearest grunt, enforcer, knight, death "
                  "knight, ogre or zombie standing. The game runs behind this menu only while nothing hunts you (notarget: "
                  "Cheats and Recording); else a test begins as you close the menu."),
        command("An Enforcer Ahead", "vr_test_spawn 8; vr_test_spawn_dist 110; impulse 241"),
        command("A Knight Ahead", "vr_test_spawn 5; vr_test_spawn_dist 110; impulse 241"),
        command("A Death Knight Ahead", "vr_test_spawn 6; vr_test_spawn_dist 110; impulse 241"),
        command("An Ogre Ahead", "vr_test_spawn 1; vr_test_spawn_dist 110; impulse 241"),
        command("A Zombie Ahead", "vr_test_spawn 2; vr_test_spawn_dist 110; impulse 241")
            .help("A zombie dies whole only beheaded: its tests are the living ones and Beheaded."),
        command("Notarget On/Off", "notarget")
            .help("notarget: monsters don't see you (toggled; the console says which), so the game runs behind this menu."),
        command("Clean Slate", "vr_knockdown_test 20; vr_scene_clean all")
            .help("Every monster removed, standing, knocked down or dead, and the gibs, dropped weapons, blood and fires "
                  "cleaned up: nothing left lying where the next one falls."),
        command("Fold Corpse Arm Into Chest", "vr_ragdoll_arm_test")
            .help("Rotate one corpse arm and its elbow into the chest. Limbs Meet Each Other should free it again; "
                  "Ragdolls Now in the console reports buried arm points. Living monsters are skipped."),
        toggle("Draw Their Targets", vr_ragdoll_active_debug)
            .help("vr_ragdoll_active_debug: each driven ragdoll's joint targets over it (green as strong as the joint, grey "
                  "limp), its pelvis pin (yellow), its hands' and feet's goals (red), its probes and holds."),
        cycle("Log Each Behaviour", vr_debug_ragdoll, {{0.f, "Off"}, {1.f, "On"}, {2.f, "And Probes"}})
            .help("vr_debug_ragdoll: each ragdoll made and what its death does (stumbling, a hand on the wound, to crawl), "
                  "each catch, lean and ledge hold made and let go; And Probes: each probe too."),
        command("How Many Began", "vr_ragdoll_act status")
            .help("vr_ragdoll_act status: the tests fired so far, how many began, how many didn't (each said in the "
                  "console as it goes: ragact:)."),
        command("Active Ragdolls Now", "vr_ragdoll_active_list")
            .help("vr_ragdoll_active_list: each driven ragdoll: how long driven, its strengths, its pin, what it does."),
        open("Ragdoll Settings", pageIndex(pageRagdolls))
            .help("Gibs and Corpses > Ragdolls: each behaviour's strength, chance and time."),
    };
    const char* group = "";
    for(int i = 0; i < ragtest::rowCount(); i++)
    {
        const ragtest::Row r = ragtest::row(i);
        if(strcmp(group, r.group))
        {
            group = r.group;
            items.pushBack(header(group));
        }
        items.pushBack(command(r.label, r.command).help(r.help));
    }
    return items;
}
