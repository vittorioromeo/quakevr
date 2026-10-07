
// vr_menu_cheats.inc -- Debug > Cheats and Recording (docs/vr-port/ROUND21.md, "Cheats and Recording"): Quake's cheats
// and a scene set up for footage: saved and loaded, cleaned (vr_scene_clean: vr_cheats.cpp, QC vr_cheats.qc), slowed,
// paused, the gadget and the hands hidden. Settings with a home elsewhere (the spawner's Thing, slow motion's, the
// recording's) are linked, not repeated. Included by vr_menu.cpp (inside its namespace: vr_cheats.hpp is included there).

[[nodiscard]] const char* cheatState()
{
    return qvr::cheats::stateLine();
}

[[nodiscard]] za::Vector<Item> pageDebugCheats()
{
    return {
        info(cheatState),
        header("Cheats (single player and co-op)"),
        command("God Mode", "god").help("god: no damage taken. On, or off again."),
        command("Noclip", "noclip")
            .help("noclip: fly through walls. In the headset the stick flies you as it walks you, level along your head's "
                  "way, up or down as the moving hand points (as swimming). Off again where you can stand."),
        command("Notarget", "notarget").help("notarget: the monsters don't see you (one already after you keeps on). On, or off."),
        command("Fly", "fly").help("fly: no gravity, the walls still in the way; the stick flies you as noclip's. On, or off."),
        command("All Weapons, Ammo and Keys", "impulse 9")
            .help("impulse 9: every weapon (the mission packs' too), full ammunition (the mission packs' where their game is "
                  "installed), the silver and gold keys."),
        command("Full Health and Armour", "impulse 194").help("impulse 194: 100 health (or more, kept) and the red armour (200)."),
        command("Die Now", "impulse 195").help("impulse 195: you die at once (armour, god mode and the Pentagram set aside; not gibbed): to try what death does, the gear hidden (Hide Gear When Dead). Respawn or load a save."),
        toggle("Infinite Ammo", vr_cheat_ammo)
            .help("vr_cheat_ammo: your ammunition kept full, and the magazine of the weapon in either hand (not an enemy's "
                  "gun's, nor the chainsaw's fuel). Not saved."),
        header("Powerups"),
        command("Quad Damage", "impulse 190").help("impulse 190: Quad Damage for an hour, or, while you have it, off at once."),
        command("Pentagram of Protection", "impulse 191").help("impulse 191: the Pentagram for an hour, or off at once."),
        command("Ring of Shadows", "impulse 192").help("impulse 192: the Ring for an hour, or off at once."),
        command("Biosuit", "impulse 193").help("impulse 193: the Biosuit for an hour, or off at once."),
        command("Wetsuit (Armagon)", "impulse 200").help("impulse 200: Scourge of Armagon's wetsuit for 30 s (single player)."),
        command("Empathy Shields (Armagon)", "impulse 201")
            .help("impulse 201: Scourge of Armagon's empathy shields for 30 s (single player)."),
        header("Monsters"),
        command("Kill All Monsters", "impulse 205").help("impulse 205: every living monster killed (counted as kills). Single player."),
        toggle("Freeze Monsters", vr_freeze_monsters)
            .help("vr_freeze_monsters: the living monsters stand frozen mid-step, mid-swing: no thinking, no moving (their "
                  "missiles fly on). A blow still hurts them, and kills them. Off: each goes on where it stopped. Not saved."),
        cycle("Skill", "skill", {{0.f, "Easy"}, {1.f, "Normal"}, {2.f, "Hard"}, {3.f, "Nightmare"}})
            .help("skill: from the next map, or Restart the Map below."),
        header("Scene: Save and Load"),
        command("Quick Save", "save quick").help("save quick: the quick save (as the keyboard's)."),
        command("Quick Load", "load quick").help("load quick: back to the quick save."),
        command("Save the Scene", "save scene")
            .help("save scene: a slot of its own for a shot set up (scene.sav), which quick saves don't overwrite."),
        command("Load the Scene", "load scene").help("load scene: back to the scene saved, to take the shot again."),
        command("Restart the Map", "restart").help("restart: the map from its start (at the Skill above)."),
        header("Scene: Clean Up (what you hold is kept)"),
        command("Count What's Left", "vr_scene_count")
            .help("vr_scene_count: the gibs, heads, limbs, small gibs and brains, corpses, props made since the map loaded "
                  "and fires lying about; the decals, wound masks, particles, casings and explosion debris (console)."),
        command("Gibs, Heads and Limbs", "vr_scene_clean gibs")
            .help("vr_scene_clean gibs: every gib, head, severed limb, small gib and brain chunk, and the stumps' fountains."),
        command("Corpses and Ragdolls", "vr_scene_clean corpses")
            .help("vr_scene_clean corpses: every dead monster (its ragdoll with it), and the players' old bodies."),
        command("Props Made Since the Map Loaded", "vr_scene_clean props")
            .help("vr_scene_clean props: the loose things made since the map loaded: dropped weapons and backpacks, crate "
                  "pieces, spawned boxes and crates, grenades. The map's own (and its crates, rocks and bricks) stay."),
        command("Fires and Smoke", "vr_scene_clean fires").help("vr_scene_clean fires: every fire put out, the smoke gone."),
        command("Decals", "vr_scene_clean decals")
            .help("vr_scene_clean decals: the blood, scorches and bullet marks on the walls and floors, the pools and drips."),
        command("Wounds and Blood on Bodies", "vr_scene_clean wounds")
            .help("vr_scene_clean wounds: the wounds and blood painted on the monsters and bodies, and on your hands, arms "
                  "and gear."),
        command("Particles and Casings", "vr_scene_clean effects")
            .help("vr_scene_clean effects: the particles, spent casings, explosion debris, smoke and lights in the air."),
        command("Everything", "vr_scene_clean all").help("vr_scene_clean all: all of the above."),
        header("Scene: Set Up"),
        command("Put a Monster or Prop Ahead", "impulse 241")
            .help("impulse 241: the Thing chosen on Debug > Tests (Ahead of You: Thing, Distance, As a Corpse) put ahead of you."),
        open("Choose the Thing: Tests", pageIndex(pageDebugTests)).help("Debug > Tests > Ahead of You."),
        open("Spawn Pickup Weapons", pageIndex(pageSpawnWeapons)),
        header("Time"),
        command("Normal Speed", "vr_slowmo 1").help("vr_slowmo 1: the game at its own speed."),
        command("Half Speed", "vr_slowmo 0.5").help("vr_slowmo 0.5: slow motion (Graphics > Recording > Time Scale)."),
        command("Quarter Speed", "vr_slowmo 0.25").help("vr_slowmo 0.25."),
        command("Tenth Speed", "vr_slowmo 0.1").help("vr_slowmo 0.1."),
        toggle("Pause the World", "sv_freezenonclients")
            .help("sv_freezenonclients: everything but you stands still: monsters, missiles, gibs, physics, the game's "
                  "clock. You move (noclip to place yourself), the world waits. Single player. Not saved."),
        header("Clean Shot and Recording"),
        cycle("Hide", vr_shot_hide,
            {{0.f, "Nothing"}, {1.f, "Gadget"}, {2.f, "Hands"}, {3.f, "Gadget and Hands"}, {4.f, "Body"},
             {5.f, "Gadget and Body"}, {6.f, "Hands and Body"}, {7.f, "Gadget, Hands, Body"}})
            .help("vr_shot_hide: not drawn, for a clean shot: the wrist gadget (its screen and messages), your hands and "
                  "what they hold, your body and holsters. They work on unseen. Not saved."),
        open("Recording: Window View, Spectator, Slow Motion", pageIndex(pageRecording))
            .help("Graphics > Recording: what the desktop window shows (the mirror, the spectator camera, their HUD), slow "
                  "motion's settings, the highlight markers."),
        command("Mark This Moment", "vr_highlight_mark").help("vr_highlight_mark: a highlight marker now (Log Highlights on)."),
    };
}
