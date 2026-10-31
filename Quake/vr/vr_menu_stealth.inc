// vr_menu_stealth.inc -- Combat > Stealth AI (QC vr_stealth.qc; docs/vr-port/STEALTH.md): monsters Idle, Alert
// (investigating what they noticed) or Hostile; and Debug > Tests > Stealth AI. Included by vr_menu.cpp (inside its
// namespace).

[[nodiscard]] za::Vector<Item> pageStealth()
{
    return {
        row(opt::vr_ai_enhanced),
        header("Seeing You"),
        row(opt::vr_stealth_meter),
        row(opt::vr_stealth_contact),
        row(opt::vr_stealth_sneak),
        header("Hearing"),
        row(opt::vr_stealth_noise),
        row(opt::vr_stealth_run_speed),
        row(opt::vr_stealth_noise_guns),
        row(opt::vr_stealth_noise_blasts),
        row(opt::vr_stealth_gates),
        row(opt::vr_stealth_seethrough),
        row(opt::vr_stealth_ambush_deaf),
        row(opt::vr_stealth_senses),
        header("Investigating"),
        row(opt::vr_stealth_investigate),
        row(opt::vr_stealth_lose),
        row(opt::vr_stealth_lose_time),
        row(opt::vr_stealth_lose_far),
        row(opt::vr_stealth_chase_trail),
        row(opt::vr_stealth_chase_predict),
        header("Getting Round"),
        row(opt::vr_ai_props),
        row(opt::vr_ai_prop_push),
        header("Spreading the Alarm"),
        row(opt::vr_stealth_share_near),
        row(opt::vr_stealth_share_view),
        header("What Else Draws Them"),
        row(opt::vr_stealth_flashlight),
        header("Showing It"),
        row(opt::vr_stealth_gem),
        row(opt::vr_stealth_marks),
        open("Tests", pageIndex(pageStealthTests)).help("Debug > Tests > Stealth AI."),
    };
}

[[nodiscard]] za::Vector<Item> pageStealthTests()
{
    return {
        header("On e1m1 (map e1m1; a grunt put ahead of you first)"),
        command("Put a Grunt Ahead", "god; vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241")
            .help("God mode and a grunt 150 units ahead (the debug spawner's)."),
        command("Run the Scenes", "vr_stealth_test 1")
            .help("13 scenes on the nearest monster: dark and lit in front, a glimpse, running behind it, a noise through "
                  "a wall, touch, a map trigger, a sneak hit, a grazing shot, a body, the Enhanced AI switch, an "
                  "investigation, losing you; `stealthtest:` lines in the console (about a minute and a half)."),
        command("A Crate Knocked Behind It", "vr_stealth_test 2")
            .help("A crate blasted behind the nearest monster: its knock heard, it walks there."),
        command("The Flashlight on It", "vr_flashlight_clip_head right; vr_flashlight_toggle; vr_stealth_test 32")
            .help("Your lamp on your head, on: look at the nearest monster. On its back: it turns Alert; in its face: it spots you."),
        header("Their Own Monsters (put down and taken away)"),
        command("Your Shot Heard", "god; vr_stealth_test 100")
            .help("Three grunts behind you, your main hand's weapon's reach scaled to the room: 0.8 of it in the open, 1.2, "
                  "and 0.8 behind a wall. Fire within 5 s of \"gun ready\": only the first should hear it."),
        command("Explosions Heard", "god; vr_stealth_test 101")
            .help("The same three round an explosion of 50 (heard at 800 units) at your feet, then one of 200 (1600)."),
        command("Every Monster Kind", "god; vr_stealth_test 106")
            .help("Each kind installed (id1's, the expansions'), 200 units ahead: the rules apply (or it is a flyer, a "
                  "swimmer, a boss, or dormant), dark: idle; a knock: Alert; lit: it spots you (about 4 minutes)."),
        command("Infighting", "god; vr_stealth_test 107")
            .help("A grunt shot by another fights it; the other dead, it stands Idle again and doesn't see you in the dark."),
        command("Lava's Edge", "god; notarget; vr_stealth_test 108")
            .help("The nearest lava or slime (e1m7's): a grunt Alert at a point across it stops at the edge."),
        command("Through Teleporters", "map vrteleporters; wait60; god; setpos -1100 1408 24 0 180 0; wait5; noclip 0; vr_stealth_test 110")
            .help("vrteleporters (it loads it), you in room U: a grunt in room T sees you through T's north gate, walks "
                  "through to where you stood and back; a knock in U heard through the gate, the same walk; a dog sees "
                  "you through it and comes through; Quake's AI: only the grunt sees you through (about 2 minutes)."),
        command("A Horde (Profiler)", "god; vr_stealth_test 103")
            .help("60 grunts round you, one in six dead, a knock every 2 s, for 20 s: vr_profile's \"stealth\" scope (under "
                  "quakec) is their AI's time."),
        command("Coop: Lights, Lamps, the Meter", "god; vr_stealth_test 102")
            .help("With a second player in: each one's light and flashlight known to the host, a grunt spotting the lit one "
                  "as fast as one player alone, the other player's lamp alerting it."),
        command("Save Mid-Investigation", "god; vr_stealth_test 104")
            .help("A grunt Alert, walking to a point behind it: save now, load, then the next row."),
        command("After the Load", "vr_stealth_test 105")
            .help("The Alert monster carries on: its walk, its search, back to Idle at its post."),
        header("You"),
        command("Your Numbers", "vr_stealth_test 22")
            .help("For 4 s: your speed, the light on you, crouching, your visibility (the gem's)."),
        command("The Map's Light", "vr_stealth_test 26")
            .help("The light where this map's monsters and pickups stand, as a histogram (the Dark and Lit thresholds)."),
        header("Logging"),
        row(opt::vr_stealth_debug_meters),
        row(opt::vr_stealth_debug_meters_range),
        row(opt::vr_stealth_debug),
        row(opt::vr_stealth_light_force),
    };
}
