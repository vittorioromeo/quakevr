// vr_menu_stealth.inc -- Combat > Stealth AI (QC vr_stealth.qc; docs/vr-port/STEALTH_PLAN.md): monsters Idle, Alert
// (investigating what they noticed) or Hostile; and Debug > Tests > Stealth AI. Included by vr_menu.cpp (inside its
// namespace).

[[nodiscard]] za::Vector<Item> pageStealth()
{
    return {
        toggle("Enhanced AI", vr_ai_enhanced)
            .help("Every rule on this page at once. Off: Quake's monster AI, exactly (they see you at once, hear nothing); "
                  "switched mid-game, every monster goes back to it at once (an investigating monster stops, idle). For a "
                  "map that doesn't play well with the stealth rules."),
        header("Seeing You"),
        toggle("Suspicion Meter", vr_stealth_meter)
            .help("A monster that sees you fills a meter, faster the more light is on you, the nearer, the more in front "
                  "of it, the more you move (crouching slows it). Partly full it turns to look (Alert); full, it attacks. "
                  "Off: Quake's sight (seen at once in its view, or near)."),
        slider("Time to Spot You", vr_stealth_meter_time, 0.25f, 6.f, 0.25f, "%.2f s").extend(0.05f, 30.f)
            .help("How long the meter takes to fill with you in full light, near, in front of it, walking. Standing still "
                  "takes twice as long; half lit, about twice again."),
        slider("Meter Fades", vr_stealth_meter_decay, 0.f, 1.f, 0.05f, "%.2f a second")
            .help("How fast the meter empties while it doesn't see you (a share of it a second)."),
        slider("Glimpse At", vr_stealth_glimpse, 0.05f, 0.95f, 0.05f, "%.2f")
            .help("The meter's share at which the monster turns to look at where it glimpsed you (Alert)."),
        slider("Dark Up To", vr_stealth_light_dark, 0.f, 64.f, 1.f, "%.0f").extend(0.f, 255.f)
            .help("Light on you (Quake's light: 128 full) at or below which you are not seen at all, however near: only "
                  "touch, noise or your flashlight give you away. Your flashlight, a held torch and gunfire's flashes "
                  "light you too."),
        slider("Fully Lit From", vr_stealth_light_bright, 16.f, 160.f, 2.f, "%.0f").extend(1.f, 255.f)
            .help("Light on you at or over which you are seen as fast as can be. The maps' monsters and pickups mostly "
                  "stand in 32 to 96."),
        slider("Crouching", vr_stealth_crouch, 0.f, 1.f, 0.05f, "x%.2f")
            .help("The meter's speed in a full crouch (your eyes 40% under your standing height); in between, between."),
        slider("Standing Still", vr_stealth_still, 0.f, 1.f, 0.05f, "x%.2f")
            .help("The meter's speed while you stand still (walking 1, running 2)."),
        slider("Sight Range", vr_stealth_sight_range, 250.f, 4000.f, 50.f, "%.0f units")
            .help("How far a monster can make you out at all; nearer, the faster (full speed within 128 units)."),
        slider("Out of the Corner of Its Eye", vr_stealth_peripheral, 0.f, 1.f, 0.05f, "x%.2f")
            .help("The meter's speed with you to its side (out of its front cone); behind it, it sees nothing."),
        toggle("Touching Gives You Away", vr_stealth_contact)
            .help("Your body, either hand or a prop you hold touching a monster: it spots you at once."),
        slider("Sneak Attack Damage", vr_stealth_sneak, 1.f, 3.f, 0.05f, "x%.2f").extend(0.f, 10.f)
            .help("Any damage you deal (a blow, a shot, an explosion) to a monster that hasn't noticed anything. 1: none."),
        header("Hearing"),
        toggle("Monsters Hear Noises", vr_stealth_noise)
            .help("Noises draw monsters (Alert: they come to look): your running, hard landings, props knocking, blows, "
                  "shots, explosions; louder ones farther. A noise you make in a monster's sight gives you away. Walls "
                  "muffle them; a thrown prop draws them to where it lands, not to you."),
        slider("Running From", vr_stealth_run_speed, 100.f, 400.f, 10.f, "%.0f u/s")
            .help("Moving faster than this is running, and heard (full stick: 320; walking and crouching are quiet). "
                  "Climbing and shimmying are silent."),
        slider("Running Heard At", vr_stealth_noise_run, 0.f, 1500.f, 25.f, "%.0f units"),
        slider("Props Heard At", vr_stealth_noise_props, 0.f, 3000.f, 50.f, "%.0f units")
            .help("A prop's loudest knock (a heavy crate thrown hard); quieter ones nearer."),
        slider("Shots Heard", vr_stealth_noise_guns, 0.f, 3.f, 0.05f, "x%.2f")
            .help("Times each weapon's reach (a shotgun 1100 units, the super shotgun 1300, nailguns 800-900)."),
        slider("Blows Heard", vr_stealth_noise_melee, 0.f, 3.f, 0.05f, "x%.2f")
            .help("Times a landed blow's reach (200 units for a punch; stronger blows and heavier weapons louder)."),
        slider("Explosions Heard", vr_stealth_noise_blasts, 0.f, 3.f, 0.05f, "x%.2f")
            .help("Times an explosion's reach (8 units a point of its damage, at least 800: a rocket's or a grenade's 960)."),
        slider("Round a Corner", vr_stealth_noise_wall, 0.f, 1.f, 0.05f, "x%.2f")
            .help("A noise's reach kept with a wall or a door in the line but the place in the monster's sight (a doorway)."),
        slider("Through Solid Walls", vr_stealth_noise_solid, 0.f, 1.f, 0.05f, "x%.2f"),
        slider("Past a Prop", vr_stealth_noise_absorb, 0.f, 1.f, 0.05f, "x%.2f"),
        toggle("Through Slipgates", vr_stealth_gates)
            .help("Seamless slipgates that come out of another gate (Graphics > Slipgates): every monster sees you "
                  "through them (not only ranged ones), noises carry through them as through an opening, and an Alert "
                  "monster walks through to look (Monsters Walk Through on). Off: sight through them is ranged "
                  "monsters' only, and noises stay in their room."),
        toggle("See-Through Walls", vr_stealth_seethrough)
            .help("Grates, fences, webs and glass (see-through textures and walls): monsters see you through them, your "
                  "flashlight's beam reaches them through them, and noises carry through as in the open. Off: they block "
                  "sight and muffle sound as solid walls."),
        toggle("Deaf Ambushers", vr_stealth_ambush_deaf)
            .help("Monsters a map placed in ambush don't hear noises or other monsters (Quake's meaning of the flag); "
                  "they still see you."),
        toggle("Senses by Kind", vr_stealth_senses)
            .help("Dogs hear far better, fiends and shamblers better, zombies are near deaf and half blind, ogres a bit "
                  "dull."),
        header("Investigating"),
        toggle("Walk to It", vr_stealth_investigate)
            .help("An Alert monster turns to what it noticed, then walks there and searches; off: it only turns and looks."),
        slider("Turn Speed", vr_stealth_turn, 0.1f, 1.f, 0.05f, "x%.2f")
            .help("An Alert monster's turn, times its own (warily slower)."),
        slider("Search For", vr_stealth_search_time, 0.f, 20.f, 0.5f, "%.1f s")
            .help("How long it searches the spot before it walks back to its post."),
        slider("Wary After", vr_stealth_sensitive, 1.f, 3.f, 0.05f, "x%.2f")
            .help("Its meter's speed for a while after a search that found nothing."),
        slider("Wary For", vr_stealth_sensitive_time, 0.f, 120.f, 5.f, "%.0f s"),
        toggle("Lose You", vr_stealth_lose)
            .help("A monster that lost sight of you goes to where it saw you last and searches there, then gives up."),
        header("Spreading the Alarm"),
        slider("Near Monsters Join", vr_stealth_share_near, 0.f, 1000.f, 16.f, "%.0f units")
            .help("A monster this near another that turns Alert or attacks (in line, not through a wall) does too. 0: off."),
        slider("Monsters in Sight Join", vr_stealth_share_view, 0.f, 3000.f, 50.f, "%.0f units")
            .help("One that sees another attack within this does too (turning Alert: within half). 0: off."),
        header("What Else Draws Them"),
        slider("Shots Grazing", vr_stealth_graze, 0.f, 256.f, 8.f, "%.0f units")
            .help("Your shot passing this near a monster alerts it, however far it came from. 0: off."),
        toggle("Flashlight Beams", vr_stealth_flashlight)
            .help("Your flashlight on a monster: it comes towards the light (and its glare shows you). Its beam seen "
                  "landing nearby: it looks about for where it came from."),
        slider("Torches Seen At", vr_stealth_torch, 0.f, 1500.f, 25.f, "%.0f units")
            .help("A lit torch you hold or threw, seen this near: it comes to look. 0: off."),
        slider("Bodies Seen At", vr_stealth_corpses, 0.f, 1500.f, 25.f, "%.0f units")
            .help("Corpses and gibs seen this near alert monsters (each body raises the alarm for 30 s). 0: off."),
        header("Showing It"),
        toggle("Visibility Gem", vr_stealth_gem)
            .help("A gem on the wrist gadget: its glow how visible you are (the light on you, crouching), a ring round it "
                  "how loud your last noise was."),
        toggle("? and ! Marks", vr_stealth_marks)
            .help("A ? rises over a monster turning Alert, a ! over one that spots you."),
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
        command("Through Slipgates", "map vrslipgates; wait60; god; setpos -1100 1408 24 0 180 0; wait5; noclip 0; vr_stealth_test 110")
            .help("vrslipgates (it loads it), you in room U: a grunt in room T sees you through T's north gate, walks "
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
        slider("Log", vr_stealth_debug, 0.f, 2.f, 1.f, "%.0f")
            .help("1: each monster's state changes (Alert, Hostile, Idle again); 2: also each meter step and noise heard."),
        slider("Force the Light on You", vr_stealth_light_force, -1.f, 160.f, 1.f, "%.0f")
            .help("Tests: the light on you as this (-1: measured)."),
    };
}
