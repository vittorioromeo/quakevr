
// vr_menu_toolgun.inc -- the toolgun's menu (vr_toolgun.cpp; docs/vr-port/TOOLGUN.md): VR Settings pages shown on a small
// panel over the gun while it is held (B/Y opens and closes it; vr_panel.cpp places it), pointed at with the other hand's
// laser. Its root's Back closes it (goBack). The cheats are Debug > Cheats and Recording's own rows (the same commands
// and help), the commonest. Included by vr_menu.cpp (inside its namespace).

[[nodiscard]] const char* toolgunState()
{
    return qvr::toolgun::stateLine();
}

[[nodiscard]] za::Vector<Item> pageToolgunSpawn();
[[nodiscard]] za::Vector<Item> pageToolgunCheats();

[[nodiscard]] za::Vector<Item> pageToolgun()
{
    return {
        info(toolgunState),
        cycle("Tool", vr_toolgun_tool, {{0.f, "Spawn"}, {1.f, "Remove"}, {2.f, "Physgun"}, {3.f, "Scale"}, {4.f, "Joint"}})
            .help("What the trigger does. Spawn: makes the thing chosen where the ghost stands. Remove: removes what glows "
                  "red. Physgun: hold the trigger to drag what you point at; the other hand's trigger freezes it there. "
                  "Scale: aim at a handle of the prop's box and drag it. Joint: shoot two props to join them."),
        header("Spawn"),
        open("Choose What to Spawn", pageIndex(pageToolgunSpawn)).help("Monsters, props, weapons and items."),
        command("Pick It From the World", "vr_toolgun_pick")
            .help("vr_toolgun_pick: the next shot chooses what it hits as the thing to spawn (a weapon lying about, a prop, a "
                  "monster)."),
        command("Clear the Choice", "vr_toolgun_select clear"),
        command("Reset Offsets and Turn", "vr_toolgun_reset")
            .help("vr_toolgun_reset: the ghost back where the gun points, facing you. Hold X or A (the toolgun's hand) and "
                  "move the sticks to move and turn it."),
        cycle("Sticks Turn It About", vr_toolgun_turn_axis, {{0.f, "Pitch"}, {1.f, "Yaw"}, {2.f, "Roll"}})
            .help("X or A held: the toolgun's stick left and right turns the ghost, or what the physgun holds, about this "
                  "axis; forward and back pushes it away and pulls it in. The other stick moves it up, down and aside."),
        header("Physgun and Scale"),
        command("Unfreeze Everything", "vr_toolgun_unfreeze_all")
            .help("vr_toolgun_unfreeze_all: every prop the physgun froze falls again."),
        toggle("Proportional Scaling", vr_toolgun_scale_uniform)
            .help("The scale tool's face handles scale the whole prop (on), or stretch it along that face's axis (off). "
                  "Corners always scale it whole. X or A held: the toolgun's stick forward and back grows and shrinks it."),
        command("Reset Its Size", "vr_toolgun_scale_reset").help("vr_toolgun_scale_reset: the prop aimed at back to its size."),
        header("Joint"),
        cycle("Joint", vr_toolgun_joint,
            {{0.f, "Weld"}, {1.f, "Ball"}, {2.f, "Hinge"}, {3.f, "Slider"}, {4.f, "Rope"}, {5.f, "Spring"}, {6.f, "Unjoin"}})
            .help("Shoot a prop, then another: weld (stuck as they are), ball (turning about the second point), hinge "
                  "(turning about the second face's normal), slider (along it), rope (no further apart than now), spring. "
                  "Unjoin: a prop shot loses its joints."),
        command("Remove Every Joint", "vr_toolgun_unjoin_all"),
        header("More"),
        open("Cheats and Utilities", pageIndex(pageToolgunCheats)),
        slider("Menu Size", vr_toolgun_menu_height, 4.f, 16.f, 0.5f, "%.1f").help("This menu's height on the gun, in units."),
    };
}

// The spawn list: a row each (the chosen one marked), under its section's header.
[[nodiscard]] const char* toolgunSpawnLabel(int i)
{
    return qvr::toolgun::spawnableLabel(i);
}

void toolgunSpawnPick(int i)
{
    qvr::toolgun::selectSpawnable(i);
}

za::Vector<Item> pageToolgunSpawn()
{
    za::Vector<Item> list;
    list.pushBack(info(toolgunState));
    const char* section = nullptr;
    for(int i = 0; i < qvr::toolgun::spawnableCount(); i++)
    {
        const char* s = qvr::toolgun::spawnableSection(i);
        if(!section || strcmp(section, s))
        {
            list.pushBack(header(s));
            section = s;
        }
        list.pushBack(row(toolgunSpawnLabel, nullptr, toolgunSpawnPick, i, -1));
    }
    return list;
}

// The cheats page's rows worth having at hand (by their labels: their commands and help are its own).
constexpr const char* toolgunCheatLabels[] = {"God Mode", "Noclip", "Notarget", "Fly", "All Weapons, Ammo and Keys",
    "Full Health and Armour", "Infinite Ammo", "Kill All Monsters", "Freeze Monsters", "Die Now", "Quick Save", "Quick Load",
    "Restart the Map", "Props Made Since the Map Loaded", "Corpses and Ragdolls", "Everything", "Normal Speed", "Half Speed",
    "Quarter Speed", "Pause the World"};

za::Vector<Item> pageToolgunCheats()
{
    za::Vector<Item> list;
    list.pushBack(info(cheatState));
    za::Vector<Item> all = pageDebugCheats();
    const Item* pendingHeader = nullptr;
    for(const Item& item : all)
    {
        if(item.kind == Item::Header)
        {
            pendingHeader = &item;
            continue;
        }
        bool wanted = false;
        for(const char* label : toolgunCheatLabels)
        {
            wanted = wanted || (item.label && !strcmp(item.label, label));
        }
        if(!wanted)
        {
            continue;
        }
        if(pendingHeader)
        {
            list.pushBack(*pendingHeader);
            pendingHeader = nullptr;
        }
        list.pushBack(item);
    }
    return list;
}
