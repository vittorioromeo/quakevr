// vr_menu_debug_tests.inc -- Debug > Tests (and the spawn, horde and MG3 test pages; Automation is
// vr_menu_automation.inc, included from here). Part of vr_menu.cpp (included where it was: the same translation unit,
// so its anonymous namespace).

// "Stamina 0.40: heavy x1.35, empty hand 0.8 kg, shake 0.21, run x0.98": the tired arms and legs now (Tests page).
[[nodiscard]] const char* staminaReadout()
{
    char(&text)[96] = readouts.stamina;
    q_snprintf(text, sizeof(text), "Stamina %.2f: heavy x%.2f, empty hand %.1f kg, shake %.2f, run x%.2f",
        fatigue::staminaLeft(), weight::staminaMultiplier(), za::max(vr_weight_stamina_empty.value, 0.f) * weight::staminaShare(),
        fatigue::shakeLevel(), fatigue::speedScaleFor(fatigue::staminaLeft()));
    return text;
}

// What tests are done with in the headset (single player).
za::Vector<Item> pageSpawnWeapons()
{
    return adSpawnItems({
        header("Grabbable Pickups Ahead of You"),
        command("Shotgun", "vr_physics_spawn weapon_shotgun reach"),
        command("Super Shotgun", "vr_physics_spawn weapon_supershotgun reach"),
        command("Nailgun", "vr_physics_spawn weapon_nailgun reach"),
        command("Super Nailgun", "vr_physics_spawn weapon_supernailgun reach"),
        command("Grenade Launcher", "vr_physics_spawn weapon_grenadelauncher reach"),
        command("Rocket Launcher", "vr_physics_spawn weapon_rocketlauncher reach"),
        command("Lightning Gun", "vr_physics_spawn weapon_lightning reach"),
        command("Crowbar", "vr_physics_spawn weapon_crowbar reach"),
        command("Mjolnir", "vr_physics_spawn weapon_mjolnir reach"),
        command("Laser Gun", "vr_physics_spawn weapon_laser_gun reach"),
        command("Proximity Gun", "vr_physics_spawn weapon_proximity_gun reach"),
        command("Toolgun", "vr_physics_spawn weapon_toolgun reach")
            .help("The toolgun (docs/vr-port/TOOLGUN.md): spawn, remove, physgun, scale and joint tools; B or Y opens its panel, its stick press switches the tool."),
    }, true);
}

za::Vector<Item> pageMachineHordeTests()
{
    return {
        header("Machine Horde Tests"),
        header("Reload the arena after destructive tests"),
        command("Acceptance", "vr_mg_horde_test 1"),
        command("Wave and Equipment Report", "vr_mg_horde_test 2"),
        command("Complete Current Wave", "vr_mg_horde_test 3"),
        command("Start Boss Wave", "vr_mg_horde_test 4"),
        command("Collect Spawned Key", "vr_mg_horde_test 5"),
        command("Death and Revival Check", "vr_mg_horde_test 6"),
        command("Drop Powerup", "vr_mg_horde_test 7"),
        command("Authored Keyed Button Check", "vr_mg_horde_test 8"),
        command("Reset Source Hunger Timer", "vr_mg_horde_test 9"),
        command("Activate Authored Deferred Monster", "vr_mg_horde_test 10"),
        command("Wave Monitor On", "vr_mg_horde_test 13")
            .help("Log each wave's squad budget and every squad's monsters to the console (developer 1)."),
        command("Wave Monitor Off", "vr_mg_horde_test 14"),
        command("All Players Report", "vr_mg_horde_test 15")
            .help("Coop: every player's health, death state, shared keys and frags (developer 1)."),
        command("Authored Keyed Door Check", "vr_mg_horde_test 18")
            .help("A real keyed door stays shut without shared keys, then spends exactly one and opens."),
        command("Team Wipe", "vr_mg_horde_test 17")
            .help("Destructive: every player dies; press fire to restart the arena (coop: revival needs a living teammate)."),
    };
}

// Dawn of the Machine (MG3) acceptance aids: its campaign only (Official Campaigns, or `vr_campaign_native mg3`), "mg3test:" lines
// with developer 1 (QC/vr_mg3_test.qc).
za::Vector<Item> pageMg3Tests()
{
    return {
        header("Dawn of the Machine Tests"),
        header("Dawn of the Machine campaign only (Official Campaigns)"),
        command("State Report", "vr_mg3_test 1")
            .help("Print the map, skill, serverflags and the player's health, caps, ammunition and weapons (developer 1)."),
        command("Seed Saved Upgrades", "vr_mg3_test 2")
            .help("Destructive: set every upgrade mask and both bloody weapon bits, for changelevel/save/death carry checks."),
        command("Add an Upgrade Bit", "vr_mg3_test 3")
            .help("Destructive: one more health upgrade bit this level; a death's restart must take it back."),
        command("Capacity Check", "vr_mg3_test 4")
            .help("Destructive, any campaign: print the health/ammo capacities, overfill and bound every ammunition, heal from 1."),
        command("Take This Map's Upgrades", "vr_mg3_test 5")
            .help("Destructive: empty health/ammo, take every capacity upgrade here by its real pickup, check masks, caps and refills."),
        command("Map Triggers Check", "vr_mg3_test 6")
            .help("Destructive, any campaign: silent teleports and door relays here, plus spawned always/multitouch/heal/music/quad/doorgroup/repeater/killmonster triggers."),
        command("Explosion Repeaters Check", "vr_mg3_test 7")
            .help("Destructive, any campaign: use every explosion repeater here (secret2: 60) and count their blasts 20 seconds later."),
        command("Monster Keys and Lore Check", "vr_mg3_test 8")
            .help("Destructive: a health_target monster hit down past its relay (fires once), an aggro_target group woken, a lore text shown and cleared."),
        command("Items Check", "vr_mg3_test 9")
            .help("Destructive: take every armour shard here, touch each draught, wear a lava suit in lava and slime, take the hell knight's head (Bloody Nightmare on)."),
        command("Take This Map's Runes", "vr_mg3_test 10")
            .help("Destructive: bring out a hidden rune and take every rune here by its pickup; prints serverflags."),
        command("Hub Rune Check", "vr_mg3_test 11")
            .help("On the hub: fire its rune check as entering does; 6 s later each rune's doors and the exit (all four runes) are checked."),
        command("Rune Count Report", "vr_mg3_test 12")
            .help("Monsters, items, intermission views and corpses here with this many runes (NOT_IF_n_RUNES removes the others)."),
        command("Walk Into the Exit", "vr_mg3_test 13")
            .help("Destructive: touch this map's first changelevel (its route); then Leave the Intermission."),
        command("Leave the Intermission", "vr_mg3_test 14")
            .help("One button press of the intermission (text, then the next map)."),
        command("Hub Skill Buttons Check", "vr_mg3_test 15")
            .help("On the hub: its Bloody Nightmare relays' state, then skill buttons 4, 1, 4 (Bloody Nightmare on, off, on)."),
        command("Seed a Full Loadout", "vr_mg3_test 16")
            .help("Destructive: guns, a sword, the shotgun and the Super Axe in the holsters, yellow armour, ammunition; then change level."),
        command("Loadout After Bloody Nightmare", "vr_mg3_test 17")
            .help("After a level change: in a Bloody Nightmare game only the axe, shotgun, Super Axe (and the bloody super shotgun) are left."),
        command("Bloody Nightmare Damage Check", "vr_mg3_test 18")
            .help("Destructive: 50 on a monster and 10 from it on you: 80% and 120% on Bloody Nightmare, else 100%."),
        command("Bloody Nightmare New Game Flags", "vr_mg3_test 19")
            .help("Destructive: Bloody Nightmare on, found and its new game (upstream impulses 223/224), skill 3."),
        command("Walk Into the Exit to secret2", "vr_mg3_test 20")
            .help("On the hub: touch its final exit; in Bloody Nightmare's new game it leads to boss2."),
        command("Chthon Beaten (Ending)", "vr_mg3_test 21")
            .help("Destructive: the Chthon ending as if he died: the finale text and the credits, or on Bloody Nightmare its new game on map1. Then Leave the Intermission."),
        command("Shub Beaten (Ending)", "vr_mg3_test 22")
            .help("Destructive: the Shub ending: the final text, then the credits. Then Leave the Intermission."),
        command("Route Carry Report", "vr_mg3_test 31")
            .help("Runes, health, ammunition, upgrade masks and capacities, hands' and holsters' weapons, magazines and ids: compare across a level change, save/load or death (the route sweep's report)."),
        command("Seed Hands and Holsters", "vr_mg3_test 32")
            .help("Destructive: shotgun and nailgun in the hands (magazines 3/7; hold both grips to carry them), six shotguns holstered (1..6), upgrade masks 5 2 8192 16384 1."),
        command("Give the Four Runes", "vr_mg3_test 36")
            .help("Destructive: all four runes (serverflags 15), e.g. to open the hub's exit to secret2 in a Bloody Nightmare new game."),
        slider("Ghosts' Opacity", vr_mg3_ghost_alpha, 0.05f, 1.f, 0.05f, "%.2f")
            .help("How solid Dawn of the Machine's ghosts look: 0.4 see-through (default), 1 solid."),
        toggle("Aggro Groups", vr_mg3_aggro_groups)
            .help("A waking monster wakes what its aggro_target names (map3, map7, map8). Upstream ships this off; off by default."),
    };
}

// Dawn of the Machine's weapons (MG3.md M3-11..14): "mg3wtest:" lines with developer 1 (QC/vr_mg3_weapons_test.qc).
// The Super Axe works in any campaign when the Dawn of the Machine data is there (its models are read from it in place).
za::Vector<Item> pageMg3WeaponTests()
{
    return {
        header("Dawn of the Machine Weapons"),
        command("A Super Axe in Your Hand", "impulse 168")
            .help("The Super Axe in the main hand (impulse 188: the off hand). Strike a monster twice within Super Axe Burst "
                  "Window (Combat > Weapon Damage) for the lightning burst (15 cells; the head glows while it is ready)."),
        command("A Super Axe Pickup Ahead", "vr_physics_spawn weapon_superaxe reach")
            .help("A weapon_superaxe lying 64 units ahead, as a map places one."),
        command("Weapons Report", "vr_mg3_wtest 1")
            .help("The Dawn of the Machine data, your hands, holsters and cells, the Super Axe's burst chain (developer 1)."),
        command("Super Axe Blows and Burst", "vr_mg3_wtest 2")
            .help("Destructive: two ogres and a zombie ahead, struck by the Super Axe: first blow, burst, window, another "
                  "monster, water, cells, zombie and killing blows checked over a few seconds."),
        command("Super Axe Pickup", "vr_mg3_wtest 3")
            .help("Destructive: a pickup ahead taken by the empty main hand: its target fired, its silent drop (16 units)."),
        command("Map2's Super Axe", "vr_mg3_wtest 4")
            .help("Destructive, Dawn of the Machine's map2: its weapon_mjolnir is a Super Axe; take it: the secret counted, "
                  "dropped 2047 units below, as Dawn of the Machine does."),
        command("Super Axe in Hand and Holster", "vr_mg3_wtest 5")
            .help("Destructive: a Super Axe in the main hand and the first holster (then change level or save and load, "
                  "and Weapons Report)."),
        command("Axe Buttons Check", "vr_mg3_wtest 6")
            .help("Destructive, a Dawn of the Machine map with axe buttons (map6, map7, map8, secret5): each stays shut to "
                  "a shot and a blast, then opens to a blow (fist, Super Axe, thrown weapon, thrown prop, headbutt)."),
        command("Stand Before an Axe Button", "vr_mg3_wtest 7")
            .help("You stand facing the nearest closed axe button: shoot it (it says to use the axe), then strike it."),
        command("Did the Axe Button Open", "vr_mg3_wtest 8").help("The nearest axe button's state (developer 1)."),
        command("Laser Cannon Bolts", "vr_mg3_wtest 9")
            .help("A bolt's damage (Dawn of the Machine's 15, lit 20; elsewhere 18, 25), then 12 bolts at the floor ahead: "
                  "their bounces (0.9 of the damage kept) and stops."),
        command("Take the Nearest Laser Cannon", "vr_mg3_wtest 10")
            .help("Destructive: the nearest weapon_laser_gun into the empty main hand (map2b)."),
        command("Bloody Bits On", "vr_mg3_wtest 11")
            .help("Both bloody shotguns' bits set for this game: every shotgun refires in 0.28 s, every super shotgun "
                  "fires 28 pellets."),
        command("Bloody Bits Off", "vr_mg3_wtest 15"),
        command("Bloody Report", "vr_mg3_wtest 12").help("The bloody bits, the shotgun's refire, your shells (developer 1)."),
        command("Take the Bloody Shotguns", "vr_mg3_wtest 13")
            .help("Destructive: the map's bloody shotguns (none: two spawned ahead) taken by the empty main hand."),
        command("Bloody Nightmare New Game Flag", "vr_mg3_wtest 14")
            .help("Destructive: serverflags 256 set (then change level: the map's bloody shotguns stay)."),
    };
}

// Dawn of the Machine's monsters (MG3.md M3-15..18): "mg3mtest:" lines with developer 1 (QC/vr_mg3_monsters_test.qc).
// The infected are stock monsters (any campaign); MG3's own monsters need its data (their models read from it in place).
za::Vector<Item> pageMg3MonsterTests()
{
    return {
        header("Dawn of the Machine Monsters"),
        command("An Infected Grunt Ahead", "vr_test_spawn 30; vr_test_spawn_dist 128; impulse 241")
            .help("An infected grunt 128 units ahead: killed, he bursts and gets up as a zombie (counted once, as it)."),
        command("An Infected Knight Ahead", "vr_test_spawn 31; vr_test_spawn_dist 128; impulse 241"),
        command("An Infected Enforcer Ahead", "vr_test_spawn 32; vr_test_spawn_dist 128; impulse 241")
            .help("An infected enforcer: killed, he bursts and gets up as a fiend."),
        command("An Infected Death Knight Ahead", "vr_test_spawn 33; vr_test_spawn_dist 128; impulse 241"),
        command("A Death Knight Lying as a Corpse", "vr_test_spawn_flags 65536; vr_test_spawn 33; vr_test_spawn_dist 128; impulse 241; vr_test_spawn_flags 0")
            .help("An infected death knight lying as Dawn of the Machine's corpses lie (not solid) until woken: shoot him, "
                  "or wake him (he rises, his death backwards)."),
        command("Monsters Report", "vr_mg3_mtest 1")
            .help("The map's Dawn of the Machine monsters by kind, the infected turned, the kills (developer 1)."),
        command("Infected Check", "vr_mg3_mtest 2")
            .help("Destructive: an infected grunt, knight, enforcer and death knight ahead, each killed (bursts into a "
                  "zombie or a fiend, not counted), then killed again (counted once each)."),
        command("Lying Death Knight Check", "vr_mg3_mtest 3")
            .help("Destructive: a death knight lying as a corpse (not solid, his last death frame), woken: he rises."),
        command("A Rocket Ogre Ahead", "vr_test_spawn 34; vr_test_spawn_dist 192; impulse 241")
            .help("Dawn of the Machine's rocket ogre (its data read in place): volleys of two rockets; bat them back."),
        command("A Rocket Ogre's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 34; vr_test_spawn_dead 1; vr_test_spawn_dist 128; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0"),
        command("A Demo Dog Ahead", "vr_test_spawn 35; vr_test_spawn_dist 192; impulse 241")
            .help("Dawn of the Machine's demo dog: its leap onto you kills it; however it dies, three grenades spill."),
        command("Demo Dog Check", "vr_mg3_mtest 6")
            .help("Destructive: demo dogs ahead: one shot dead beside a grunt (grenades, the grunt hurt), one landing on "
                  "you, one beheaded (it lies headless, the grenades spill all the same)."),
        command("A Ranged Knight Ahead", "vr_test_spawn 36; vr_test_spawn_dist 256; impulse 241")
            .help("Dawn of the Machine's ranged knight: fans of diamonds at range; bat or parry them."),
        command("A Ranged Knight's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 36; vr_test_spawn_dead 1; vr_test_spawn_dist 128; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0"),
        command("Ranged Knight Check", "vr_mg3_mtest 7")
            .help("Destructive: a ranged knight ahead: its model, class, head zone; made to cast (six diamonds, you hurt), "
                  "killed (its death frames), another gibbed (the death knight's head)."),
        command("Rocket Ogre Check", "vr_mg3_mtest 5")
            .help("Destructive: a rocket ogre ahead: its model, class, head zone; made to shoot (two rockets, you hurt), "
                  "killed (its corpse, no chainsaw dropped)."),
    };
}

// Dawn of the Machine's monsters, M3-19..23 (MG3.md): "mg3btest:" lines with developer 1 (QC/vr_mg3_bestiary_test.qc).
// They spawn in any campaign when the Dawn of the Machine data is there (read from it in place).
za::Vector<Item> pageMg3BestiaryTests()
{
    return {
        header("Dawn of the Machine Bestiary"),
        command("An Orb Ahead", "vr_test_spawn 40; vr_test_spawn_dist 160; impulse 241")
            .help("Dawn of the Machine's orb 160 units ahead: a flying eye that sees behind it too, bursts spheres at you, and "
                  "blows up where it lands when killed."),
        command("A Slime Ahead", "vr_test_spawn 41; vr_test_spawn_dist 160; impulse 241")
            .help("Dawn of the Machine's slime: a spawn that, blowing up, throws blobs that become spawns, twice over."),
        command("A Ghost Ahead", "vr_test_spawn 42; vr_test_spawn_dist 128; impulse 241")
            .help("Dawn of the Machine's ghost: it drifts about; touch it (a hand will do) and it fades away."),
        command("A Sacrifice Ahead", "vr_test_spawn 43; vr_test_spawn_dist 96; impulse 241")
            .help("A hanging sacrifice (Dawn of the Machine's misc_sacrifice): struck down, it is gibbed."),
        command("A Lava Man Ahead", "vr_test_spawn 44; vr_test_spawn_dist 200; impulse 241")
            .help("Dawn of the Machine's lava man (MG3's model): it rises, stands as it throws lava balls, takes 0.8 of anything "
                  "but the lightning gun and the laser cannon."),
        command("A Super Shambler Ahead", "vr_test_spawn 45; vr_test_spawn_dist 200; impulse 241")
            .help("Dawn of the Machine's blood shambler: 2000 health, plasma sprays on its blows, lightning near and far."),
        command("A Super Shambler's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 45; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on and a super shambler killed at the Distance ahead (Debug > Tests > Ahead of You): he goes limp as he "
                  "falls (his own rig on MG3's model)."),
        command("Bestiary Report", "vr_mg3_btest 1")
            .help("The Dawn of the Machine data each monster needs, and how many of each this map has (developer 1)."),
        command("Orb Test", "vr_mg3_btest 2")
            .help("Destructive: an orb ahead, woken (god mode meanwhile): its eyes, its spheres, its pain, its death and blast "
                  "(developer 1)."),
        command("Ghost, Sacrifice and Slime Test", "vr_mg3_btest 3")
            .help("Destructive: a ghost laid to rest by your touch, a sacrifice struck down (its target fired), a slime's two "
                  "generations of spawns counted as they start and die (developer 1)."),
        command("Lava Man Test", "vr_mg3_btest 5")
            .help("Destructive: a lava man of Dawn of the Machine's ahead (god mode): it rises and flies, its first hit staggers "
                  "it, its damage rule, its throws, its death by a use; a Rogue lava man beside it unchanged (developer 1)."),
        command("Super Shambler Test", "vr_mg3_btest 7")
            .help("Destructive: a super shambler ahead (god mode): its plasma sprays, its lightning, half damage from blasts, its "
                  "parries and head zone; killed: his ragdoll (developer 1)."),
        command("Quake's Monsters in Dawn of the Machine", "vr_mg3_btest 10")
            .help("Destructive: backpacks, skill 3's harder skins and pain rest, the shambler's lightning, the vore's balls, Bloody "
                  "Nightmare's extra lasers, spikes and grenades, hanging and close-throwing zombies; it expects them only in "
                  "Dawn of the Machine (and Bloody Nightmare: the next row) at skill 3 (developer 1)."),
        command("Bloody Nightmare On", "vr_mg3_btest 11")
            .help("Destructive: serverflags 64 (Dawn of the Machine's Bloody Nightmare) for the test above."),
        command("The Map's Sacrifices", "vr_mg3_btest 4")
            .help("Destructive (Dawn of the Machine's map8): every sacrifice used in turn; the counter's count down to its "
                  "target fired (developer 1)."),
    };
}

za::Vector<Item> pageMg3ShubTests()
{
    return {
        header("Dawn of the Machine: Shub"),
        command("Shub Ahead (Free)", "vr_test_spawn 60; vr_test_spawn_dist 256; impulse 241")
            .help("Dawn of the Machine's Shub-Niggurath 256 units ahead (needs room: she is 256 units wide): volleys of diamonds, "
                  "autoguns, lobbed plasma, a sweeping beam, eyes and a seeker by her phase; she raises shub zombies where the "
                  "map has their spawns. One from here is free: killed, she bursts but ends nothing."),
        command("A Shub Zombie Ahead", "vr_test_spawn 61; vr_test_spawn_dist 96; impulse 241")
            .help("One of Shub's zombies: Quake's zombie lying there, up 7 s later and after you; it throws its flesh only up close."),
        command("Shub's Eye Ahead", "vr_test_spawn 62; vr_test_spawn_dist 160; impulse 241")
            .help("One of Shub's eyes: it hangs there and after 2 s spirals 72 spheres at you, then bursts. 120 health."),
        command("Shub's Seeker Ahead", "vr_test_spawn 63; vr_test_spawn_dist 200; impulse 241")
            .help("Shub's seeker eye: it chases you, faster and faster; its touch does 500. 300 health."),
        command("Shub Report", "vr_mg3_shubtest 1")
            .help("Her data, the map's Shub (phase, health), its shub zombie spawns, pillars, zombies and children (developer 1)."),
        command("Shub Phases Test", "vr_mg3_shubtest 2")
            .help("Destructive (god mode meanwhile): the map's Shub (boss2: you are taken to the arena) or a free one ahead: her "
                  "wounds through her four phases, her thrash, her waves, each phase's children, her autoguns, every child "
                  "dead (developer 1)."),
        command("Shub Zombies and Pillars Test", "vr_mg3_shubtest 3")
            .help("Destructive (boss2; god mode meanwhile): her zombies raised at the map's spawns (33 at most), up 7 s later, "
                  "her spheres and their flesh passing each other; the pillars sinking 20 a hit, with you on one: down with it, "
                  "the floor holding you as it sinks on (developer 1)."),
        command("Shub Death Test", "vr_mg3_shubtest 4")
            .help("Destructive (god mode meanwhile): Shub killed: her children cleared, the lights out, her burst; the map's "
                  "then shows the final text and the credits (developer 1)."),
        command("Training Dummy as Shub Test", "vr_mg3_shubtest 5")
            .help("Destructive: a training dummy 300 units ahead as Shub (free), then as her eye: what it is, killed as each, "
                  "nothing ended, a dummy back; Dummy Enemy reset to the grunt (developer 1)."),
    };
}

// Dawn of the Machine's Chthon, M3-24/25 (MG3.md): "mg3ctest:" lines with developer 1 (QC/vr_mg3_chthon_test.qc).
za::Vector<Item> pageMg3ChthonTests()
{
    return {
        header("Dawn of the Machine: Chthon"),
        command("Chthon Ahead", "vr_test_spawn 50; vr_test_spawn_dist 600; impulse 241")
            .help("Dawn of the Machine's Chthon 600 units ahead (any map with its data), woken at you: fans and volleys of "
                  "spheres, six fits as he is hurt, 0.8 damage but from lightning and lasers. He ends no game here."),
        command("Chthon Report", "vr_mg3_ctest 1")
            .help("This map's Chthon (phase, health, fits), the second arena's points, lava suits, music (developer 1)."),
        command("Chthon Test", "vr_mg3_ctest 2")
            .help("Destructive: a Chthon ahead (god mode): his damage rules, every fit and phase, the spiral, his death, rings "
                  "and gibs (developer 1)."),
        command("The Boss Map's Fight", "vr_mg3_ctest 3")
            .help("Destructive (Dawn of the Machine's boss map, before the fight): you walk into his trigger; each fit and phase "
                  "driven, the waves, both teleports, his death (god mode; developer 1)."),
    };
}

// The Thing (vr_test_spawn) the Tests page was built for: Put It There's lock follows it (AD support: an AD entry
// without its data locks it).
int testsPageSpawn = -1;

za::Vector<Item> pageDebugTests()
{
    testsPageSpawn = static_cast<int>(vr_test_spawn.value);
    return {
        header("Loot Beams"),
        command("Loot Beams: Lay Out Loot", "vr_lootbeam_test 24 18")
            .help("vr_lootbeam_test 24 18: 24 things ahead of you in rows (weapons, nailgun magazines full and nearly "
                  "empty, ammo, health, armour, quad damage) and 18 gibs among them. Graphics > Loot Beams sets which glow."),
        command("Loot Beams: Report", "vr_lootbeams_report")
            .help("vr_lootbeams_report: each beam this frame (entity, category, model, fade) and the things skipped "
                  "(carried, moving, empty magazines), on the console."),
        toggle("Loot Beams: Debug Lines", vr_lootbeam_debug).help("vr_lootbeam_debug: those counts once a second."),
        header("Doom"),
        command("Doom: Info", "vr_doom_info")
            .help("vr_doom_info: Doom's state (level, health, ammo, place), its focus, the screen's distance and angle "
                  "from your eye, its cost a frame, Quake's time."),
        toggle("Doom: Show Touches", vr_debug_touch).help("vr_debug_touch: each touch's start and end printed (2: every frame)."),
        toggle("Doom: Debug", vr_debug_doom).help("vr_debug_doom: Doom's own text, the focus changing and menu taps printed."),
        command("Doom: Restart", "vr_doom_restart 1").help("Doom stopped (its settings saved) and started again, WADs searched again."),
        header("Arcane Dimensions"),
        command("Arcane Dimensions: Play (Developer)", "vr_campaign_native ad")
            .help("AD support in progress (ARCANE_DIMENSIONS.md): a new Arcane Dimensions game at its hub, its own start "
                  "map, with the latest release (1.80 patch 1) in an ad folder. Its maps play by its rules; the console "
                  "says what is missing."),
        adSpawnRow(310, command("Arcane Dimensions: Widowmaker Pickup Ahead", "vr_ad_test 5")) // AD support
            .help("In an Arcane Dimensions map: a Widowmaker pickup (AD's upgrade) appears ahead of you, to take with a "
                  "hand. Arcane Dimensions: Upgrades Report then says whether you have the upgrade."),
        adSpawnRow(309, command("Arcane Dimensions: Upgraded Weapons Ahead", "vr_ad_test 96")) // AD support
            .help("The Shadow Axe, the Widowmaker and the Plasma Gun pickups ahead of you, as the toolgun makes them: "
                  "drawn as their VR models (made from your AD's own view models)."),
        adSpawnRow(309, command("Arcane Dimensions: Shadow Axe in Hand", "vr_ad_test 97")) // AD support
            .help("The Shadow Axe in your main hand, the Widowmaker and the Plasma Gun in the hip holsters."),
        adSpawnRow(310, command("Arcane Dimensions: Widowmaker in Hand", "vr_ad_test 98")) // AD support
            .help("The Widowmaker in your main hand, the Shadow Axe and the Plasma Gun in the hip holsters."),
        adSpawnRow(311, command("Arcane Dimensions: Plasma Gun in Hand", "vr_ad_test 99")) // AD support
            .help("The Plasma Gun in your main hand, the Shadow Axe and the Widowmaker in the hip holsters."),
        adSpawnRow(311, command("Arcane Dimensions: Plasma Gun Cell Test", "vr_ad_test 141")) // AD support
            .help("The Plasma Gun in your off hand, your main hand free: its cell's well on the gun's inner side is "
                  "found, the cell pops out sideways, and a cell from your pouch brought to the well goes in (adplasma: "
                  "lines in the console). Then eject it (B/Y) and reload it by hand."),
        adSpawnRow(310, command("Arcane Dimensions: Widowmaker Tube Test", "vr_ad_test 149")) // AD support
            .help("The Widowmaker in your off hand, empty, your main hand free: handfuls from your pouch brought to its "
                  "shell port (the receiver's inner side) fill its tube, then a held trigger fires it full auto for two "
                  "seconds (adwidow: lines in the console). Then load it and fire it yourself."),
        adSpawnRow(310, command("Arcane Dimensions: Widowmaker Barrels Info", "vr_ad_widowmaker_spin_info")) // AD support
            .help("Each hand's Widowmaker barrels: their turn, speed and the frames drawn, the trigger and the last shot, "
                  "the muzzle (the top barrel's front) in the world."),
        adSpawnRow(310, slider("Arcane Dimensions: Widowmaker Barrels Held At", "vr_ad_widowmaker_spin_at", -1.f, 360.f, 5.f, "%.0f deg")) // AD support
            .help("Debug: the Widowmakers in the hands drawn with their barrels turned this far (for pictures); -1 as "
                  "they are, turning with the trigger."),
        adSpawnRow(309, command("Arcane Dimensions: Shadow Axe Rules Test", "vr_ad_test 140")) // AD support
            .help("Three AD zombies and an AD knight ahead; two seconds on, AD's Shadow Axe rules are checked through your "
                  "axe's own blows: a zombie in pain gibbed for good, the knight's long pain, a downed zombie chopped by a "
                  "swing, a trigger's upgrade_axe key, the blow's 35. The console prints each (adshadowaxe:). Then the "
                  "Shadow Axe stays in your main hand and one zombie is left: knock it down and chop it."),
        adSpawnRow(307, command("Arcane Dimensions: Health Vials Test", "vr_ad_test 142; wait; vr_ad_test 143; vr_physics_spawn item_healthvial 64 -24; vr_physics_spawn item_healthgem 64 24")) // AD support
            .help("A health vial and a gem are force-grabbed into your main hand and put in the shoulder holster (each "
                  "heals 2-5), and AD's own vial's heal is checked (it once left you at 0 health, unable to act); "
                  "adhealth: lines in the console. Then a shotgun in your main hand and two vials ahead: force grab "
                  "them, and shoot and jump."),
        adSpawnRow(300, command("Arcane Dimensions: Artifacts Ahead", "vr_physics_spawn item_artifact_jumpboots 64 -48; vr_physics_spawn item_artifact_blastbelt 64 0; vr_physics_spawn item_artifact_sharp_shooter 64 48; vr_physics_spawn item_artifact_lavashield 112 -24; vr_physics_spawn item_artifact_airtank 112 24")) // AD support
            .help("Any map, with Arcane Dimensions' data installed: its Jump Boots, Blast Belt, Sharp Shooter, Lava Shield "
                  "and Air Tank ahead of you, to walk into (the wrist gadget shows each one held and the soonest one's "
                  "seconds left)."),
        command("Arcane Dimensions: Screen Tint Report", "vr_ad_tint_info")
            .help("vr_ad_tint_info: the screen tint Arcane Dimensions asks for now (its colour, density, a tint or a "
                  "camera's fade), the one drawn (eased), Screen Tints, the whole colour shift's alpha."),
        command("Arcane Dimensions: Status", "vr_ad_info")
            .help("vr_ad_info: AD's data (the latest release, another version refused), whether this map plays by AD's "
                  "rules and why (the campaign, or the signals found), how many of its entities spawned with AD's code."),
        header("Monsters in the Map"),
        command("Whole-Map Monsters: Report", "vr_test_monsters 1")
            .help("Count this map's monsters: live, awake (hunting someone), dead, waiting for their trigger (one "
                  "monsterstest: line)."),
        command("Whole-Map Monsters: Bring In the Waiting", "vr_test_monsters 4")
            .help("Every monster still waiting for its trigger (the official campaigns' deferred monsters) comes in at "
                  "once: the map's full count. Reload afterward."),
        command("Whole-Map Monsters: Wake All", "vr_test_monsters 2")
            .help("Every live monster of the map hunts you at once, as if it had seen you: the full count's AI "
                  "(the benchmarks' mg3_*_awake). Reload afterward."),
        command("Whole-Map Monsters: Kill All", "vr_test_monsters 3")
            .help("Every live monster of the map dies at once (just enough damage: deaths, corpses and as many ragdolls "
                  "as Most Ragdolls allows (vr_ragdoll_max). The benchmarks' mg3_*_kill. Reload afterward."),
        command("Hit Probe: Nearest Monster", "vr_test_monsters 5")
            .help("Level shots (precise hits) at the nearest live monster every 8 units from below its box to 160 above "
                  "it: the heights from its origin that hit it (one hitprobe: line). A tall model's parts out of reach "
                  "show as gaps."),
        header("Spawn and Behaviour Pages"),
        open("Ragdoll Behaviours", pageIndex(pageRagdollTests))
            .help("Each active-ragdoll behaviour set going on the nearest grunt, enforcer, knight, death knight, ogre or "
                  "zombie: deaths by where struck, stumble, wound, crawl, lost limbs, swim, flail, walls, ledges, flinches, "
                  "staggers, throws, get-ups; the console says whether each began."),
        open("Dawn of the Machine Bestiary", pageIndex(pageMg3BestiaryTests))
            .help("Dawn of the Machine's monsters in any campaign with its data: the orb, ..."),
        open("Dawn of the Machine Weapons", pageIndex(pageMg3WeaponTests))
            .help("The Super Axe (any campaign with the Dawn of the Machine data), the axe buttons, the laser cannon, the bloody shotguns."),
        open("Dawn of the Machine Monsters", pageIndex(pageMg3MonsterTests))
            .help("The infected (any campaign) and Dawn of the Machine's own monsters (with its data): spawns and checks."),
        open("Arcane Dimensions Monsters", pageIndex(pageAdMonsterTests)) // AD support
            .help("Arcane Dimensions' monsters with Quake VR's treatment (ragdolls, beheading, limbs, knockdowns): "
                  "spawns ahead and reports, in an AD map."),
        open("Spawn Pickup Weapons", pageIndex(pageSpawnWeapons))
            .help("Spawn a physical pickup ahead of you, ready to grab and use."),
        header("Physics Stress"),
        slider("Pile Size", vr_test_pile_count, 50.f, 1000.f, 50.f, "%.0f props")
            .extend(10.f, 2000.f)
            .help("How many props Pile of Rocks, Pile of Bricks and Mixed Pile put ahead of you. 500: about 350 bodies "
                  "awake as they fall (Physics on Threads at work: about twice as fast), one frame of ~9 ms as they "
                  "appear; 1000: a 20+ ms frame as they appear, then 1-2 ms steps on threads."),
        slider("Crates in the Wall", vr_test_pile_crates, 8.f, 120.f, 8.f, "%.0f crates")
            .extend(1.f, 400.f)
            .help("How many small crates Wall of Crates stacks ahead of you (walls 8 wide and 5 high, one behind another; "
                  "80: two walls). Standing walls cost little (under 0.1 ms a step); knocked over, a wall wakes its 40."),
        command("Pile of Rocks", "vr_physics_bigpile rocks")
            .help("vr_physics_bigpile rocks: Pile Size rocks 96 units ahead of you, in leaning columns that topple into "
                  "one pile (Physics on Threads at work while they fall). Clear the Piles takes them away."),
        command("Pile of Bricks", "vr_physics_bigpile bricks").help("vr_physics_bigpile bricks: the same, of bricks."),
        command("Wall of Crates", "vr_physics_bigpile crates")
            .help("vr_physics_bigpile crates: Crates in the Wall small crates stacked into walls facing you: push, "
                  "shoot or blow them up."),
        command("Mixed Pile", "vr_physics_bigpile mixed")
            .help("vr_physics_bigpile mixed: Pile Size props, rocks and bricks, with every fourth column small crates."),
        command("Clear the Piles", "vr_physics_clearpiles")
            .help("vr_physics_clearpiles: every prop these put there taken away, and every broken crate's pieces."),
        command("Physics Step Time", "vr_physics_steptime")
            .help("vr_physics_steptime: Box3D's step time a frame since the last time (average, median, 95th and 99th "
                  "percentiles, worst, ms), the awake bodies and its threads. Run it, make a pile, play, run it again "
                  "(Profiling and Memory: Physics Threads, Physics Step Time by Awake Bodies)."),
        command("Explosion Debris Ahead", "vr_explosion_debris_test")
            .help("vr_explosion_debris_test: an explosion's look and its chunks (the server's Box3D props) 64 units ahead, "
                  "no blast. Then Explosion Debris List."),
        command("Explosion Debris List", "vr_explosion_debris_list")
            .help("vr_explosion_debris_list: each chunk: its entity, place, speed, resting (asleep) or moving and on what "
                  "(the world, a lift), age, time left, fade. vr_explosion_debris_stats the counts; "
                  "vr_explosion_debris_launch <x y z> <vx vy vz> [size] [life] one chunk where you say."),
        header("Spatial Audio"),
        command("Spatial Audio Info", "vr_snd_info")
            .help("vr_snd_info: Steam Audio loaded or not, the voices, the scene (triangles, doors and lifts), the "
                  "simulations' times, where the listener is."),
        command("32 Sounds Around You", "vr_snd_bench_spawn 32")
            .help("vr_snd_bench_spawn: 32 looping sounds in a ring round you (till the map changes): a load for the "
                  "profiler's sound line, with Spatial Audio on and off."),
        command("Five Explosions at Once", "vr_snd_burst weapons/r_exp3.wav 5")
            .help("vr_snd_burst: five rocket explosions at once 2.5 m ahead of you (as explosive boxes blowing up together): "
                  "with the limiter on (Sound, Advanced: Mix Limiter) a loud bang, off a crackle; Record the Mix and "
                  "Spatial Audio Info tell how far over full scale it went."),
        command("A Sound 45 Degrees Right", "vr_snd_play_dir misc/r_tele1.wav 45")
            .help("vr_snd_play_dir <sample> <azimuth> [elevation] [metres]: a sound from that direction of your head, 2 m "
                  "away (here the teleport's, 45 degrees right). With Record the Mix: its left/right balance (ROUND21.md, "
                  "HRTF balance)."),
        command("A Sound 45 Degrees Left", "vr_snd_play_dir misc/r_tele1.wav -45")
            .help("vr_snd_play_dir: the same sound 45 degrees left: it should sound as far to the left as the other did to "
                  "the right."),
        command("A 22 kHz Sound Behind You", "vr_snd_play_dir vr/torch_out.wav 180")
            .help("vr_snd_play_dir: the torch going out (a 22 kHz sound, with highs) behind your head. With Sound > "
                  "Full-Band Sound on it should sound behind, and duller than ahead; off (Quake's 11 kHz low-pass) it "
                  "loses the highs that tell (ROUND21.md, \"Full-band sound\")."),
        command("A 22 kHz Sound Ahead", "vr_snd_play_dir vr/torch_out.wav 0")
            .help("vr_snd_play_dir: the same sound straight ahead, to compare."),
        header("Ahead of You"),
        adSpawnChoices(cycle("Thing", vr_test_spawn, // AD support
            {{0.f, "Grunt"}, {1.f, "Ogre"}, {2.f, "Zombie"}, {3.f, "Shambler"}, {4.f, "Scrag"}, {5.f, "Knight"},
             {6.f, "Hell Knight"}, {7.f, "Dog"}, {8.f, "Enforcer"}, {9.f, "Fiend"}, {10.f, "Vore"}, {11.f, "Spawn"},
             {12.f, "Gremlin"}, {13.f, "Centroid"}, {14.f, "Mummy"}, {15.f, "Phantom Swordsman"}, {16.f, "Wrath"},
             {17.f, "Overlord"}, {18.f, "Guardian"}, {19.f, "Dragon"}, {20.f, "Marksman Ogre"},
             {30.f, "Infected Grunt"}, {31.f, "Infected Knight"}, {32.f, "Infected Enforcer"}, {33.f, "Infected Death Knight"},
             {34.f, "Rocket Ogre"}, {35.f, "Demo Dog"}, {36.f, "Ranged Knight"}, {40.f, "Orb"}, {41.f, "Slime"}, {42.f, "Ghost"}, {43.f, "Sacrifice"}, {44.f, "Lava Man (Dawn of the Machine)"}, {45.f, "Super Shambler"},
             {60.f, "Shub-Niggurath (Free)"}, {61.f, "Shub Zombie"}, {62.f, "Shub's Eye"}, {63.f, "Shub's Seeker"},
             {50.f, "Chthon (Dawn of the Machine)"},
             {100.f, "Health Box"}, {101.f, "Shells Box"}, {102.f, "Explosive Box"},
             {103.f, "Small Explosive Box"}, {104.f, "Explosive Box (Never Blows Up)"}, {105.f, "Ogre's Head"},
             {106.f, "Gib"}, {107.f, "Small Crate"}, {108.f, "Large Crate"}, {109.f, "Two Crates Stacked"},
             {110.f, "Rocks and Bricks"}, {111.f, "Barrel"}, {112.f, "Barrel Lying"}, {113.f, "Silver Key"}})) // AD support
            .help("What Put It There puts ahead of you, facing you (or as Facing says). The mission packs' monsters need their game installed; Dawn "
                  "of the Machine's (its infected, which burst into zombies and fiends; its own monsters, the orb, the sacrifice: MG3's data, read in place)."),
        slider("Distance", vr_test_spawn_dist, 32.f, 256.f, 8.f, "%.0f units").extend().help("How far ahead."),
        cycle("Facing", vr_test_spawn_facing, {{0.f, "As Each Spawner"}, {1.f, "Towards You"}, {2.f, "Away"}, {3.f, "Its Own Angle"}, {4.f, "Its Left Side to You"}, {5.f, "Its Right Side to You"}})
            .help("Which way monsters put ahead of you face: Put It There, impulse 244 (a grunt ahead) and vr_physics_spawn "
                  "(Spawn a Crowd). As Each Spawner: the first two face you, vr_physics_spawn keeps the map's angle. Away: "
                  "they look where you look (trailer shots from behind them)."),
        toggle("Into the Main Hand", vr_test_spawn_hold)
            .help("A box or a crate (Health Box .. Explosive Box, the crates) put into your empty main hand, as if gripped: "
                  "to test held props (the blood on what you hold: vr_gore_spatter_test, vr_gore_hands_info)."),
        cycle("As a Corpse", vr_test_spawn_dead, {{0.f, "Off"}, {1.f, "Corpse"}, {2.f, "Gibbed"}, {3.f, "Ragdoll"}})
            .help("A monster killed at once: a corpse, to test gibbing and carrying; Gibbed: killed hard enough to gib (its "
                  "gibs and head to pick up); Ragdoll: a corpse with ragdolls on (vr_ragdoll 1: a grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler or gremlin goes limp as he "
                  "falls)."),
        slider("Box Turned", vr_test_spawn_yaw, 0.f, 90.f, 1.f, "%.0f degrees")
            .extend()
            .help("A box (Health Box .. Explosive Box): let loose and turned this far about its upright, to test the "
                  "grappling hook against its real shape (its turned box's empty corners let the hook by)."),
        slider("Box Tilted", vr_test_spawn_tilt, 0.f, 90.f, 1.f, "%.0f degrees")
            .extend()
            .help("A box: tipped this far about the way you face, on its lowest corner (it topples: sv_gravity 0 keeps it so)."),
        adSpawnRow(static_cast<int>(vr_test_spawn.value), command("Put It There", "impulse 241").help("Puts the Thing ahead of you.")) , // AD support
        command("Enemy Weapon Drop Cap", "developer 1; vr_dropcap_test 1")
            .help("vr_dropcap_test 1: 60 grunts spawned ahead and killed one by one (vr_dropcap_test_n), the first one's "
                  "rifle taken into an empty hand, a chainsaw thrown up and three other weapons dropped: the burst rifles "
                  "and chainsaws lying about never pass Most Lying About (vr_enemy_weapon_drop_max), the held one, the "
                  "flying one and the others stay (dctest: lines, PASS or FAIL). vr_dropcap_test 2: the count lying about."),
        command("A Knight's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 5; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a knight killed at the Distance ahead: he goes limp as "
                  "he falls (his sword dropped)."),
        command("An Ogre's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 1; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and an ogre killed at the Distance ahead: he goes limp as he falls (his chainsaw dropped)."),
        command("An Enforcer's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 8; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and an enforcer killed at the Distance ahead: he goes limp as he falls (his laser rifle dropped)."),
        command("A Death Knight's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 6; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a death knight killed at the Distance ahead: he goes limp as he falls (his sword dropped)."),
        command("A Rottweiler's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 7; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a rottweiler killed at the Distance ahead: it goes limp as it falls."),
        command("A Scrag's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 4; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a scrag killed at the Distance ahead: he falls limp."),
        command("A Fiend's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 9; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a fiend killed at the Distance ahead: it goes limp as it falls."),
        command("A Shambler's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 3; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a shambler killed at the Distance ahead: he goes limp as he falls."),
        command("A Gremlin's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 12; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a gremlin killed at the Distance ahead (Scourge of Armagon): it goes limp as it falls (a stolen gun dropped)."),
        command("A Vore's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 10; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a vore killed at the Distance ahead: it goes limp as it falls."),
        command("A Centroid's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 13; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a centroid killed at the Distance ahead (Scourge of Armagon): it goes limp as it falls."),
        command("A Grunt's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 0; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a grunt killed at the Distance ahead: he goes limp as "
                  "he falls."),
        command("Go to a Crowbar on a Crate", "vr_crates_goto crowbar")
            .help("vr_crates_goto crowbar: you in front of the next crate with a crowbar lying on it (Crates: Crowbar on Crates)."),
        command("Go to the Next Crate", "vr_crates_goto")
            .help("vr_crates_goto [n]: you in front of the next of the crates placed in this map (or crate n), to look at it."),
        command("Crate Cover", "impulse 223")
            .help("impulse 223: a large crate just ahead to crouch behind, a small one at your right to hold up, and a "
                  "grunt beyond, facing you and not yet awake. Crouched behind the crate it shouldn't see you (it stays "
                  "asleep); standing up, or holding the small crate up, it should (Crates Hide You)."),
        command("Can the Grunt See You?", "impulse 224")
            .help("impulse 224: whether Crate Cover's grunt sees you now, how high your head is, and whether it woke."),
        command("Crouch Shots", "developer 1; vr_crouch_test 12")
            .help("vr_crouch_test 12: a grunt put ahead of you fires 12 bullets at you as it aims them, then goes: whether "
                  "you are crouched (Player Hitbox > Crouching), your eyes' height, the aim's height over your feet and "
                  "the damage you took (crouchtest: line; God mode off). Try it crouched behind the 32-high cover in the "
                  "teleporter test map's crouching room: no damage."),
        command("Crouch Status", "vr_crouch_status")
            .help("Prints your eyes' height over your feet, your crouched box's height (0 standing) and whether you could "
                  "stand."),
        header("Melee Sweep"),
        toggle("Swept Blades", "vr_melee_swept")
            .help("vr_melee_swept: a weapon's blow is the surface its whole blade sweeps each frame (a hammer's or mace's "
                  "head: its volume), landing where it crossed the thing most squarely; off: its points' lines alone, the "
                  "outermost striking, as before."),
        command("Melee Sweep Test", "vr_melee_sweep_test 1")
            .help("vr_melee_sweep_test 1: scripted sweeps of the weapon in your main hand past a box ahead of you and onto "
                  "the floor, with Swept Blades off and on: whether each strikes and where (meleesweep: lines, PASS or "
                  "FAIL). Give the weapon first (an AD hammer: vr_ad_test 128 with AD's data)."),
        header("Parry"),
        command("Check the Parry Pose", "impulse 249")
            .help("Developer 1: whether each held weapon blocks a blow from ahead, and its angle and position."),
        command("Dragon Parry Sequence", "vr_physics_spawn VR_Parry_DragonTest 120")
            .help("Developer 1: a real dragon tail hit and two same-frame follow-ups, then its route recovery. Needs Dissolution of Eternity; grants 500 health. Hold a guard to test the parry: a melee weapon level across in front (a crowbar or sword held up like a gun is no guard), its grip held."),
        command("A Melee Blow Now", "impulse 242")
            .help("The nearest melee monster within 150 units strikes for 10 damage through the actual parry test. With notarget, it attacks only when asked. Parry Stops Attacks also cancels blows asked for during its stagger."),
        header("Ragdoll Triggers"),
        cycle("Show Ragdoll Triggers", "vr_kdtrig_debug", {{0.f, "Off"}, {1.f, "Log"}, {2.f, "Also Lost Rolls"}})
            .help("vr_kdtrig_debug: a `kdtrig:` line for each hit that knocks a humanoid down by stairs, a ledge, a slope, "
                  "a close shotgun blast or its wounds (which, its chance and roll), each thrown body and shoulder "
                  "charge; Also Lost Rolls: each that held but missed, and, each half second while you run fast, why "
                  "your pose isn't a shoulder charge (Combat > Ragdoll Triggers)."),
        command("Shotgun the Nearest", "vr_kdtrig_test 1")
            .help("vr_kdtrig_test 1: a shotgun blast (6 pellets) on the live monster nearest you, from where you stand "
                  "(1 damage dealt; its strength a real blast's)."),
        command("Super Shotgun the Nearest", "vr_kdtrig_test 2").help("vr_kdtrig_test 2: a super shotgun blast (14 pellets)."),
        command("Punch the Nearest", "vr_kdtrig_test 5").help("vr_kdtrig_test 5: a 30-damage blunt blow (a fist's)."),
        command("Chop the Nearest", "vr_kdtrig_test 6").help("vr_kdtrig_test 6: a 30-damage blade's blow (an axe's)."),
        command("Blast Before the Nearest", "vr_kdtrig_test 7")
            .help("vr_kdtrig_test 7: a 60-damage blast 48 units before it (1 dealt)."),
        command("Knock Down the Nearest", "vr_kdtrig_test 10")
            .help("vr_kdtrig_test 10: the live monster nearest you knocked down at once (a body to throw)."),
        command("Fling the Nearest Body", "vr_kdtrig_test 12")
            .help("vr_kdtrig_test 12: the ragdoll nearest you (a corpse or a knocked-down one) thrown at the live monster "
                  "nearest it at Test Throw Speed (Thrown Bodies: topples, staggers or nothing by their weights)."),
        slider("Test Throw Speed", vr_kdtrig_test_speed, 50.f, 800.f, 10.f, "%.0f u/s")
            .help("vr_kdtrig_test_speed: Fling the Nearest Body's speed."),
        command("Throw Trials", "vr_kdtrig_test 44")
            .help("vr_kdtrig_test 44: vr_kdtrig_test_n trials of a fresh grunt corpse thrown at a fresh test monster "
                  "(vr_kdtrig_test_class) at vr_kdtrig_test_x _y _z (0 0 0: 64 units ahead of you) at Test Throw Speed: "
                  "each `kdtrig trial` line (down, staggers or stands, how far it was pushed), then the counts and the "
                  "mean push."),
        command("Spawn a Test Monster Ahead", "vr_kdtrig_test 21")
            .help("vr_kdtrig_test 21: a test monster (vr_kdtrig_test_class: 0 a grunt, 4 an ogre, 8 a shambler) 160 "
                  "units ahead of you, facing you (run into it side-on: Shoulder Charges)."),
        header("Holding Enemies"),
        cycle("Show Struggle", "vr_struggle_debug", {{0.f, "Off"}, {1.f, "Panel"}, {2.f, "Panel and Log"}})
            .help("vr_struggle_debug: a panel over each knocked-down enemy struggling in your hands (its behaviours' "
                  "weights, effort, fatigue, what you hold, the hold's lag); Panel and Log also prints `struggle:` lines "
                  "(Combat > Holding Enemies > Struggle)."),
        command("Struggle Status", "vr_struggle_status")
            .help("vr_struggle_status: each struggling enemy's weights and measures, the struggles' cost since the last "
                  "call (ms a frame), and the ragdolls' positions' hash."),
        toggle("Every Downed One Struggles", vr_struggle_force)
            .help("vr_struggle_force: every knocked-down humanoid struggles as if held by its chest where it lies (to see "
                  "the moves, and their cost with many)."),
        command("Nearest Body Contact", "vr_body_contact_status")
            .help("vr_body_contact_status: the live monster nearest you, how far (origins, level), how deep your two "
                  "bodies overlap (> 0) by Body Inset, whether a hand holds it (Step In)."),
        command("Closest Melee Approach", "vr_enemy_melee_space_status")
            .help("vr_enemy_melee_space_status: per kind of monster after you, the closest its box and its middle came to "
                  "your head (level; below 0 your head was inside it), the same while it attacked in melee, and how far "
                  "Melee Space pushed it back."),
        command("Clear Melee Approach Log", "vr_enemy_melee_space_status reset")
            .help("Starts the closest-approach log above again."),
        toggle("Log Melee Arc", vr_enemy_melee_arc_debug)
            .help("Prints each monster blow at you: its angle off the monster's facing, its arc and reach (Combat > "
                  "Enemy Melee) and whether it landed."),
        toggle("Show Melee Arcs", vr_enemy_melee_arc_show)
            .help("Each melee monster after you draws its arc and reach on the floor: red with you inside, green outside "
                  "(Combat > Enemy Melee > Show Melee Arcs)."),
        command("Melee Arc: Nearest Faces You", "vr_test_meleearc 1")
            .help("Sets the nearest monster 56 units away facing you, after you; 4 seconds later prints its hits on "
                  "you (your health is held at 1000 meanwhile). From 100 units (a knight's run swing): vr_test_meleearc 1001."),
        command("Melee Arc: Nearest Faces Away", "vr_test_meleearc 2")
            .help("As above, its back to you: with Melee Arc it turns to you before its first blow."),
        command("Melee Arc: You Step Aside", "vr_test_meleearc 3")
            .help("As Faces You, and as its attack starts you are moved to its side (100 degrees off its facing): "
                  "with Melee Arc its swing misses."),
        toggle("Log Aim Cone", vr_enemy_aim_debug)
            .help("Prints each monster shot at you: its angle off the monster's facing, its aim and fire cones (Combat > "
                  "Enemy Melee > Ranged Aim), how long it waited and how far it was bent."),
        toggle("Show Aim Cones", vr_enemy_aim_show)
            .help("Each ranged monster after you draws its aim cone (red with you inside) and fire cone (orange) ahead of "
                  "it (Combat > Enemy Melee > Show Aim Cones)."),
        command("Aim Cone: You 60 Deg Off", "vr_test_aimcone 300060")
            .help("Sets the nearest monster 300 units away after you and starts its ranged attack; until it ends you are "
                  "held 60 degrees off its facing (circling it faster than it turns). Prints each shot (bent, waited) "
                  "and its hits on you. Other angles and distances: vr_test_aimcone <distance * 1000 + degrees>."),
        command("Who Is Held?", "vr_foegrab_status")
            .help("vr_foegrab_status: each hand holding an enemy, its hold, how firmly the enemy is held in all, how far "
                  "the hand is from the spot it holds (Combat > Holding Enemies); an enemy held on as a ragdoll after a "
                  "throw or a lift: where its pelvis is from you, how far each held limb is from its hand."),
        command("A Grunt Within Reach", "vr_test_spawn 0; vr_test_spawn_dist 36; impulse 241")
            .help("A grunt 36 units ahead, facing you: close enough to hold with both hands and lift (Combat > Holding "
                  "Enemies > Lift: pull up) or judo-throw (turn it over to a side)."),
        command("Walk the Nearest Away", "vr_foegrab_walk_test 110 1")
            .help("vr_foegrab_walk_test 110 1: the live monster nearest you walks straight away from you at 110 units/s "
                  "for a second; how far it got is printed. Held, it should hardly move (A Grunt Ahead first)."),
        toggle("Log Leg Sweeps", vr_legsweep_debug)
            .help("vr_legsweep_debug: each time what a hand holds meets a humanoid's lower legs the console prints its "
                  "measure (length, slenderness, the part that met them, the sweep's speed) and whether it swept them or "
                  "why not; a sweep's fall is traced for a second (its feet along the sweep, which way it fell)."),
        command("Leg Sweep Table", "vr_legsweep_table")
            .help("vr_legsweep_table: every weapon's and some props' length, cross-section, slenderness and blade, and "
                  "whether it sweeps legs (Combat > Knockdowns > Leg Sweep)."),
        cycle("Log Holds", "vr_foegrab_debug", {{0.f, "Off"}, {1.f, "Taken and Let Go"}, {2.f, "Every Frame"}})
            .help("vr_foegrab_debug: the console logs each hold taken and let go and why (and a grip that found none), "
                  "each two-hand throw and lift tried and how it went (why a lift wasn't: not a humanoid, too far), a "
                  "ragdoll held on and let go of or broken free, and each two-hand hold's hardest turn; Every Frame: each "
                  "held enemy's movement, each hand's stretch, both hands' turn."),
        toggle("Neck Snap Diagnostic", "vr_foegrab_necksnap_debug")
            .help("vr_foegrab_necksnap_debug: over the humanoid you hold (else the nearest), its head zone, where each "
                  "hand holds, the hand line's twist against Twist Angle (Combat > Holding Enemies > Neck Snap, Show "
                  "Diagnostic)."),
        toggle("Lift Diagnostic", "vr_foegrab_lift_debug")
            .help("vr_foegrab_lift_debug: over the humanoid you hold (else the nearest), your hands' pull up against Lift "
                  "Speed and 45 degrees, your reach, what stops a lift now and the last try (Combat > Holding Enemies > "
                  "Lift, Show Diagnostic)."),
        toggle("Leg Grab Diagnostic", "vr_foegrab_leggrab_debug")
            .help("vr_foegrab_leggrab_debug: over the humanoid you hold (else the nearest), the part of its legs each hand "
                  "holds, your pull towards you and up against Pull Speed and Leg Lift Speed, what stops a leg pull or "
                  "lift now and the last try (Combat > Holding Enemies > Leg Grabs, Show Diagnostic)."),
        toggle("Judo Throw Diagnostic", "vr_foegrab_throw_debug")
            .help("vr_foegrab_throw_debug: over the enemy you hold (else the nearest), your hands' turn and speed against "
                  "Turn to Throw and Hands' Speed, the turn's way, what stops a throw now and the last try (Combat > "
                  "Holding Enemies > Two-Hand Throw, Show Diagnostic)."),
        command("Hurt the Held to 30%", "vr_foegrab_hurt 0.3")
            .help("vr_foegrab_hurt 0.3: the enemy you hold (else the one nearest you) is left with 30% of its full health: "
                  "a death knight, an ogre or a fiend can then be thrown (Combat > Holding Enemies, Hurt Below)."),
        command("Heal the Held", "vr_foegrab_hurt 1")
            .help("vr_foegrab_hurt 1: the enemy you hold (else the one nearest you) at its full health again."),
        command("Snap the Nearest Neck", "vr_foegrab_necksnap_test")
            .help("vr_foegrab_necksnap_test: the humanoid nearest you has its neck snapped as both hands would (standing, "
                  "knocked down first), no hands holding it: see its head flop (Broken Neck Flops)."),
        command("Throw the Nearest Left", "vr_foegrab_throw_test 0")
            .help("vr_foegrab_throw_test 0: the monster nearest you thrown to your left as both hands' turn would (its kind "
                  "and health decide; no hands needed). The console traces its fall: its torso's lean, its feet, its head "
                  "(Combat > Holding Enemies > Topple)."),
        command("Throw the Nearest Right", "vr_foegrab_throw_test 1")
            .help("vr_foegrab_throw_test 1: the same, to your right."),
        command("Throw the Nearest at You", "vr_foegrab_throw_test 2")
            .help("vr_foegrab_throw_test 2: the same, towards you."),
        header("Enemy Shoves"),
        command("Shove the Nearest Monster", "impulse 219")
            .help("impulse 219: the nearest monster within 200 units shoved as your two-handed shove does (knocked away, "
                  "staggered). Developer 1 logs grunts', enforcers' and vores' shoves and why one can't shove (Combat > "
                  "Enemy Shoves)."),
        command("Knock Down the Nearest", "vr_knockdown_test 0")
            .help("vr_knockdown_test 0: the nearest monster that can be knocked down is, pushed away from you, whatever "
                  "its chance (A Grunt Ahead first: Debug > Tests)."),
        command("Shove the Nearest Down, One Hand", "vr_knockdown_chance 100; vr_knockdown_test 21")
            .help("Sets Knockdowns' Chance to 100 (every shove; set it back after), then vr_knockdown_test 21: the nearest "
                  "monster shoved as your one-handed open-palm shove does, knocked down, carried on and toppled over its "
                  "feet (Knockdowns' Travel, Topple Angle and Time, Feet Lag, Max Spin). Print Rolls on: its fall "
                  "printed every 0.1 s (\"shove trace\": its lean, its whole turn, how far its feet and pelvis went)."),
        command("Shove the Nearest Down, Two Hands", "vr_knockdown_chance 100; vr_knockdown_test 22")
            .help("The same with a two-handed shove (vr_knockdown_test 22): it goes further."),
        command("Shove the Nearest, Standing, One Hand", "vr_knockdown_chance 0; vr_knockdown_debug 1; developer 1; vr_knockdown_test 21")
            .help("Knockdowns' Chance 0 (set it back after) and Print Rolls on, then vr_knockdown_test 21: the nearest "
                  "monster shoved with one hand and staying up; the console prints how far it went (\"shove slide\"), "
                  "what Knockdowns' Travel is a share of."),
        command("Shove the Nearest, Standing, Two Hands", "vr_knockdown_chance 0; vr_knockdown_debug 1; developer 1; vr_knockdown_test 22")
            .help("The same with a two-handed shove (vr_knockdown_test 22)."),
        command("Shove the Nearest, Stagger", "vr_knockdown_debug 1; developer 1; vr_knockdown_test 21")
            .help("Print Rolls on, then vr_knockdown_test 21: the nearest monster shoved with one hand, its chance as set; "
                  "with Active Shoves and Throws (Gibs and Corpses > Ragdoll Settings) it staggers: the console prints its "
                  "balance and whether it recovered or fell (\"stagger:\"; vr_debug_ragdoll 2: each frame's lean and "
                  "steps)."),
        command("Shove the Nearest, Steady Stagger", "vr_knockdown_chance 0.01; vr_knockdown_debug 1; developer 1; vr_knockdown_test 21")
            .help("Knockdowns' Chance 0.01 (set it back after): its balance all but full, so it staggers and recovers, "
                  "back to its animation and its AI (the hand-back's gap printed: \"handback:\")."),
        command("Hit Stagger: Chances of the Nearest", "developer 1; vr_knockdown_test 40")
            .help("Hits Unbalance (Combat > Knockdowns): the nearest monster's chance to lose its balance to a weak tap, "
                  "a strong blow, a punch to the head, a heavy throw and a thrown weapon (\"hit stagger chances:\")."),
        command("Hit Stagger: Strong Blow on the Nearest", "developer 1; vr_knockdown_test 42")
            .help("The nearest monster struck by a strong blow's stagger alone (no damage): 1 a ragdoll's stagger, 2 "
                  "pushed and flinching, 0 kept its balance, -1 still in its Hit Cooldown."),
        command("Impacts: Print Strikes", "developer 1; vr_knockdown_test 56")
            .help("Impacts (Combat > Knockdowns): every strike of a held or thrown body on a wall or the floor printed "
                  "(\"ragimpact:\": its speed, the mass behind it, the damage, the head or limb worn). vr_knockdown_test 57: off."),
        command("Impacts: State of the Nearest", "developer 1; vr_knockdown_test 50")
            .help("The nearest monster's health, its head's and limbs' worn damage, and the strikes dealt so far."),
        command("Impacts: Throw the Nearest Down One at a Wall", "developer 1; vr_test_throw_up_speed 7; vr_knockdown_test 51")
            .help("The nearest knocked-down monster flung at the nearest wall at 7 m/s and a little up, as if you threw "
                  "it (knock one down first: Shove the Nearest Down)."),
        command("Impacts: Drop You onto the Nearest Down One", "developer 1; vr_knockdown_test 52")
            .help("You put 256 units (2 storeys) over the nearest knocked-down monster: landing on it crushes the part under "
                  "your feet (Landing Crush). vr_knockdown_test 53: 96 units."),
        command("Impacts: Jump onto the Nearest Ragdoll's Head", "developer 1; vr_knockdown_test 56; vr_test_drop_height 46; "
                "vr_knockdown_test 60")
            .help("You put over the nearest ragdoll's head (one down or a corpse), your feet 46 units over the floor: a "
                  "jump from the same level (Crush Wear; the third pops it). vr_knockdown_test 61: over its shin; 62: "
                  "the nearest living one killed."),
        command("Impacts: Drop onto the Nearest Ragdoll's Head", "developer 1; vr_knockdown_test 56; vr_test_drop_height "
                "112; vr_knockdown_test 60")
            .help("As above from 112 units (two heights, Crush Pop Speed): its head pops at once."),
        command("Impacts: Drop You onto the Nearest Standing One", "developer 1; vr_knockdown_test 56; vr_knockdown_test 59")
            .help("You put Drop Height (vr_test_drop_height, 256) units over the nearest standing monster's head: landing "
                  "on it crushes its head (Landing Crush, Crush Standing Ones)."),
        command("Manhandle: the Nearest Staggered Now", "developer 1; vr_knockdown_debug 1; vr_knockdown_test 54")
            .help("The nearest standing monster counts as just staggered: grab it with one hand within Grab While "
                  "Staggered (Holding Enemies) and it goes down in your hand. vr_knockdown_test 55: its health to a fifth "
                  "(Grab When Weak)."),
        command("Why Can't I Knock It Down?", "developer 1; vr_knockdown_test 10; vr_ragdoll_list")
            .help("vr_knockdown_test 10: why a shove, a throw or a lift of the nearest living monster makes no ragdoll "
                  "now: each gate (its knockdown chance, canragdoll, can stagger, stamina) and the ragdoll pool as the "
                  "game counts it (each ragdoll, whether it can be freed for room), then the engine's ragdolls "
                  "(vr_ragdoll_list). Use it when shoves and lifts stop working, and send the console."),
        command("List Monster Groups", "vr_mgroup_list")
            .help("vr_mgroup_list: every monster classname's group (Combat > Monster Groups), then this map's monsters' "
                  "(any not in the table marked: it is in Others) to the console."),
        command("Get Them Up Now", "vr_knockdown_test 1")
            .help("vr_knockdown_test 1: every knocked-down monster tries to get up now. Combat > Knockdowns, Print Rolls: "
                  "And Get-Ups' Motion prints how smoothly each is drawn getting up."),
        command("Buzzsaw on the Nearest Body", "developer 1; vr_knockdown_test 70")
            .help("vr_knockdown_test 70: a running buzzsaw (Rogue's trap) at the middle of the nearest ragdoll, a corpse "
                  "or a knocked-down monster: its blade cuts the limbs and head in it off and gibs its torso (the "
                  "console prints each strike). Needs Rogue's buzzsaw model."),
        command("Remove Every Monster", "vr_knockdown_test 20")
            .help("Every monster removed, standing, knocked down or dead: a clean slate between shove tests (with a grunt "
                  "ahead: impulse 244). Shove one off vrclimb's long ledge or into its trench to see Over a Ledge, Always."),
        command("A Blast in 3 Seconds", "impulse 221")
            .help("impulse 221: an explosion of 60 at your feet 3 s from now, a little ahead of you (towards a wall you hang "
                  "from): hanging from a ledge or a rung, a blast of Climbing: Blasts Knock You Off or more makes you let go "
                  "and throws you. Developer 1 (and vr_climb_debug 1) logs it."),
        header("Chainsaw"),
        command("A Chainsaw in Your Hand", "impulse 164").help("A full ogre's chainsaw in the main hand (impulse 184: the off "
                                                                "hand). Take its cord with the other hand and pull."),
        command("Take the Nearest Chainsaw", "impulse 229").help("The chainsaw lying nearest you (an ogre's, dropped) into an "
                                                                 "empty hand, its fuel as it was."),
        command("Start the Engine", "impulse 230")
            .help("The chainsaws in your hands started, as a good pull of the cord does."),
        command("Drop the Chainsaws", "impulse 220")
            .help("impulse 220: the chainsaws in your hands dropped, as letting go does: one running runs on as it lies "
                  "(Runs On When Let Go), smoking and shaking about (Combat > Enemy Weapons > Chainsaw Engine)."),
        command("Nearly Empty Tank", "impulse 227").help("The chainsaws in your hands left with 5% fuel: to see one stall."),
        command("Report the Chainsaws", "impulse 228").help("Prints each chainsaw in your hands: its fuel, engine, chain and cord."),
        command("Chainsaw Fit", "vr_chainsaw_fit")
            .help("vr_chainsaw_fit: with the chainsaw in the main hand, prints where the off hand must move to take its "
                  "cord and each of its hotspots (two, on the front handle), and how many it has."),
        header("Weapon Instances"),
        command("List Your Weapons", "vr_test_weaponinst 0; impulse 120")
            .help("Prints what has which weapon: your hands, your holsters, the weapons lying near you, each with its id "
                  "and magazine (each weapon is one record that goes where it goes: its blood with it), and how many "
                  "records there are."),
        cycle("Holster", vr_test_weaponinst_slot,
            {{0.f, "Right Hip"}, {5.f, "Left Hip"}, {4.f, "Right Shoulder"}, {3.f, "Left Shoulder"}, {9.f, "Right Upper"},
             {8.f, "Left Upper"}})
            .help("The holster the two commands below use."),
        command("Holster the Main Hand's", "vr_test_weaponinst 1; impulse 120").help("Into that holster, as letting go there does."),
        command("Draw into the Main Hand", "vr_test_weaponinst 2; impulse 120").help("From that holster, as gripping there does."),
        command("Drop the Main Hand's", "vr_test_weaponinst 3; impulse 120").help("As letting go does (a throw at the hand's speed)."),
        command("Take the Nearest Weapon", "vr_test_weaponinst 4; impulse 120").help("The weapon lying nearest you into the main hand."),
        command("Hand Off to the Off Hand", "vr_test_weaponinst 5; impulse 120")
            .help("The main hand's weapon carried by the off hand, as letting go of a two-handed weapon does."),
        command("Take It Back", "vr_test_weaponinst 6; impulse 120").help("The main hand takes the carried weapon's handle."),
        command("Switch Hands", "vr_test_weaponinst 7; impulse 120").help("The main hand's weapon into the off hand (8: back)."),
        header("Reloading"),
        command("Shotgun in the Off Hand", "impulse 154; wait; vr_test_weaponinst 7; impulse 120; give s 40")
            .help("A loaded shotgun into the off hand and 40 shells: the main hand is free for the ammo pouch."),
        command("Nailgun in the Off Hand", "impulse 156; wait; vr_test_weaponinst 7; impulse 120; give n 100")
            .help("A loaded nailgun into the off hand and 100 nails (impulse 157: the super nailgun, 161 the thunderbolt)."),
        command("Rocket Launcher in the Off Hand", "impulse 160; wait; vr_test_weaponinst 7; impulse 120; give r 40")
            .help("A loaded rocket launcher into the off hand and 40 rockets (impulse 158: the grenade launcher, 159 the "
                  "proximity launcher): empty it, then load it at the muzzle."),
        command("Eject the Off Hand's Magazine", "vr_reload_test 6; impulse 125").help("As its B/Y does."),
        command("Empty the Off Hand's Gun", "vr_reload_test 5; impulse 125")
            .help("Its magazine back into your ammo (to load it again)."),
        command("Take a Shell (Main Hand)", "vr_reload_test 1; impulse 125")
            .help("As gripping at the ammo pouch does: a shell (or a taped pair) for the off hand's gun."),
        command("Load the Held Shell", "vr_reload_test 2; impulse 125")
            .help("The main hand's shell into the off hand's gun, as at its port."),
        command("Drop the Held Shell", "vr_reload_test 3; impulse 125").help("Let go of, as anywhere but the pouch."),
        command("Put It Back", "vr_reload_test 4; impulse 125").help("As letting go at the pouch: refunded."),
        command("Report", "vr_reload_test 0; impulse 125")
            .help("Prints your shells, the off hand's magazine, what each hand holds, the shells lying about."),
        command("Report the Loose Rounds", "vr_reload_test 14; impulse 125")
            .help("Each loose round's distance from the off hand's load point, how it lies against the way in, and "
                  "whether it passes through the gun; the load point's axis and the way its opening faces."),
        command("Put the Off Hand's Gun Down", "vr_reload_test 20; impulse 125")
            .help("The off hand's gun let go of: it lies as a prop, its rounds with it. Guns lying about load as held "
                  "ones do, by hand and by contact."),
        command("Report the Lying Guns", "vr_reload_test 23; impulse 125")
            .help("Each gun lying about: its magazine, whether its magazine is in, whether it is open, its load point."),
        cycle("Reload Prints", "vr_reload_debug", {{0.f, "Off"}, {1.f, "Events"}, {2.f, "Every Frame"}, {3.f, "And Magazine Grips"}})
            .help("Events: each take, load, refund, loss, magazine out and hold. Every Frame: a held round's distance to the "
                  "port, a held magazine's pull, snap and apart, a hit's speed. And Magazine Grips: each empty hand's "
                  "distance off the other gun's magazine (its box) and the grip it would take."),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Each held gun's load point and radius, a held magazine's top, an attached magazine's box (blue); the "
                  "pouch's magazines as drawn (white), where pouch reloading finds them (cyan) and their reach (green)."),
        command("Pouch Magazines Report", "vr_reload_pouch_mags; toggleconsole")
            .help("vr_reload_pouch_mags: each magazine drawn in the ammo pouch, where pouch reloading finds it and the "
                  "old row's middle (cm off it), and each gun's load point against the nearest one."),
        toggle("Show the Pouches' Reach", "vr_show_grenade_pouch").help("Spheres where the grenade pouch and the ammo pouch are reached."),
        header("Tutorial (vrtutorial)"),
        command("The Tutorial", "skill 0; map vrtutorial")
            .help("skill 0; map vrtutorial: the tutorial (a military base by day, 12 lessons and an arena; "
                  "Misc/quakevr/maps/vrtutorial_gen.py), on Easy as the start flow and the hub's button start it."),
        command("First Start Again", "vr_tutorial_started 0")
            .help("vr_tutorial_started 0: the next start of the game goes to the tutorial, as a new install's first start "
                  "does (the tutorial sets it back to 1 as it loads)."),
        cycle("Go to a Lesson", "vr_tutorial_goto",
            {{0.f, "-"}, {1.f, "1 Moving"}, {2.f, "2 Buttons"}, {3.f, "3 Jumping"}, {4.f, "4 Swimming"}, {5.f, "5 Healing"},
             {6.f, "6 Melee"}, {7.f, "7 A Fight"}, {8.f, "8 Weapons"}, {9.f, "9 Darkness"}, {10.f, "10 Throwing"},
             {11.f, "11 Fire"}, {12.f, "12 The Arena"}})
            .help("In vrtutorial: puts you at that lesson's start (its checkpoint, taken: you come back there). Doors "
                  "on the way stay as they are."),
        header("Trailer Scene (vrtrailer)"),
        command("The Trailer Scene", "map vrtrailer")
            .help("map vrtrailer: a bridge across a lake at night, the Super Axe on a pedestal half way (Dawn of the "
                  "Machine's data needed), a grunt on the far islet looking at the water, oblivious until hurt "
                  "(Misc/quakevr/maps/vrtrailer_gen.py). You start empty-handed; recording mode is on there: no tips, no "
                  "wrist messages, no head text."),
        command("Reset Trailer Scene", "vr_trailer_reset 1")
            .help("vr_trailer_reset 1: for a retake: the grunt back where he stood (his body, head, gibs, gun and backpack "
                  "gone), the Super Axe back on its pedestal (out of your hands and holsters), you at the start facing "
                  "along the bridge."),
        toggle("Trailer Scene Log", "vr_trailer_log")
            .help("vr_trailer_log 1: the grunt's state (enemy, frame, think, stealth state, oblivious), the axe's place "
                  "and yours, four times a second (the console). Off for takes: the lines would show."),
        header("Climbing"),
        command("Climbing Test Map", "map vrclimb").help("map vrclimb: rungs, ledges, a jump wall, moving and floating ledges."),
        command("To the Jump Wall", "setpos -40 -310 24 0 0 0; noclip")
            .help("In vrclimb: you facing a wall 96 high, 40 units ahead: walk into it, jump and grab its top (Climbing: "
                  "Mid-Air Grab Window)."),
        command("A Large Crate Ahead", "vr_test_spawn_dist 40; vr_test_spawn 120; impulse 241")
            .help("vr_test_spawn 120: a large crate (65 kg, 48 high) 40 units ahead, a face square to you: climb it "
                  "(Climbing: Climb Props). 121: two stacked, 122: one lying on its side, 123: a small one on a large one, "
                  "124: a small one."),
        command("Two Large Crates Stacked Ahead", "vr_test_spawn_dist 40; vr_test_spawn 121; impulse 241")
            .help("vr_test_spawn 121: two large crates, one on the other (96 high), 40 units ahead."),
        command("A Large Crate on Its Side Ahead", "vr_test_spawn_dist 40; vr_test_spawn 122; impulse 241")
            .help("vr_test_spawn 122: a large crate lying on its side (40 high), 40 units ahead."),
        slider("Props Hold Like the Level From", "vr_climb_prop_mass", 0.f, 200.f, 5.f, "%.0f kg")
            .help("vr_climb_prop_mass: a prop at least this heavy holds like a wall while you hang from it; lighter, your "
                  "weight pulls it (Light Prop Pull) and it tips or slides towards you, and your hand lets go. Large crate "
                  "65, explosive box 80, small crate 40, barrel 30. 0: every prop holds."),
        slider("Light Prop Pull", "vr_climb_prop_pull", 0.f, 100.f, 5.f, "%.0f kg")
            .help("vr_climb_prop_pull: the most weight a hand on a light prop pulls it with (your weight, Box3D's Player "
                  "Mass, shared by the holding hands, towards your body)."),
        slider("Prop Hold Tips Off At", "vr_climb_prop_tilt", 0.f, 45.f, 1.f, "%.0f deg")
            .help("vr_climb_prop_tilt: a prop's top turned this far from how it lay when you took hold lets your hand go."),
        slider("Light Prop Slips Off At", "vr_climb_prop_slip", 0.f, 32.f, 1.f, "%.0f units")
            .help("vr_climb_prop_slip: a light prop's hold moved this far from where you took it lets your hand go."),
        slider("Prop Knocked Away At", "vr_climb_prop_letgo_speed", 0.f, 600.f, 25.f, "%.0f u/s")
            .help("vr_climb_prop_letgo_speed: a held prop's hold whose speed changes this much in a frame (a blast, a hit) "
                  "lets your hand go."),
        header("Weapon Pickups (pickuptest: ... in the console)"),
        command("Every Weapon Pickup Ahead", "vr_pickup_test 1")
            .help("vr_pickup_test 1: every weapon pickup (the mission pack's when loaded) in rows ahead of you, each with "
                  "the same weapon dropped beside it, to compare them (Items: Weapon Pickups Look); their models, "
                  "offsets and boxes printed a second later."),
        command("Next Pair Before You", "vr_pickup_test 3")
            .help("vr_pickup_test 3: you moved before the next pickup and dropped weapon of the row above, for a close look."),
        command("Every Enemy Weapon Dropped Ahead", "vr_debug_box3d 1; vr_pickup_test 4")
            .help("vr_pickup_test 4: every weapon a monster can drop (the grunt's and enforcer's guns, the knights' "
                  "swords, the ogre's chainsaw, the random drops') dropped in rows ahead of you as a death drops it; "
                  "their convex pieces' cut times printed (box3d: ..., read from the disk or cut). Setup's preparation "
                  "run does this on the hub."),
        command("Weapons Lying Near", "vr_pickup_test 5")
            .help("vr_pickup_test 5: each weapon lying within 400 units (a map's, a drop): where it is, how fast it moves, "
                  "the lowest point of its turned drawn box and the floor under it: one sunk into or thrown off what it "
                  "lies on shows."),
        command("Floating Pickup Ahead", "vr_pickup_test 6")
            .help("vr_pickup_test 6: a super shotgun pickup hanging 40 units ahead of you, watched: reach into it slowly and "
                  "grip (Floating Pickup Grace), or swat it; whether it was taken or knocked loose prints (floatgrab: in the "
                  "console; developer 1 also prints each hand's reach and knock)."),
        header("Damage Credit (dmgcredit: ... in the console)"),
        command("Every Model and Limb Precached", "vr_prepare_models")
            .help("vr_prepare_models: every model of the game's folders (progs/*.mdl) and every monster's limbs loaded now, "
                  "so their skins' normal maps are made and kept on disk (cache/normalmaps) instead of at a map's first "
                  "load. Setup's preparation run does this on the hub (2-3 s; the count and time printed)."),
        command("Every Model of This Campaign Precached", "vr_prepare_models campaign")
            .help("vr_prepare_models campaign: the active campaign's own folder's models (progs/, every subfolder) and "
                  "their limbs loaded now. Setup's preparation does this on the start map of each installed campaign "
                  "with a folder of its own (Dimension of the Machine, Dawn of the Machine)."),
        header("Saves"),
        command("What Continue Would Load", "vr_continue_info")
            .help("vr_continue_info: the save the main menu's Continue row and the hub's Continue button load (the newest "
                  "loadable save of a campaign's map, an autosave or your own) and the newer ones passed over, printed."),
        command("List the Saves", "vr_save_info")
            .help("vr_save_info: every save in the game folder and autosave/, each with what it was played as (its "
                  "campaign, its Map Library package), its map's title and its line in the Load menu, printed."),
        command("List the Autosaves", "vr_autosave_list")
            .help("vr_autosave_list: the Load Autosave menu's list (the rotating autosaves kept, vr_autosave_slots per "
                  "map, newest first; this game's only unless vr_autosave_list_all), printed, and how long reading it "
                  "took; vr_autosave_load <n> loads the n-th."),
        command("List the Autosaves: Cold", "vr_autosave_list cold")
            .help("vr_autosave_list cold: as List the Autosaves, every file read again (as at the menu's first "
                  "opening): its time is the menu's hitch."),
        command("Open the Death Menu", "menu_autosaves death")
            .help("The menu a dead player gets (vr_death_menu): Respawn, Restart the Level, the autosaves; here alive, "
                  "to look at it."),
        header("Crowbar"),
        command("A Crowbar in Your Hand", "impulse 167").help("A crowbar in the main hand (impulse 187: the off hand)."),
        command("Drop a Crowbar Ahead", "impulse 217")
            .help("A crowbar dropped 48 units ahead of you, as a crate or a map places one (QC VR_Crowbar_Spawn), then the "
                  "report below."),
        command("Take the Nearest Crowbar", "impulse 216").help("The crowbar lying nearest you into an empty hand."),
        command("Report the Crowbars", "impulse 218").help("Prints each crowbar lying about and whether your hands hold one."),
        command("Hotspot Fit", "vr_hotspot_fit")
            .help("vr_hotspot_fit: the weapon in the main hand (any): where the hand and the fist are on its model, its "
                  "tip's distance, and where the off hand must move to take each hotspot (a blade: its ends and middle)."),
        header("Enemy Guns"),
        command("A Grunt's Gun in Your Hand", "impulse 165")
            .help("A full grunt's burst rifle (Enemy Weapons: Rounds) in the main hand (impulse 185: the off hand)."),
        command("An Enforcer's Rifle in Your Hand", "impulse 166")
            .help("A full enforcer's laser rifle (Enemy Weapons: Shots) in the main hand (impulse 186: the off hand)."),
        command("Take the Nearest Enemy Gun", "impulse 212")
            .help("The grunt's gun or enforcer's rifle lying nearest you (dropped) into an empty hand, its ammo as it "
                  "was."),
        command("Report the Enemy Guns", "impulse 213").help("Prints each enemy gun in your hands and its ammo."),
        command("One Shot Left", "impulse 214").help("The enemy guns in your hands left with one shot: to see one run dry."),
        command("Weapon Effects Test", "vr_weaponfx_test 1 3")
            .help("vr_weaponfx_test 1 3: the main hand's weapon kicks and flashes as if it fired (its Effects), with 3 "
                  "tracers (no shot)."),
        toggle("Print Weapon Effects", vr_debug_weaponfx)
            .help("vr_debug_weaponfx: each shot's recoil, flash and tracers (2: and the recoil each frame); the shotgun's auto "
                  "pump strokes (start, back, home) and when its shell leaves."),
        slider("Hold the Auto Pump", vr_autopump_hold, -0.05f, 1.f, 0.05f, "%.2f")
            .help("vr_autopump_hold: every shotgun's fore-end held at that point of its auto pump's stroke, to look at it "
                  "(0.35-0.45: at the back; below 0: off)."),
        header("Flung Props"),
        slider("Fling Speed", vr_test_fling_speed, 1.f, 40.f, 1.f, "%.0f m/s").extend(),
        cycle("Fling At", vr_test_fling_at, {{0.f, "Nearest Monster"}, {1.f, "You"}}),
        toggle("Fling Away From It", vr_test_fling_away)
            .help("The prop starts inside the edge of its box and flies away from it (a prop batted away from you): it "
                  "must never hurt."),
        command("Two-Handed Throw", "developer 1; impulse 204")
            .help("A gib ahead of you as if just thrown with both hands: prints (2h test:) whether each hand may strike "
                  "and the gib can be struck, now and 0.5 s later (Throwing and Physics: Two-Handed Throws)."),
        command("Gib in the Off Hand", "developer 1; impulse 252")
            .help("A gib or head (each press the next kind), or the prop Which Gib picks, in your off hand: take it "
                  "with the other hand too and throw it with both, or let go with the first hand and grip it again. "
                  "'gib:' lines say what burst it (Real Gib, below)."),
        cycle("Which Gib", vr_test_held_pick,
              {{-1.f, "Each in Turn"}, {0.f, "Small Gib (gib1)"}, {1.f, "Player's Head"}, {2.f, "Torso (gib2)"},
               {3.f, "Big Chunk (gib3)"}, {4.f, "Grunt's Head"}, {5.f, "Ogre's Head"}, {6.f, "Knight's Head"},
               {7.f, "Zombie's Head"}, {8.f, "Fiend's Head"}, {9.f, "Nearest Rock"}, {10.f, "Hand Grenade"},
               {11.f, "Nearest Shells Box"}, {12.f, "Nearest Brick"}})
            .help("vr_test_held_pick: the gib or head Gib in the Off Hand gives (Each in Turn: the next each press). The "
                  "torso and the big chunk are the large ones (Misc/quakevr/gib_2h_models_test.sh throws each). Rock, "
                  "Shells Box, Brick: the nearest lying about; Hand Grenade: one from the pouch (Misc/quakevr/"
                  "twohand_regrip_test.sh takes each in both hands and hands it over)."),
        toggle("Into the Main Hand", vr_test_held_hand)
            .help("vr_test_held_hand: Gib in the Off Hand fills the main hand instead (hold its grip): a prop to hit the "
                  "super shotgun's barrels open or shut with, the gun in the off hand (reload_test.sh section 11)."),
        toggle("Real Gib", vr_test_held_destroy)
            .help("vr_test_held_destroy: the off hand's test gib can be burst as a real one (shot, struck, thrown hard "
                  "at a wall). Misc/quakevr/gib_2h_throw_test.sh throws it with both hands along several arcs."),
        command("Fling the Nearest Prop", "impulse 232")
            .help("Sends the loose prop nearest you (put a box there first) at it, as if batted or knocked flying; "
                  "developer 1 prints the hit (prop: flung ...)."),
        slider("Throw Up Speed", vr_test_throw_up_speed, 0.f, 25.f, 0.5f, "%.1f m/s")
            .help("0: let go of gently (dropped, not thrown): it must never hurt you."),
        toggle("Throw Up From Your Body", vr_test_throw_up_body)
            .help("Throw the Nearest Prop Up lets it go at your chest, inside your body, as a long heavy thing let go "
                  "of at the body (not from over your head)."),
        command("Throw the Nearest Prop Up", "developer 1; impulse 222")
            .help("The loose prop nearest you thrown straight up as yours from just over your head: it falls back on you "
                  "and hurts you (Your Throws Spare You For, on Throwing and Physics). Prints the throw and the hit."),
        header("Thrown Axe"),
        cycle("Axe Throw", vr_test_axe,
            {{0.f, "Blade First"}, {1.f, "Flat"}, {2.f, "Handle First"}, {3.f, "Spinning"}, {4.f, "Overhand"},
             {5.f, "Sidearm"}, {6.f, "Sloppy"}, {8.f, "Upright"}, {9.f, "Knife-Style"}, {10.f, "Spear-Like"},
             {11.f, "Flat (Hand)"}, {12.f, "Handle First (Hand)"}})
            .help("How Throw an Axe throws it: blade first (the edge upright), the blade's side first, the handle's end "
                  "first, blade first spinning end over end; or as a hand would, turned and spun a little differently "
                  "each time: overhand (end over end), sidearm (the blade level, spun about the upright), sloppy (any "
                  "way), upright (pushed, the handle upright, the blade ahead), knife-style (held by the head, the "
                  "handle ahead, flicked end over end), spear-like (the head ahead), flat (the blade's side ahead) and "
                  "handle first. The hand's throws aim up to meet what is ahead at eye level."),
        slider("Axe Speed", vr_test_axe_speed, 2.f, 20.f, 1.f, "%.0f m/s").extend(),
        cycle("Throw Instead", vr_test_axe_what, {{0.f, "The Axe"}, {1.f, "A Gib"}, {2.f, "An Explosive Box"}})
            .help("Throws a gib or an explosive box (one that never blows up) the same way instead of the axe: to "
                  "compare their spin in the air (Debug: Spin in the Air)."),
        cycle("Weapon Instead", vr_test_axe_weapon,
            {{0.f, "The Axe"}, {2.f, "Axe (as a throw)"}, {4.f, "Shotgun"}, {7.f, "Super Nailgun"}, {12.f, "Laser Cannon"}})
            .help("Throws that weapon instead of the axe, hurting as your hand's throws do (their damage by speed and "
                  "weight; the training dummy shows it): Axe Speed is its speed."),
        toggle("Axe Hurts", vr_test_axe_damage).help("Off: its blow does no damage (to watch a monster bleed)."),
        cycle("Axe At", vr_test_axe_at, {{0.f, "Ahead"}, {1.f, "Nearest Monster"}, {2.f, "Nearest Door"}, {3.f, "Nearest Prop"},
                                              {4.f, "Nearest Button"}})
            .help("What Throw an Axe throws at: ahead of you, or the nearest live monster, door, loose prop or wall "
                  "button (you are moved to face it, level with its middle; a button: square to its wall)."),
        slider("Axe Range", vr_test_axe_dist, 0.f, 400.f, 20.f, "%.0f units")
            .help("You are moved first to this far from what is ahead (0: where you are)."),
        slider("Axe Height", vr_test_axe_up, -30.f, 40.f, 1.f, "%+.0f units")
            .help("At a target: thrown this far above its middle (15: a grunt's head, to behead it: Gore > Decapitation)."),
        command("Throw an Axe", "impulse 209").help("An axe thrown straight ahead from your eyes (not yours: a new one)."),
        command("Hand on the Stuck Axe", "impulse 207")
            .help("Moves you so that your main hand is on the handle of the nearest stuck axe: grip to pull it out."),
        command("Report the Axes", "impulse 208").help("Prints each thrown axe: what it is stuck in, where (developer 1)."),
        command("Push the Props Axes Are In", "vr_test_axe_host 1; impulse 208")
            .help("Each prop an axe is stuck in (a box, a crate, a pickup) is pushed up and across: the axe moves with it "
                  "(Report the Axes again to see; vr_test_axe_host 1)."),
        command("Break the Props Axes Are In", "vr_test_axe_host 2; impulse 208")
            .help("Each prop an axe is stuck in is broken (an explosive box blows up, a crate breaks, a pickup is taken by "
                  "you): the axe falls (vr_test_axe_host 2)."),
        command("Slide the Nearest Prop", "vr_physics_fling nearest 150")
            .help("Sends the loose prop nearest you skidding along the floor the way you face, at 150 units/s (5.7 m/s): "
                  "its scrape (Physics Sounds; Logs: Physics Sounds prints it)."),
        header("At You"),
        cycle("Projectile", vr_test_projectile,
            {{0.f, "Knight's Spike"}, {1.f, "Enforcer's Laser"}, {2.f, "Scrag's Spit"}, {3.f, "Vore's Ball"},
             {4.f, "Ogre's Grenade"}, {5.f, "Zombie's Flesh"}})
            .help("What Fire at Me fires at your face from 300 units: to bat back with a weapon, or catch (a grenade)."),
        slider("From the Left", vr_test_projectile_side, -90.f, 90.f, 15.f, "%.0f deg")
            .help("Degrees to your left of ahead it comes from (negative: from the right)."),
        command("Fire at Me", "impulse 246").help("Fires the Projectile at you now."),
        command("Make an Ogre Throw", "impulse 240").help("The nearest ogre or zombie throws at you now."),
        cycle("Grenade Shot", vr_test_grenade_shot, {{10.f, "Pellet"}, {1.f, "Nail"}, {0.f, "Rocket"}, {4.f, "Laser"}})
            .help("What Shoot the Nearest Grenade fires from your eyes (Shoot Grenades, on Batting and Catching)."),
        command("Shoot the Nearest Grenade", "developer 1; impulse 210")
            .help("Fires the Grenade Shot at the nearest grenade (an ogre's in flight: Fire at Me with the Ogre's Grenade "
                  "first; yours; a dud). Prints what it shot, how far, its shot box (impulse 210)."),
        slider("Dud Distance", vr_test_grenade_dist, 64.f, 1024.f, 32.f, "%.0f units")
            .help("How far ahead Drop a Dud Ahead puts the grenade."),
        slider("Dud Height", vr_test_grenade_height, 0.f, 64.f, 2.f, "%.0f units")
            .help("How high over the floor Drop a Dud Ahead puts the grenade (it falls): at hand height to punch or swing at "
                  "it (Blow Speed to Set Off, on Batting and Catching)."),
        toggle("Dud Is Yours", vr_test_grenade_yours)
            .help("On: the dud is yours (your blows pass through your own grenades). Off: nobody's, as an enemy's grenade "
                  "lying about: your blows meet it (bat it, or set it off when very strong)."),
        command("Drop a Dud Ahead", "developer 1; impulse 211")
            .help("Takes a hand grenade from your pouch into your empty off hand, lets go of it unarmed (a dud), and puts it "
                  "on the floor ahead: a trap to shoot from afar (impulse 211)."),
        header("Getting Hit"),
        command("Hit Me From the Left", "vr_pain_test 15 90")
            .help("vr_pain_test 15 90: a 15 point hit from your left (no damage): the hands knocked right, the left one more; "
                  "the controllers buzz (When You're Hit, Damage and Knockback)."),
        command("Hit Me From the Right", "vr_pain_test 15 -90").help("vr_pain_test 15 -90: as above, from your right."),
        command("Hit Me From Ahead", "vr_pain_test 15 0").help("vr_pain_test 15 0: from ahead: both hands knocked back."),
        command("Rocket From Ahead", "vr_pain_test 80 0").help("vr_pain_test 80 0: a big hit: the Largest Knock."),
        command("Fall", "vr_pain_test 10 none").help("vr_pain_test 10 none: a hit from no direction (a fall, lava): the hands drop."),
        toggle("Print Hits", vr_debug_pain)
            .help("Prints each hit (painhit: damage, side, knock, buzz) and the hands' knock each frame (painknock, cm along "
                  "your body) (vr_debug_pain)."),
        header("Grappling Hook"),
        command("Report the Hooks", "impulse 239").help("Prints every hook: its state, what it is in, where its gun is, the "
                                                         "rope's length and path (needs Developer Messages)."),
        command("Print the Ropes", "vr_grapple_rope_dump; vr_grapple_rope_draw_dump")
            .help("Prints each rope's corners (the server's) and its drawn chain: points inside the world, pieces through it."),
        header("Stamina"),
        info([]() -> const char* { return staminaReadout(); }),
        command("Deplete Stamina", "vr_stamina_set 0").help("vr_stamina_set 0: no stamina left (tired arms: heavy hands; shaking while you hang). Hanging with none, your hands let go."),
        command("Nearly Empty", "vr_stamina_set 0.1").help("vr_stamina_set 0.1: a tenth of your stamina left, enough to hang a moment."),
        command("Half Stamina", "vr_stamina_set 0.5").help("vr_stamina_set 0.5: half your stamina left."),
        command("Quarter Stamina", "vr_stamina_set 0.25").help("vr_stamina_set 0.25: a quarter of your stamina left (the run a little slower: Slower When Tired)."),
        command("Restore Stamina", "vr_stamina_set 1").help("vr_stamina_set 1: rested."),
        toggle("Hold Stamina", vr_debug_stamina_hold).help("Keeps your stamina where it is, or where the buttons above put it: nothing spends it and it doesn't come back (vr_debug_stamina_hold)."),
        header("Loading Notice"),
        cycle("Loading Notice", vr_loading_notice, {{0.f, "Off"}, {1.f, "Headset"}, {2.f, "Mock Too"}})
            .help("\"Loading...\" in front of you before a level change (a map's exit, a map transition, a map started "
                  "from the menus, a save loaded, a death's restart): the load waits until two headset frames showed "
                  "it, as the runtime keeps showing the last frame while the game loads (vr_loading_notice). Mock Too: "
                  "the mock headset's test runs too."),
        cycle("Loading Notice Hold", vr_loading_hold, {{0.f, "Off"}, {1.f, "1 s"}, {3.f, "3 s"}})
            .help("Keeps the notice up this long before each load starts, to look at it (vr_loading_hold; Off: two "
                  "frames)."),
        command("Preview Loading Notice", "vr_loading_preview 3")
            .help("Shows the notice for 3 seconds without a load (vr_loading_preview [seconds])."),
        command("Restart Map (Shows It)", "restart fresh")
            .help("Starts this map again from its beginning, through the notice (restart fresh)."),
        header("Stuck in Walls"),
        toggle("Unstick", vr_unstick)
            .help("Found inside a wall, a door, a button or a lift, you're moved to the nearest free spot (vr_unstick). "
                  "Off: only Quake's small nudge up."),
        command("Stuck Info", "vr_stuck_info")
            .help("Prints where you are, what you're inside of, the doors, buttons and lifts near you, and how often "
                  "you and monsters were freed."),
        toggle("Unstick Monsters", vr_unstick_monsters)
            .help("A monster found inside a wall, the floor, a door or a lift, not moving, for the time below is moved to "
                  "the nearest free spot (vr_unstick_monsters; Developer Messages prints each). Off: it stays stuck."),
        slider("Unstick Monsters After", vr_unstick_monsters_time, 0.2f, 5.f, 0.2f, "%.1f s")
            .help("How long a monster stays stuck inside something before it is moved out (vr_unstick_monsters_time)."),
        command("Sink the Nearest Monster", "vr_stuck_sink")
            .help("Puts the live monster nearest you 24 units down into the floor (vr_stuck_sink [edict] [depth]): with "
                  "Unstick Monsters on it should climb out within a second; Stuck Info counts it."),
        command("Trace Ahead", "vr_stuck_trace 32 0 0")
            .help("Prints where your box stops moving 32 units along the world's x axis and the plane it meets, and "
                  "Quake's own hull's answer (vr_stuck_trace <dx> <dy> <dz> [edict])."),
        toggle("Print Walk Moves", vr_debug_walkmove)
            .help("Prints each of your walk moves: the slide, the step up, the planes met, the ledge test "
                  "(vr_debug_walkmove). For a report of stairs or slopes you can't walk up."),
        header("Player Hitbox (Prototype)"),
        open("Player Hitbox Settings", pageIndex(pageHitbox)).help("Movement > Player Hitbox: the widths and their toggles."),
        open("Monster Hitbox Settings", pageIndex(pageMonsterHitbox)).help("Movement > Monster Hitbox: monsters' widths by class, and their walk tests."),
        command("Hitbox Stats", "vr_hull_stats").help("Prints the map's rebuilt brushes and compiled hull: counts, memory, build times."),
        cycle("Keep Hitboxes for Reloads", vr_hull_keep, {{0.f, "Off"}, {1.f, "1 Map"}, {2.f, "2 Maps"}, {4.f, "4 Maps"}})
            .help("The map's brushes and compiled hulls kept in memory when it is left, for a load of the same map again "
                  "(a death's reload, restart, a changelevel back): that load skips their build (vr_hull_keep). Off: "
                  "built at every load."),
        cycle("Hitboxes on Disk", vr_hull_cache, {{0.f, "Off"}, {1.f, "On"}, {2.f, "Check"}})
            .help("A big map's compiled hulls kept on disk (cache/hulls) and read at its next load instead of compiled "
                  "again (vrstart: 9 s to 0.1 s). Check: read, then compiled anyway and compared (Hitbox Stats counts "
                  "them). Off: compiled at every load (vr_hull_cache)."),
        cycle("Hitbox Build Memory", vr_hull_build_mb, {{0.f, "No Limit"}, {512.f, "512 MB"}, {1024.f, "1 GB"}, {2048.f, "2 GB"}})
            .help("How much memory a big map's hitbox build (its first load after an install or update) may hold at once "
                  "(estimated): past it a tree's build waits for another to end. vrstart: about 330 MB a tree, three at "
                  "once in 1 GB. No Limit: all at once (about 1 GB more, 5-10% quicker) (vr_hull_build_mb)."),
        header("Teleporters: Crossing One (the start map; developer 1: VR portal: ...)"),
        command("The Gates In This Map", "vr_portals_info")
            .help("vr_portals_info: every teleporter built for this map - its plane, its opening, its trigger brush - and "
                  "where your body is against each: your box, your torso's middle plane, its distance from the gate's "
                  "plane, whether it is over the opening and how near your box can bring it. You are carried through "
                  "only after that plane reaches the gate's and your collision box fits the opening; leaning or reaching "
                  "with the hands does not cross, and a gate's frame stops you as a wall does."),
        header("Teleporters: Test Map (vrteleporters)"),
        command("Teleporter Test Map", "map vrteleporters")
            .help("map vrteleporters: teleporter pairs of every size (crate, player, shambler, very wide), flush with the floor "
                  "and in frames with sills, at 90 and 45 degrees, a loop, between floor heights and by a pool. Every gate "
                  "goes both ways; each room's buttons spawn a grunt, dog, ogre, shambler or scrag by its far wall."),
        command("To the Flush Gates", "setpos -256 576 24 0 90 0; noclip")
            .help("In vrteleporters: facing the player-sized flush gate (its bottom at the floor). The north gallery behind "
                  "it is where it leads, so a monster chasing you through walks straight into it."),
        command("To the Framed Gates", "setpos 1180 576 24 0 90 0; noclip")
            .help("In vrteleporters: facing the player-sized gate in a frame with a 16-unit sill (a step); the next one east "
                  "has a 32-unit sill (a jump: monsters can't)."),
        command("To the Turning Gates", "setpos -1280 640 24 0 90 0; noclip")
            .help("In vrteleporters: facing the gate that comes out of the next room's east wall (90 degrees); the loop is "
                  "left and right of you, the 45-degree wall behind you to the left."),
        command("Into the Loop", "setpos -1560 560 24 0 180 0; noclip")
            .help("In vrteleporters: 40 units from the loop's west gate, which comes out of the east one: you see your own "
                  "back (with its head) and, through the gate beyond it, yourself again, as many gates deep as Graphics > "
                  "Teleporters > Teleporters Within Teleporters (vr_portals_view prints the views drawn at each depth)."),
        command("To the Heights and Water", "setpos -400 -960 24 0 180 0; noclip")
            .help("In vrteleporters: facing the floor-level gate that comes out over the 128-high platform; the pool's two "
                  "gates are in the east and south walls."),
        toggle("Print Gate Cuts", "vr_portals_debug_split")
            .help("vr_portals_debug_split 1: each frame, every entity drawn cut by a teleporter (where, the plane, how far "
                  "through), the force grab's beam end, and each thrown or rigid thing's middle and why a gate did not "
                  "take it. -1 (console) also follows the main hand's held object, a number that entity, every frame."),
        command("Into the Loop, Lightning Gun", "setpos -1580 560 24 0 180 0; noclip; impulse 161")
            .help("In vrteleporters: right at the loop's west teleporter with the lightning gun in your main hand. Shoot your "
                  "own back through it: shots, nails, rockets and the bolt come out of the east teleporter and hit you "
                  "(the bolt's 600 units reach round this room only from close to the teleporter; god mode: no damage)."),
        command("Stuck in a Teleporter's Wall?", "vr_portals_stuck")
            .help("vr_portals_stuck: whether your body is in the wall where you stand (a teleporter's split body "
                  "included), the teleporter side nearest your torso and how far in front of its plane it is (under 0: "
                  "behind it), and how many times you were got out of the wall behind a teleporter."),
        toggle("Get Out of a Teleporter's Wall", "vr_portals_unstick")
            .help("vr_portals_unstick 1 (default): if you end up in the wall behind a teleporter, you are carried on "
                  "through it (your torso past its plane and room at the far side) or put back in front of it at once. "
                  "0: off, to see a softlock as it was."),
        header("Cheats"),
        command("God Mode", "god").help("god: takes no damage (again: takes damage)."),
        command("Quad Damage", "impulse 255").help("Quad Damage for 30 seconds."),
        command("All Weapons", "impulse 9").help("Every weapon and full ammo."),
    };
}

#include "vr_menu_automation.inc" // Debug > Automation & Self-Tests (the rows moved off the Tests, Reports, Tools and Profiling pages)
