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
        row(opt::vr_ragtool_loop),
        row(opt::vr_ragtool_length),
        row(opt::vr_ragtool_dir),
        row(opt::vr_ragtool_hold),
        command("Grab or Let Go (G)", "vr_ragtool_grab").help("The hand takes Hold's part now (a ragdoll only), or lets go."),
        command("Hand Up", "vr_ragtool_hand 0 0 12"),
        command("Hand Down", "vr_ragtool_hand 0 0 -12"),
        header("Time"),
        command("Pause or Run (P)", "vr_ragtool_pause"),
        command("Single Step (.)", "vr_ragtool_step").help("One server frame, paused."),
        row(opt::vr_ragtool_speed),
        header("Show"),
        row(opt::vr_ragtool_show_bones),
        row(opt::vr_ragtool_show_limits),
        row(opt::vr_ragtool_show_targets),
        row(opt::vr_ragtool_show_contacts),
        row(opt::vr_ragtool_show_balance),
        toggle("Physics Shapes", vr_debug_physics_shapes),
        row(opt::vr_ragtool_cam_dist),
        row(opt::vr_ragtool_cam_shift),
        row(opt::vr_ragtool_log),
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
