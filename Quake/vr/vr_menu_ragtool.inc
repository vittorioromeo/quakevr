// vr_menu_ragtool.inc -- Debug > Ragdoll Tool (vr_ragtool.cpp, docs/vr-port/RAGTOOL.md): the ragdoll tool's panel (Tab in
// the tool), its subject, its behaviour and how it is played, what is drawn, and its registry's settings by group (each
// group a page), with the presets. Included by vr_menu.cpp (inside its namespace).

[[nodiscard]] za::Vector<Choice> ragtoolModelChoices()
{
    za::Vector<Choice> out;
    for(int i = 0; i < ragtool::modelCount(); i++)
    {
        out.pushBack({static_cast<float>(i), ragtool::modelLabel(i)});
    }
    if(out.empty())
    {
        out.pushBack({0.f, "(none: start the tool)"});
    }
    return out;
}

[[nodiscard]] za::Vector<Choice> ragtoolBehaviourChoices()
{
    za::Vector<Choice> out;
    for(int i = 0; i < ragtool::behaviourCount(); i++)
    {
        out.pushBack({static_cast<float>(i), ragtool::behaviourLabel(i)});
    }
    return out;
}

[[nodiscard]] za::Vector<Item> ragtoolParamRows(int group)
{
    za::Vector<Item> items{header(ragtool::groupName(group))};
    for(int i = 0; i < ragtool::paramCount(); i++)
    {
        const ragtool::Param& p = ragtool::param(i);
        if(p.group != group)
        {
            continue;
        }
        items.pushBack(slider(p.label, p.cvar, p.min, p.max, p.step, p.format)
                           .help(p.respawn ? "Set as the ragdoll is made: Respawn (R) to see it." : "Live: the next frame's drive."));
    }
    items.pushBack(header("Presets"));
    constexpr const char* resets[] = {"vr_ragtool_reset 0", "vr_ragtool_reset 1", "vr_ragtool_reset 2",
        "vr_ragtool_reset 3", "vr_ragtool_reset 4", "vr_ragtool_reset 5", "vr_ragtool_reset 6", "vr_ragtool_reset 7",
        "vr_ragtool_reset 8"};
    if(group >= 0 && group < static_cast<int>(sizeof(resets) / sizeof(resets[0])))
    {
        items.pushBack(command("Reset This Group", resets[group]).help("This group's settings back to their defaults."));
    }
    items.pushBack(command("Save Preset", "vr_ragtool_save").help("Every registry setting to ragtool/preset.cfg."));
    return items;
}

template<int Group>
[[nodiscard]] za::Vector<Item> pageRagtoolParams()
{
    return ragtoolParamRows(Group);
}

[[nodiscard]] const char* ragtoolStatus()
{
    return ragtool::statusLine();
}

[[nodiscard]] za::Vector<Item> pageRagtool()
{
    return {
        info(ragtoolStatus),
        header("Tool"),
        command("Start (Load the Room)", "vr_ragtool")
            .help("vr_ragtool: loads vr_ragtool_map (Ragdoll Triggers) and spawns the subject; `vr_ragtool here` keeps the "
                  "map you are in. Launch flat with -ragtool. In the game: the mouse orbits, the wheel zooms, Tab this page."),
        command("Start Here", "vr_ragtool here").help("The tool in this map, the subject ahead of where you stand."),
        command("Exit", "vr_ragtool_exit").help("Back to the game: the camera, god mode and notarget, the time scale."),
        header("Subject"),
        cycle("Model", vr_ragtool_model, ragtoolModelChoices())
            .help("The toolgun's monsters with a ragdoll rig (id1, the expansions, AD's when it is installed). Respawn to "
                  "make it (M in the game: the next one, made at once)."),
        command("Respawn (R)", "vr_ragtool_spawn").help("A fresh one standing at the stage, every other monster removed."),
        header("Behaviour"),
        cycle("Behaviour", vr_ragtool_behaviour, ragtoolBehaviourChoices())
            .help("What Play sets going (B in the game: the next). Each is the game's own call (vr_ragdoll_act, "
                  "vr_knockdown_test, QC's parry, the physgun's hold as a hand)."),
        command("Play (Space)", "vr_ragtool_play").help("A fresh subject, then the behaviour; its summary logged (ragtool:)."),
        toggle("Loop (L)", vr_ragtool_loop).help("Played again when it ends."),
        slider("Watch For (s)", vr_ragtool_length, 1.f, 20.f, 0.5f, "%.1f")
            .help("How long a run lasts when its behaviour gives no length of its own."),
        slider("Push From (deg)", vr_ragtool_dir, 0.f, 315.f, 45.f, "%.0f")
            .help("Where a shove, a blow, a knockdown or a parry comes from: 0 his front, 90 his left, 180 his back."),
        cycle("Hold", vr_ragtool_hold, {{0.f, "Chest"}, {1.f, "Head"}, {2.f, "Arm"}, {3.f, "Leg"}, {4.f, "Pelvis"}})
            .help("The virtual hand's hold for Struggle, Lift and Drag (H in the game). Move it: the arrows, Page Up/Down."),
        command("Grab or Let Go (G)", "vr_ragtool_grab").help("The hand takes Hold's part now (a ragdoll only), or lets go."),
        command("Hand Up", "vr_ragtool_hand 0 0 12"),
        command("Hand Down", "vr_ragtool_hand 0 0 -12"),
        header("Time"),
        command("Pause or Run (P)", "vr_ragtool_pause"),
        command("Single Step (.)", "vr_ragtool_step").help("One server frame, paused."),
        slider("Speed ([ ])", vr_ragtool_speed, 0.05f, 2.f, 0.05f, "%.2fx").help("The time scale while the tool runs."),
        header("Show"),
        toggle("Skeleton and Frames", vr_ragtool_show_bones),
        toggle("Joint Limits", vr_ragtool_show_limits).help("Cyan: a ball joint's cone; orange: a hinge's arc; magenta: the bone now."),
        toggle("Motor Targets", vr_ragtool_show_targets)
            .help("Green: each joint's target, as bright as it is strong (grey limp); yellow: the pelvis pin; red: a hand's or foot's goal."),
        toggle("Contacts", vr_ragtool_show_contacts).help("Red: pressed into the world; pink: touching."),
        toggle("Centre of Mass and Support", vr_ragtool_show_balance)
            .help("Yellow: the centre of mass; its point on the floor green over the feet's support (its outline), red off it."),
        toggle("Physics Shapes", vr_debug_physics_shapes),
        slider("Camera Distance (Wheel)", vr_ragtool_cam_dist, 24.f, 600.f, 8.f, "%.0f"),
        slider("Subject Off Centre", vr_ragtool_cam_shift, -0.6f, 0.6f, 0.05f, "%.2f"),
        cycle("Log", vr_ragtool_log, {{0.f, "Off"}, {1.f, "Each Run"}, {2.f, "And Samples"}}),
        header("Parameters"),
        open("Motors", pageIndex(pageRagtoolParams<0>)),
        open("Joint Groups", pageIndex(pageRagtoolParams<1>)),
        open("Stagger and Balance", pageIndex(pageRagtoolParams<2>)),
        open("Shove", pageIndex(pageRagtoolParams<3>)),
        open("Knockdown and Get-Up", pageIndex(pageRagtoolParams<4>)),
        open("Death and Crawl", pageIndex(pageRagtoolParams<5>)),
        open("Struggle", pageIndex(pageRagtoolParams<6>)),
        open("Parry and Hit Stagger", pageIndex(pageRagtoolParams<7>)),
        open("Flail, Liquids and Hand", pageIndex(pageRagtoolParams<8>)),
        header("Presets"),
        command("Save Preset", "vr_ragtool_save").help("vr_ragtool_save [name]: every registry setting to ragtool/<name>.cfg."),
        command("Load Preset", "vr_ragtool_load").help("vr_ragtool_load [name]: back from ragtool/<name>.cfg."),
        command("Reset All to Defaults", "vr_ragtool_reset all"),
        command("List Settings", "vr_ragtool_params").help("vr_ragtool_params [group]: each with its default if changed."),
        command("Status", "vr_ragtool_status"),
    };
}
