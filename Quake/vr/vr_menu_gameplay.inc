// vr_menu_gameplay.inc -- VR Settings > Gameplay: enemies (shoves, melee, holding, knockdowns, weapons), damage, sound,
// blood feedback, parry, stamina, batting. Part of vr_menu.cpp (included where it was: the same translation unit, so
// its anonymous namespace).

[[nodiscard]] za::Vector<Item> pageGameplay()
{
    return {
        header("Monsters"),
        toggle("Enemies Hurt by Liquids", vr_enemy_liquid_damage).help("Monsters burn in slime/lava and drown after 12 seconds with their heads underwater, including knocked-down ragdolls. Fish, bosses and lava dwellers are immune."),
        slider("Liquid Breaks Falls", vr_liquid_fall_cushion, 0.f, 128.f, 8.f, "%.0f units").extend(0.f, 512.f)
            .help("A monster landing in water, slime or lava under this much of it (standing or knocked down) takes no fall "
                  "damage; shallower takes some. 0: liquid breaks no fall (vr_liquid_fall_cushion)."),
        toggle("Smooth Monster Steps", vr_monster_lerp_continue)
            .help("A monster's next step is drawn on from where it is drawn, not from where its last step ends: no jump "
                  "when it moves again before the last step is drawn out (a dog's quick turns looked like teleports). "
                  "Off: Quake's drawing."),
        toggle("Ogres Aim Grenades Up and Down", vr_ogre_aim_height)
            .help("Ogres (and zombies throwing flesh) lob at your height, on a ledge above them or a floor below, on an arc at "
                  "their throw's own speed. Off: Quake's lob, which always flies as if you stood level with them."),
        header("Enemy Weapons"),
        open("Enemy Weapons", pageIndex(pageEnemyWeapons))
            .help("What monsters drop as they die, to pick up and use: knights' swords, ogres' chainsaws, grunts' shotguns, "
                  "enforcers' laser rifles. Their damage and ammo."),
        header("Weapon Drops"),
        cycle("Enemy Weapon Drops", "vr_enemy_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random enemy weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Enemy Drops Chance", "vr_enemy_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for enemy weapon drops."),
        cycle("Ammo Box Weapon Drops", "vr_ammobox_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random ammo box weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Ammo Box Drops Chance", "vr_ammobox_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for ammo box weapon drops."),
        header("Feel"),
        toggle("Explosion Rumble", vr_explosion_rumble).help("Explosions near you rumble in your hands."),
        toggle("Low Health Heartbeat", vr_heartbeat).help("A heartbeat in your hands when your health is low."),
    };
}

// Grunts and enforcers shove you away when you stand too close (QC vr_enemyshove.qc; ROUND21.md, "Grunts and enforcers
// shove you").
[[nodiscard]] za::Vector<Item> pageEnemyShoves()
{
    return {
        toggle("Enemy Shoves", vr_enemy_shove)
            .help("Grunts and enforcers shove you away, pushing their gun out with both hands, when you stand too close "
                  "for a moment; vores swing their arms at you. Never while they fire, are staggered or knocked away."),
        slider("Too Close", vr_enemy_shove_range, 36.f, 96.f, 2.f, "%.0f units").extend(32.f, 160.f)
            .help("How close you must be (between your middles, flat: touching is 32 units) for them to shove you."),
        slider("Delay", vr_enemy_shove_delay, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 10.f)
            .help("How long you must stay that close before they shove (the shove's wind-up then takes 0.3 s)."),
        slider("Cooldown", vr_enemy_shove_cooldown, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 30.f)
            .help("From one shove to the same enemy's next, parried or not."),
        slider("Damage", vr_enemy_shove_damage, 0.f, 20.f, 1.f, "%.0f").extend(0.f, 50.f)
            .help("A shove's damage, cut by a parry as any blow's (Parry and Bash). 0: none, and it can't be parried."),
        slider("Push Distance", vr_enemy_shove_distance, 0.f, 160.f, 4.f, "%.0f units").extend(0.f, 400.f)
            .help("How far a shove pushes you (a quick slide; 32 units is about a metre). Not scaled by Knockback."),
        slider("Parry Push Reduction", vr_enemy_shove_parry_reduction, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of a shove's push a parry takes off (as Parry Damage Reduction takes off its damage): 0.75, a "
                  "parried shove pushes you a quarter as far. 1: not at all."),
        slider("Enforcer Damage", vr_enemy_shove_enforcer_damage, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("An enforcer's shove's damage, times Damage."),
        slider("Enforcer Push", vr_enemy_shove_enforcer_distance, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("How far an enforcer's shove pushes you, times Push Distance."),
        toggle("Vores Shove", vr_enemy_shove_vore)
            .help("Vores shove you too, swinging their arms down at you: the hardest shove of all. They reach a little "
                  "farther (they are wider)."),
        slider("Vore Damage", vr_enemy_shove_vore_damage, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A vore's shove's damage, times Damage."),
        slider("Vore Push", vr_enemy_shove_vore_distance, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("How far a vore's shove pushes you, times Push Distance."),
    };
}

// Enemy melee (QC fight.qc; ROUND22.md, "Melee arc", "Melee space", "Enemy Melee menu"): the monsters' melee blows at
// you: where they land (the arc ahead of the monster, its reach, both growing with its size and kind), how fast it turns
// after you while swinging, how close it comes.
[[nodiscard]] za::Vector<Item> pageEnemyMelee()
{
    return {
        toggle("Melee Arc", vr_enemy_melee_arc)
            .help("A monster's melee blow (a sword, a bite, a claw, a chainsaw) lands only if you are inside the arc ahead "
                  "of it when it strikes. Facing away, it turns to you before it attacks; while swinging it turns after "
                  "you only slowly (Swing Turn), so stepping aside or behind it dodges the swing. Leaps still hit by "
                  "touch. Off: Quake's blows, which check only reach (they hit you behind or beside it)."),
        toggle("Show Melee Arcs", vr_enemy_melee_arc_show)
            .help("Debug: each melee monster after you (within 600 units) draws on the floor its arc and reach as they "
                  "are now (a fan; its sweeping blows' wider fan once it has swung one), red with you inside it (its blow "
                  "would land), orange inside only a sweeping blow's, green outside. Its kind, arc and reach above it."),
        toggle("Show Aim Cones", vr_enemy_aim_show)
            .help("Debug: each ranged monster after you (within 1000 units) draws its aim cone ahead of it, red with you "
                  "inside (its shot comes straight at you), green outside, and its fire cone past it, orange with you "
                  "there (it fires at once, bent toward you only as far as its aim cone). Outside both it waits, turning. "
                  "Its kind and cones above it."),
        header("Arc"),
        slider("Arc Angle", vr_enemy_melee_arc_angle, 30.f, 120.f, 5.f, "%.0f deg")
            .help("How far to each side of its facing a grunt-sized monster's blow reaches (a knight, a death knight)."),
        slider("Wide Swings", vr_enemy_melee_arc_wide, 0.f, 60.f, 5.f, "+%.0f deg")
            .help("This much more to each side for sweeping blows (an ogre's chainsaw, a shambler's or minotaur's "
                  "claw)."),
        slider("Arc Growth with Size", vr_enemy_melee_arc_size, 0.f, 1.f, 0.05f, "%.2f")
            .help("A bigger monster's arc is wider: this share more Arc Angle for each grunt width more (a grunt is 24 "
                  "units wide, an ogre 40, a shambler 64: 0.1 gives an ogre 69 deg, a shambler 76). 0: the same for all."),
        slider("Reach Growth with Size", vr_enemy_melee_reach_size, 0.f, 1.f, 0.05f, "%.2f")
            .help("A bigger monster reaches further: this share more of its own reach (a knight's sword 60 units, an "
                  "ogre's chainsaw 100) for each grunt width more (0.1: an ogre's 107). 0: Quake's reach for all."),
        toggle("Kind Multipliers", vr_enemy_melee_kind)
            .help("Some kinds' own arcs on top of their size: a knight's quick sword thrust is narrower (0.85: 55 deg), so "
                  "it is easier to sidestep than an ogre's chainsaw. Off: size alone."),
        header("Swinging"),
        slider("Swing Turn", vr_enemy_melee_turn, 0.f, 30.f, 1.f, "%.0f deg/frame")
            .help("How fast a swinging monster turns after you (degrees each tenth of a second; slower monsters keep "
                  "their own speed). Lower: easier to sidestep. 0: its own speed."),
        slider("Melee Space", vr_enemy_melee_space, 0.f, 48.f, 2.f, "%.0f units")
            .help("A monster attacking you in melee (a knight's swing and its run past you, an ogre's chainsaw, a "
                  "fiend's claws) is kept this far from your head, pushed back smoothly, so it never fills your view and "
                  "you can parry. Its blows still reach you. 0: off (it charges in until your bodies meet)."),
        header("Ranged Aim"),
        toggle("Ranged Aim Cone", vr_enemy_aim_cone)
            .help("A monster's ranged shot (a grunt's, an enforcer's laser, an ogre's grenade, a scrag's spit) leaves along "
                  "its facing, bent toward you at most its aim cone, so circling or sidestepping a monster still turning "
                  "makes it miss. With you far outside its fire cone, it waits a moment, turning, before it fires. Up "
                  "and down it still aims at you. Homing shots and bosses are left alone. Off: Quake's shots, straight "
                  "at you whatever way it faces."),
        slider("Aim Cone", vr_enemy_aim_angle, 5.f, 60.f, 1.f, "%.0f deg")
            .help("How far to each side of its facing a shot may bend toward you (hitscan and fast shots: a grunt's, an "
                  "enforcer's; lobbed and slow ones times Lobbed and Slow)."),
        slider("Fire Cone", vr_enemy_aim_fire, 10.f, 120.f, 5.f, "%.0f deg")
            .help("You inside this cone ahead of it, it fires at once (bent at most its aim cone); outside it, it waits for "
                  "itself to turn, up to Max Delay. Times the kind's multiplier too."),
        slider("Max Delay", vr_enemy_aim_delay, 0.f, 1.f, 0.05f, "%.2f s")
            .help("How long a shot waits at most with you outside its fire cone, the monster turning after you meanwhile. "
                  "Then it fires anyway, bent toward you at most its aim cone. 0: no wait."),
        slider("Hitscan and Fast", vr_enemy_aim_narrow, 0.25f, 4.f, 0.05f, "x%.2f")
            .help("Both cones' multiplier for hitscan and fast shots (grunts, enforcers, gremlins, scourges, the "
                  "expansions' gunners)."),
        slider("Lobbed and Slow", vr_enemy_aim_wide, 0.25f, 4.f, 0.05f, "x%.2f")
            .help("Both cones' multiplier for lobbed and slow shots (an ogre's grenade, a zombie's flesh, a scrag's spit, "
                  "a death knight's magic): they are easier to see coming, so they may bend further."),
    };
}

// Struggling in the hands (vr_ragstruggle.cpp, vr_box3d_struggle.cpp; docs/vr-port/STRUGGLE.md): Combat > Holding
// Enemies > Struggle.
[[nodiscard]] za::Vector<Item> pageHoldingStruggle()
{
    return {
        toggle("Struggle", vr_struggle)
            .help("A knocked-down humanoid you hold (lifted, dragged, by its head, its legs or an arm) fights your hold, "
                  "blending its moves by how you hold it and where it is: held by the body it swats at you, held by the "
                  "head it pries at your hand, hung by the legs it bends up and reaches for you, dragged by the legs it "
                  "turns belly down and claws at the floor, dragged by an arm its legs stiffen. Off: a lifted one flails, "
                  "a dragged one lies limp, as before."),
        slider("Strength", vr_struggle_strength, 0.f, 2.f, 0.05f, "%.2f")
            .help("Times every struggling muscle."),
        slider("Speed", vr_struggle_speed, 0.3f, 2.f, 0.05f, "%.2f").help("Times the rate of its swings, tugs and kicks."),
        open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Struggle)>))
            .help("Which monster groups struggle (Combat > Monster Groups: each group's page has its Struggle Strength, "
                  "Speed, Arm Drive and Weaker When Hurt)."),
        header("Behaviours"),
        slider("Swat at You", vr_struggle_swat, 0.f, 2.f, 0.05f, "%.2f")
            .help("Held by the body or a shoulder: an arm winds up and swings at your head or the hand holding it, the "
                  "other reaching. 0: never."),
        slider("Pry at Your Hand", vr_struggle_pry, 0.f, 2.f, 0.05f, "%.2f")
            .help("Held by the head or an arm: its free hands go to the hand holding it and tug at it."),
        slider("Crunch Up", vr_struggle_crunch, 0.f, 2.f, 0.05f, "%.2f")
            .help("Lifted by the legs, hanging head down: it bends up at the waist and reaches for you."),
        slider("Claw the Floor", vr_struggle_claw, 0.f, 2.f, 0.05f, "%.2f")
            .help("Held or dragged by the legs on the floor: it turns belly down and stretches its arms ahead, clawing."),
        slider("Brace Its Legs", vr_struggle_brace, 0.f, 2.f, 0.05f, "%.2f")
            .help("Dragged on the floor by an arm or its head: its legs stiffen, heels dug in."),
        slider("Flail", vr_struggle_flail, 0.f, 2.f, 0.05f, "%.2f")
            .help("Its free legs' kicks and its body's and head's thrashing, under the moves above (more off the floor)."),
        slider("Slap at Your Hands", vr_struggle_frantic, 0.f, 2.f, 0.05f, "%.2f")
            .help("Held by the head, the legs or an arm: its free hands slap frantically at the hands holding it, drawn "
                  "back and struck in, erratic. 0: they only tug and grasp."),
        slider("Squirm", vr_struggle_squirm, 0.f, 2.f, 0.05f, "%.2f")
            .help("Held by the head or the legs: its body squirms, more thrashing over Flail."),
        slider("Look at You", vr_struggle_look, 0.f, 1.f, 0.05f, "%.2f")
            .help("Its head turns to look at you, as far as its neck goes: straight at you with its belly towards you or "
                  "held up before you, to the side you are on when turned away. Not while you hold its head. 0: never."),
        slider("Look Strength", vr_struggle_look_strength, 0.f, 0.9f, 0.05f, "%.2f")
            .help("Its neck's strength while it looks at you (times its effort: weaker as it tires or is hurt)."),
        slider("Stay Upright", vr_struggle_upright, 0.f, 2.f, 0.05f, "%.2f")
            .help("Dragged by an arm: it fights to stay up, its legs under it and its chest righted, rather than flopping "
                  "down. 0: it flops."),
        slider("Arm Reach", vr_struggle_arm_reach, 0.f, 175.f, 5.f, "%.0f deg")
            .help("Its shoulders' range while it struggles, for its hands to come over its head (at your hand on its "
                  "head). 0: its body's own (no higher than its shoulders)."),
        slider("Voice", vr_struggle_voice, 0.f, 2.f, 0.05f, "%.2f")
            .help("Its own pain, grunt and sight sounds while it struggles, quieter as it weakens. 0: silent."),
        slider("Voice Gap", vr_struggle_voice_gap, 0.3f, 6.f, 0.1f, "%.1f s")
            .help("About this between its cries at full effort; longer as it tires."),
        header("Character"),
        slider("Variation", vr_struggle_variation, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much each enemy's rhythm, favourite arm and aim differ from another's. 0: all alike."),
        slider("Bursts", vr_struggle_bursts, 0.f, 1.f, 0.05f, "%.2f")
            .help("Struggling in fits: bursts of effort, quicker and harder, then a lull. 0: steady."),
        slider("Tires After", vr_struggle_fatigue, 0.f, 60.f, 1.f, "%.0f s")
            .help("Seconds of struggling until it is worn out. 0: it never tires."),
        slider("Effort When Tired", vr_struggle_tired, 0.f, 1.f, 0.05f, "%.2f")
            .help("Its effort left once worn out."),
        slider("Blend Time", vr_struggle_blend, 0.05f, 1.f, 0.05f, "%.2f s")
            .help("How quickly it changes from one struggle to the next as your hold changes (lifted, put down, dragged)."),
        header("The Hold"),
        slider("Held Limb Keeps", vr_struggle_held_share, 0.f, 1.f, 0.05f, "%.2f")
            .help("The joints above the part you hold in its limb (the hip of a shin you hold) keep this share of their "
                  "strength: it bends at the hip against you. 0: the held limb hangs slack."),
        slider("Grip Guard", vr_struggle_grip_guard, 0.f, 20.f, 0.5f, "%.1f units")
            .help("The spot you hold this far from your hand (the hold lagging) eases its struggle, to a quarter at 2.5 "
                  "times it: it never tears out of your hand by struggling. 0: no guard."),
        toggle("Show Struggle", vr_struggle_debug)
            .help("A panel over each struggling enemy: its behaviours' weights, its effort and fatigue, what you hold, "
                  "how much lies on the floor, its drag, which way up it is, the hold's lag. Also Debug > Tests > "
                  "Holding Enemies."),
    };
}

// Holding enemies (vr_foegrab.cpp, QC vr_foegrab.qc; ROUND21.md, "Holding enemies"): an empty hand holds on to a living
// monster, slows it and drags it a little.
[[nodiscard]] za::Vector<Item> pageHoldingEnemies()
{
    return {
        toggle("Holding Enemies", vr_foegrab)
            .help("Experimental. Grip with an empty hand touching a living enemy to hold on to that spot of it: the hand "
                  "stays on it as it moves, and the enemy is slowed and can be pulled a little. Small ones are held well, "
                  "big ones (fiends, ogres) a little, huge ones (shamblers, vores) hardly. Let go of the grip, or pull too "
                  "far, and the hand lets go. Grabbing a sleeping or idle enemy wakes it. Needs Precise Hit Detection."),
        slider("Touch Leniency", vr_foegrab_leniency, 0.f, 5.f, 0.25f, "%.2f cm").extend(0.f, 20.f)
            .help("How far off the enemy's model your fist may be and still take hold."),
        slider("Let Go Beyond", vr_foegrab_break, 10.f, 80.f, 1.f, "%.0f cm").extend(5.f, 200.f)
            .help("Your hand this far from the spot it holds (the enemy ran, or you moved away) lets go: after a quarter "
                  "of a second there, or at once at twice this."),
        header("Hold"),
        cycle("Hold By", "vr_foegrab_by_mass", {{0.f, "Size Class"}, {1.f, "Mass"}})
            .help("Size Class: the three holds below, by kind of enemy. Mass: from the enemy's mass, full at Light and "
                  "none at Heavy."),
        slider("Small Enemies", vr_foegrab_strength_small, 0.f, 1.f, 0.05f, "%.2f")
            .help("One hand's hold on a grunt, an enforcer, a zombie, a knight, a mummy, a dog. Two hands together: "
                  "1 - (1 - a)(1 - b) (0.85 each: 0.98)."),
        slider("Big Enemies", vr_foegrab_strength_medium, 0.f, 1.f, 0.05f, "%.2f")
            .help("One hand's hold on a fiend, an ogre, a death knight's bigger kin."),
        slider("Huge Enemies", vr_foegrab_strength_large, 0.f, 1.f, 0.01f, "%.2f")
            .help("One hand's hold on a shambler, a vore and anything bigger."),
        slider("Light (Mass)", vr_foegrab_mass_light, 10.f, 400.f, 10.f, "%.0f kg").extend(1.f, 5000.f)
            .help("Hold By Mass: enemies this light or lighter are held fully (a grunt 80 kg, an enforcer 100)."),
        slider("Heavy (Mass)", vr_foegrab_mass_heavy, 100.f, 2000.f, 25.f, "%.0f kg").extend(10.f, 20000.f)
            .help("Hold By Mass: enemies this heavy or heavier are not held at all (a fiend or an ogre 250, a shambler 600)."),
        header("Effect"),
        slider("Slow Down", vr_foegrab_slow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of a fully held enemy's own movement (and turning) taken off, times its hold."),
        slider("Follow the Hand", vr_foegrab_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of your holding hand's own movement the enemy follows, times its hold: pull your hand back, or "
                  "walk off holding it, and a grunt comes along, an ogre a little, a shambler hardly."),
        slider("Pull", vr_foegrab_drag, 0.f, 20.f, 0.5f, "%.1f /s")
            .help("How quickly your hand pulls the spot it holds towards it, times the hold (0: not at all): takes up "
                  "what Follow the Hand leaves."),
        slider("Pull Speed", vr_foegrab_drag_speed, 0.f, 300.f, 5.f, "%.0f units/s")
            .help("The pull's top speed (32 units are about a metre)."),
        cycle("One Hand Drags By", "vr_foegrab_drag_onehand",
            {{1.f, "Torso, Shoulders"}, {2.f, "Torso, Shoulders, Head"}, {0.f, "Anywhere"}})
            .help("Where one hand alone (the other off it) drags a standing enemy along by Follow the Hand and Pull. "
                  "Torso, Shoulders: a hand on its legs only pulls its legs out (Leg Grabs, One-Hand Pull), on its head "
                  "only holds it, on a forearm or hand neither. Anywhere: as before."),
        header("Their Shoves"),
        cycle("Held Enemy's Shove", "vr_foegrab_shove", {{0.f, "Resisted"}, {1.f, "Breaks Free"}})
            .help("Resisted: a grunt or enforcer you hold barely pushes you (by Shove Resistance and its hold), and comes "
                  "along with you. Breaks Free: its shove pushes you fully, as ever, and tears it from your hands."),
        slider("Shove Resistance", vr_foegrab_shove_resist, 0.f, 1.f, 0.05f, "%.2f")
            .help("Resisted: share of a held enemy's shove taken off, times its hold."),
        header("Getting Close"),
        slider("Body Inset", vr_body_monster_inset, 0.f, 12.f, 1.f, "%.0f units")
            .help("Your body and a monster's meet this much further in on each side (both bodies: twice this closer face "
                  "to face), so your hands reach its neck and head. Only between you and monsters: walls, doors, "
                  "monsters against each other, shots and melee keep their boxes. 0: as before."),
        toggle("Step In", vr_foegrab_step_in)
            .help("While a hand holds an enemy, your body and its body don't meet: step in and bring the other hand to "
                  "its head. Let go while overlapping and the two are pushed gently apart (never stuck)."),
        slider("Step In Depth", vr_foegrab_step_in_depth, 0.f, 32.f, 1.f, "%.0f units")
            .help("With Step In, how far your body goes into the held enemy past where you would meet; there it stops "
                  "you as a solid body (you slide along it), so you can't walk through it. Stepping back or aside is "
                  "always free. 0: no limit (you can walk right through it)."),
        header("Two-Hand Throw"),
        toggle("Two-Hand Throw", vr_foegrab_throw)
            .help("Hold one enemy with both hands and turn it over hard, a judo throw: one shoulder down and the other up, "
                  "or its top pulled towards you or to a side. It is knocked down that way (it falls, lies, gets up), for "
                  "sure: grunts, enforcers, zombies, knights, rottweilers, mummies and the infected always; death knights, "
                  "ogres, fiends, spawns, scorpions and ranged knights only when hurt; never shamblers, vores, bosses, "
                  "flyers or swimmers. One hand never throws. The training dummy shows how it would go."),
        toggle("Show Diagnostic", vr_foegrab_throw_debug)
            .help("Over the enemy you hold (else the nearest): your hands' turn of it (an arrow from its top: the way it "
                  "would fall) and their speed against Turn to Throw and Hands' Speed, what stops a throw now (one hand, "
                  "both at its head: the neck snap's; pulling up: the lift's; away from you; its kind or health) and the "
                  "last try's result. Each try is printed to the console."),
        slider("Turn to Throw", vr_foegrab_throw_twist, 60.f, 600.f, 10.f, "%.0f deg/s").extend(10.f, 2000.f)
            .help("How fast your hands must turn the enemy over (about a level axis) to throw it. Log Holds (Debug > "
                  "Tests > Holding Enemies) prints your hardest turn of each hold."),
        slider("Hands' Speed", vr_foegrab_throw_speed, 0.2f, 3.f, 0.1f, "%.1f m/s").extend(0.f, 10.f)
            .help("And how fast your hands must move then (their mean): a slow turn never throws."),
        slider("Hurt Below", vr_foegrab_throw_hurt, 0.05f, 1.f, 0.05f, "%.2f")
            .help("The bigger enemies (death knights, ogres, fiends...) are thrown only below this share of their full "
                  "health, and never above it."),
        toggle("Throw Away From You", vr_foegrab_throw_away)
            .help("Off: a turn that would throw the enemy straight away from you does nothing (that's a shove's job). On: "
                  "it throws too."),
        cycle("Who Can Be Thrown", "vr_foegrab_throw_by_mass", {{0.f, "By Group"}, {1.f, "By Mass"}})
            .help("By Group: each monster group's Two-Hand Throw (Combat > Monster Groups: always, only when hurt, or "
                  "never). By Mass: the two masses below."),
        open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Throw)>))
            .help("Which groups are always thrown, only when hurt, or never (Combat > Monster Groups)."),
        slider("Always (Mass)", vr_foegrab_throw_mass_always, 10.f, 1000.f, 10.f, "%.0f kg").extend(0.f, 10000.f)
            .help("By Mass: enemies this light or lighter are always thrown (a grunt 80 kg, a mummy 140)."),
        slider("When Hurt (Mass)", vr_foegrab_throw_mass_hurt, 10.f, 2000.f, 10.f, "%.0f kg").extend(0.f, 20000.f)
            .help("By Mass: enemies this light or lighter, and heavier than Always, only when hurt; heavier never (an "
                  "ogre or a fiend 250 kg, a shambler 600). Vores and overlords never."),
        slider("Throw Push", vr_foegrab_throw_push, 0.f, 600.f, 10.f, "%.0f units/s")
            .help("How hard the thrown enemy is sent along the throw (times Knockdowns' Push)."),
        slider("Throw Lift", vr_foegrab_throw_lift, 0.f, 400.f, 10.f, "%.0f units/s")
            .help("And up."),
        slider("Topple", vr_foegrab_throw_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("The thrown enemy loses its footing: it is turned over about its feet towards the throw this fast, a "
                  "sweep, and the push and lift go to its top, not its feet. Off: pushed whole, as a shove's knockdown."),
        slider("Feet Speed", vr_foegrab_throw_feet_speed, 0.f, 300.f, 10.f, "%.0f units/s").extend(0.f, 1000.f)
            .help("Its feet are swept the other way as it topples (thrown left, they go right), so it spins in place in "
                  "the air rather than falling over its feet: this fast. 0: its feet stay where they stood a moment."),
        slider("Twist Spin", vr_foegrab_throw_spin, 0.f, 2.f, 0.05f, "%.2f")
            .help("Share of your hands' twist of it about the vertical it spins on with as it falls (at most 720 deg/s)."),
        toggle("Hold On After Throw", vr_foegrab_throw_hold)
            .help("On: the thrown enemy stays in your hands as a ragdoll, held softly: it still topples and falls the way "
                  "you threw it, and you can steer it, drag it along, pull it about; let go of the grips to drop it or "
                  "throw it. Off: both hands let go as it is thrown."),
        slider("Held On Strength", vr_foegrab_throw_hold_strength, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 5.f)
            .help("Hold On After Throw: the most each hand pulls the thrown body with, as a share of its weight. Below "
                  "0.5 two hands drag it (lower your hands to it) but never hold it up; above, it hangs from your hands."),
        slider("Held On Spring", vr_foegrab_throw_hold_hertz, 1.f, 12.f, 0.5f, "%.1f Hz").extend(0.5f, 30.f)
            .help("Hold On After Throw: how quickly the held limb follows your hand (higher: stiffer)."),
        slider("Passes Through You", vr_foegrab_throw_grace, 0.f, 2.f, 0.05f, "%.2f s").extend(0.f, 5.f)
            .help("The thrown enemy passes through your hands, and you, this long as it falls: your hands, still in it and "
                  "moving on, or you standing where you pull it, no longer knock it back over the other way. 0: they do."),
        slider("Throw Grunt", vr_foegrab_throw_grunt, 0.f, 1.f, 0.05f, "%.2f")
            .help("Your grunt as you throw an enemy down, this loud (0 off)."),
        cycle("Throw Grunt Sound", vr_foegrab_throw_grunt_sound, gruntChoices())
            .help("Which grunt the throw makes: the same choices as climbing's Mantle Grunt Sound. The deeper ones are "
                  "Quake's played slower. Hear it with Hear Throw Grunt."),
        command("Hear Throw Grunt", "vr_foegrab_throw_grunt_test").help("Plays the throw's grunt as set, at full volume."),
        open("Struggle", pageIndex(pageHoldingStruggle))
            .help("How a knocked-down humanoid you hold fights your hold: its moves, their strength, its fits and fatigue."),
        header("Lift"),
        toggle("Lift", vr_foegrab_lift)
            .help("Walk up close to a humanoid (a grunt, an enforcer, a zombie, a knight, a death knight, Arcane Dimensions' "
                  "soldiers and knights), hold it with both hands and pull up instead of to a side: it comes off its feet, a "
                  "ragdoll flailing in your hands. Carry it, throw it down (let go of the grips as you swing). After a while "
                  "it breaks free and shoves you back. Never ogres, fiends, shamblers or vores (each monster group's "
                  "Lift: Combat > Monster Groups)."),
        toggle("Show Diagnostic", vr_foegrab_lift_debug)
            .help("Over the humanoid you hold (else the nearest): your hands' pull (an arrow: green once fast enough and "
                  "within 45 degrees of straight up) against Lift Speed, the Lift Reach circle round it (green with you "
                  "inside), what stops a lift now (one hand, both at its head: the neck snap's; not a humanoid; too far) "
                  "and the last try's result. Each try is printed to the console."),
        slider("Lift Speed", vr_foegrab_lift_speed, 0.3f, 3.f, 0.1f, "%.1f m/s").extend(0.1f, 10.f)
            .help("How fast your hands must pull up together (within 45 degrees of straight up) to lift it."),
        slider("Lift Reach", vr_foegrab_lift_reach, 24.f, 96.f, 2.f, "%.0f units").extend(0.f, 200.f)
            .help("And how close you must stand to it (your middle to its; held at arm's length a grunt is about 36 units "
                  "off; 32 units are about a metre)."),
        slider("Breaks Free After", vr_foegrab_lift_time, 0.5f, 10.f, 0.25f, "%.2f s").extend(0.1f, 60.f)
            .help("Held up this long, it breaks free: your hands lose it, it shoves you back and gets up."),
        slider("Lift Strength", vr_foegrab_lift_strength, 0.f, 2.f, 0.05f, "%.2f").extend(0.f, 10.f)
            .help("The most each hand holds the lifted body with, as a share of its weight: above 0.5 two hands carry it "
                  "(the more, the less it sags); below, it sinks to the floor, still held."),
        slider("Lift Launch", vr_foegrab_lift_up, 0.f, 400.f, 10.f, "%.0f units/s").extend(0.f, 1000.f)
            .help("How fast it leaves the floor as you lift it."),
        slider("Break-Free Push", vr_foegrab_free_push, 0.f, 0.5f, 0.01f, "%.2f m").extend(0.f, 3.f)
            .help("How far it pushes you back, smoothly, as it breaks free (0: not at all; a shove is further)."),
        toggle("Break-Free Sound", vr_foegrab_free_sound)
            .help("The shove sound plays as it breaks free."),
        slider("Grab While Staggered", vr_foegrab_manhandle_stagger, 0.f, 5.f, 0.25f, "%.2f s").extend(0.f, 30.f)
            .help("A humanoid staggered this recently (a shove, a hit or throw that unbalanced it, a stagger it just "
                  "recovered from) goes down when one hand grabs it: a ragdoll in your hand, as a lifted one (it flails and "
                  "breaks free). One staggering as a ragdoll goes down when a hand takes it. 0: never."),
        slider("Grab When Weak", vr_foegrab_manhandle_health, 0.f, 1.f, 0.05f, "%.2f")
            .help("A humanoid at this share of its full health or less (0.25: a quarter) goes down when one hand grabs it, "
                  "as above. 0: never."),
        header("Leg Grabs"),
        toggle("Leg Grabs", vr_foegrab_leggrab)
            .help("Grab a humanoid's legs with both hands below the knees (one leg with both hands, or one leg each). Pull "
                  "them towards you: its feet slide to you and it topples over away from you. Pull them up: it is lifted "
                  "by the legs, head down, flailing, as a lift (carry it, throw it, it breaks free after Breaks Free "
                  "After). Heavier humanoids need a harder pull."),
        toggle("Show Diagnostic", vr_foegrab_leggrab_debug)
            .help("Over the humanoid you hold (else the nearest): where each hand holds it (green below the knee, orange a "
                  "thigh, red off its legs), your pull towards you and up against Pull Speed and Leg Lift Speed (arrows "
                  "and bars), the pull's stroke against Pull Distance, its weight's factor, what stops it now and the "
                  "last try. Each try is printed to the console."),
        slider("Thigh Grip", vr_foegrab_leggrab_thigh, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much a hand on a thigh (above the knee) counts: 0 it isn't a leg grab; 0.5 it is, but you must "
                  "pull twice as fast."),
        slider("Pull Speed", vr_foegrab_legpull_speed, 0.3f, 4.f, 0.1f, "%.1f m/s").extend(0.1f, 10.f)
            .help("How fast your hands must pull its legs towards you to pull them out from under it."),
        slider("Pull Distance", vr_foegrab_legpull_distance, 0.f, 40.f, 1.f, "%.0f cm").extend(0.f, 100.f)
            .help("And how far they must come towards you in that pull (a twitch doesn't count)."),
        slider("One-Hand Pull", vr_foegrab_legpull_onehand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("One hand alone on its leg (the other free, hanging on a ledge or holding something) pulls it out too, "
                  "at Pull Speed times this: a harder pull than with both hands. 0: both hands only."),
        slider("Pull Topple Speed", vr_foegrab_legpull_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("Pulled out, how fast its body turns over away from you about its feet."),
        slider("Pull Feet Slide", vr_foegrab_legpull_feet, 0.f, 200.f, 5.f, "%.0f units/s").extend(0.f, 500.f)
            .help("How fast its feet slide towards you as it topples."),
        slider("Pull Hold Strength", vr_foegrab_legpull_hold_strength, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("Pulled down, how hard your hands still holding its legs pull them: a share of its weight. Softer than a "
                  "throw's hold, so it topples the way you pulled instead of pivoting on its pinned shins."),
        slider("Pull Drag Strength", vr_foegrab_legpull_drag_strength, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("Once it lies after the pull, how hard your hands still holding its legs drag it in all (one hand or "
                  "both): a share of its weight. The soft hold above firms up to this so you can walk back and drag it by "
                  "the legs; too weak and it lies still, its weight on the floor, until the hold lets go."),
        slider("Leg Lift Speed", vr_foegrab_leglift_speed, 0.3f, 3.f, 0.1f, "%.1f m/s").extend(0.1f, 10.f)
            .help("How fast your hands must pull its legs up together (within 45 degrees of straight up) to lift it by "
                  "them (standing within Lift Reach)."),
        slider("Leg Lift Swing", vr_foegrab_leglift_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("Lifted by the legs, how fast its body swings over head down, away from you."),
        slider("Heavy Mass", vr_foegrab_leggrab_heavy, 0.f, 300.f, 10.f, "%.0f kg").extend(0.f, 2000.f)
            .help("A humanoid heavier than this needs a faster pull and lift: times the cube root of its mass over this "
                  "(a grunt is 80 kg, an enforcer 100, a death knight 150). 0: all alike."),
        header("Neck Snap"),
        toggle("Neck Snap", vr_foegrab_necksnap)
            .help("Hold a humanoid's head or neck with both hands and twist it round: push one hand forward and pull "
                  "the other back, or turn both wrists the same way, or both. Its neck snaps with a crack and it dies on "
                  "the spot, limp in your hands until you let go of the grips. Standing, staggering, knocked down or "
                  "lifted, as long as it lives. With both hands on its head the neck snap is the only move: no judo "
                  "throw, no lift. The same humanoids as the lift: never ogres, fiends, shamblers or vores."),
        toggle("Show Diagnostic", vr_foegrab_necksnap_debug)
            .help("Over the humanoid you hold (else the nearest): its head zone (green once both hands are in it), where "
                  "each hand holds (green at its head or neck, orange at a shoulder: never counts), the line between "
                  "your hands, the twist's arc against Twist Angle and a panel of the hand line's numbers against it; "
                  "the last try's result. Each try's largest turn is printed to the console."),
        slider("Twist Angle", vr_foegrab_necksnap_angle, 10.f, 120.f, 5.f, "%.0f deg").extend(1.f, 360.f)
            .help("How far round your hands must turn its head, one way, within Twist Window: the line between your "
                  "hands, across its neck and relative to its head, against where it pointed then (hands 30 cm apart, one "
                  "15 cm forward and the other 15 cm back: 45 deg), or both wrists rolling the same way. Slow shuffles "
                  "never add up; tilting doesn't count."),
        slider("Twist Window", vr_foegrab_necksnap_window, 0.1f, 1.f, 0.05f, "%.2f s")
            .help("The head must turn Twist Angle within this long (0.15 s and 75 deg: 500 deg/s on average)."),
        slider("Head Grip Margin", vr_foegrab_necksnap_margin, 0.f, 12.f, 1.f, "%.0f units").extend(-8.f, 32.f)
            .help("Your hands count as on its head this far outside its head zone (the headshots'; 32 units are about a "
                  "metre)."),
        slider("Neck Length", vr_foegrab_necksnap_neck, 0.f, 12.f, 0.5f, "%.1f units").extend(0.f, 32.f)
            .help("How far below its head your hands still hold its neck (as good as its head: only the neck snap). "
                  "Lower, or out at its shoulders, both hands throw it (judo) or lift it: a shoulder never counts."),
        slider("Crack Volume", vr_foegrab_necksnap_volume, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 2.f)
            .help("The bone-cracking sound's volume (0 silent): from its head and in your hands. Above 1 the layers in "
                  "your hands grow louder still."),
        slider("Snapped Body Hold", vr_foegrab_necksnap_strength, 0.f, 2.f, 0.05f, "%.2f").extend(0.f, 10.f)
            .help("The most each hand holds the limp body with afterwards, as a share of its weight: above 0.5 two hands "
                  "carry it; below, it sinks to the floor, still held."),
        slider("Snapped Body Spring", vr_foegrab_necksnap_hertz, 4.f, 30.f, 1.f, "%.0f Hz").extend(0.5f, 30.f)
            .help("How stiffly its head follows your hands afterwards: the light head bears the whole body's weight, so "
                  "the softer, the lower it hangs below your hands."),
        toggle("Broken Neck Flops", vr_foegrab_necksnap_loose)
            .help("A snapped neck's joint goes loose: its head flops and rolls as if the vertebrae broke. Off: it stays "
                  "as stiff as any body's."),
        slider("Broken Neck Swing", vr_foegrab_necksnap_cone, 30.f, 170.f, 5.f, "%.0f deg")
            .help("How far a snapped neck lets the head swing off its line (a living neck's: about 45; past about 120 "
                  "the head sinks into the chest)."),
        slider("Broken Neck Twist", vr_foegrab_necksnap_twist, 30.f, 175.f, 5.f, "%.0f deg")
            .help("How far a snapped neck lets the head turn either way about it."),
        slider("Broken Neck Friction", vr_foegrab_necksnap_friction, 0.f, 1.f, 0.05f, "%.2f")
            .help("What holds a snapped neck from flopping freely, as a share of the body's joint friction (0: none)."),
        slider("Tough Neck Damage", vr_foegrab_necksnap_tough, 0.f, 1.f, 0.05f, "%.2f")
            .help("A monster whose group's Neck Snap Kills is off (Monster Groups: the Mini Bosses) survives the twist: it "
                  "takes this share of its full health as damage (it dies of it only when hurt that much) and fights on."),
    };
}

// Ragdoll Triggers (QC vr_kdtrig.qc; ROUND22.md, "The flinch removed; ragdoll triggers"): Combat > Ragdoll Triggers, a page per
// trigger and Monster Resistance; the shove's, the leg sweep's, the leg pull's and the lift's on their own pages.
[[nodiscard]] za::Vector<Item> pageKdTrigLedge()
{
    return {
        toggle("Ledges", vr_kdtrig_ledge_on)
            .help("Struck with a drop of Least Drop or more right behind it (along the hit, room for its body to drop), it is pushed off. Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_ledge, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down (times how strong the hit is: up to half again; divided by its Monster Resistance)."),
        slider("Least Damage", vr_kdtrig_ledge_min, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("Weaker hits never trigger it (times its Monster Resistance)."),
        slider("Push", vr_kdtrig_ledge_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over as it goes down (a two-hand shove: 1)."),
        slider("Least Drop", vr_kdtrig_ledge_drop, 24.f, 256.f, 8.f, "%.0f units")
            .help("A drop at least this deep is a ledge (a monster is about 56 tall, a stair 16-18)."),
        slider("Look Behind", vr_kdtrig_look, 8.f, 128.f, 4.f, "%.0f units")
            .help("How far past its body the ground behind it is looked at for stairs and ledges."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigStairs()
{
    return {
        toggle("Stairs", vr_kdtrig_stairs_on)
            .help("Struck at the top of steps going down behind it (two or more, the first right behind its feet), it falls down them. Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_stairs, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down (times how strong the hit is: up to half again; divided by its Monster Resistance)."),
        slider("Least Damage", vr_kdtrig_stairs_min, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("Weaker hits never trigger it (times its Monster Resistance)."),
        slider("Push", vr_kdtrig_stairs_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over as it goes down (a two-hand shove: 1)."),
        slider("Look Behind", vr_kdtrig_look, 8.f, 128.f, 4.f, "%.0f units")
            .help("How far past its body the ground behind it is looked at for stairs and ledges."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigSlope()
{
    return {
        toggle("Slopes", vr_kdtrig_slope_on)
            .help("Standing on a steep slope, hit down it, it topples down the slope. Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_slope, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down (times how strong the hit is: up to half again; divided by its Monster Resistance)."),
        slider("Least Damage", vr_kdtrig_slope_min, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("Weaker hits never trigger it (times its Monster Resistance)."),
        slider("Push", vr_kdtrig_slope_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over as it goes down (a two-hand shove: 1)."),
        slider("Least Steepness", vr_kdtrig_slope_angle, 5.f, 45.f, 1.f, "%.0f deg")
            .help("Slopes at least this steep."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigShotgun()
{
    return {
        toggle("Shotgun Range", vr_kdtrig_shotgun_on)
            .help("A shotgun or super shotgun blast of yours from close by knocks it over. Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_shotgun, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down (times how strong the hit is: up to half again; divided by its Monster Resistance)."),
        slider("Push", vr_kdtrig_shotgun_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over as it goes down (a two-hand shove: 1)."),
        slider("Range", vr_kdtrig_shotgun_range, 32.f, 512.f, 16.f, "%.0f units")
            .help("Shotgun and super shotgun blasts this close; less likely towards it."),
        slider("Full Pellets", vr_kdtrig_shotgun_pellets, 2.f, 20.f, 1.f, "%.0f")
            .help("Pellets landing for the full chance (fewer: less, under half of them none; a shotgun has 6, a super "
                  "shotgun 14)."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigWounded()
{
    return {
        toggle("Wounded", vr_kdtrig_wounded_on)
            .help("Badly hurt, any hit may knock it down. Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_wounded, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down (times how strong the hit is: up to half again; divided by its Monster Resistance)."),
        slider("Least Damage", vr_kdtrig_wounded_min, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("Weaker hits never trigger it (times its Monster Resistance)."),
        slider("Push", vr_kdtrig_wounded_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over as it goes down (a two-hand shove: 1)."),
        slider("Wounded Below", vr_kdtrig_wounded_health, 0.f, 1.f, 0.05f, "%.2f")
            .help("Its health under this share of its full health: half the chance there, all of it near death."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigBlow()
{
    return {
        toggle("Strong Blows", vr_kdtrig_blow_on)
            .help("A strong melee blow or a prop you fling knocks it over (up to twice the chance at twice Least Damage: a hard punch is about 30). Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_blow, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down (times how strong the hit is: up to half again; divided by its Monster Resistance)."),
        slider("Least Damage", vr_kdtrig_blow_min, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("Weaker hits never trigger it (times its Monster Resistance)."),
        slider("Push", vr_kdtrig_blow_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over as it goes down (a two-hand shove: 1)."),
        slider("Blades' Share", vr_kdtrig_blow_blade, 0.f, 1.f, 0.05f, "%.2f")
            .help("An axe's, a sword's or the chainsaw's edge: the chance times this. Blunt blows (fists, a gun's butt, a "
                  "hammer, a mace, a pommel, the crowbar, props) count fully."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigBlast()
{
    return {
        toggle("Explosions", vr_kdtrig_blast_on)
            .help("A nearby explosion (a rocket, a grenade, a barrel, any blast that leaves it alive) throws it down away from the blast's centre (Least Damage: the damage it took, already less with distance). Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_blast, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down (times how strong the hit is: up to half again; divided by its Monster Resistance)."),
        slider("Least Damage", vr_kdtrig_blast_min, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("Weaker hits never trigger it (times its Monster Resistance)."),
        slider("Push", vr_kdtrig_blast_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over as it goes down (a two-hand shove: 1)."),
        slider("Range", vr_kdtrig_blast_range, 32.f, 512.f, 16.f, "%.0f units")
            .help("None farther from the blast's centre; the chance falls to half towards it."),
    };
}

// Thrown Bodies' and Tackles' weight classes (both pages).
void kdTrigWeightRows(za::Vector<Item>& items)
{
    items.pushBack(header("Weight Classes"));
    items.pushBack(slider("Monster Weight", vr_kdtrig_weight, 20.f, 200.f, 5.f, "%.0f kg")
                       .help("A monster's weight: this times its group's Trigger Resistance (a grunt 1, an ogre 2.5: 80 and "
                             "200 kg)."));
    items.pushBack(slider("Topple Class", vr_kdtrig_weight_topple, 0.5f, 4.f, 0.05f, "%.2fx")
                       .help("A monster up to this times the thrower's weight (the thrown body's mass, or yours) can be "
                             "toppled: a grunt thrown at a grunt. Heavier ones only stagger."));
    items.pushBack(slider("Stagger Class", vr_kdtrig_weight_stagger, 0.5f, 8.f, 0.1f, "%.1fx")
                       .help("Up to this times the thrower's weight it only staggers (an ogre); heavier ones stand. "
                             "Shamblers and bosses never (their groups are off)."));
    items.pushBack(slider("Stagger Time", vr_kdtrig_bump_stagger, 0.1f, 2.f, 0.05f, "%.2f s")
                       .help("A heavier one's short stagger, as a parry's (as a ragdoll where it can)."));
}

[[nodiscard]] za::Vector<Item> pageKdTrigThrown()
{
    za::Vector<Item> items{
        toggle("Thrown Bodies", vr_kdtrig_thrown_on)
            .help("A body you throw (a corpse or a knocked-down enemy: let go of, or a two-hand throw) or swing in your "
                  "hand into a living monster knocks it over the way the body goes, by their weights and the body's "
                  "momentum. Needs Ragdoll Triggers and Knockdowns on."),
        slider("Chance", vr_kdtrig_thrown, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it goes down when the body is heavy and fast enough."),
        slider("Push", vr_kdtrig_thrown_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over (a two-hand shove: 1; more with the body's momentum)."),
        slider("Speed", vr_kdtrig_thrown_speed, 50.f, 600.f, 10.f, "%.0f u/s")
            .help("A body of the monster's own weight this fast topples it (its momentum over the monster's: a lighter or "
                  "slower body less). A quarter of it the least that counts."),
        slider("Least Strike", vr_kdtrig_thrown_least, 0.f, 1.f, 0.05f, "%.2f")
            .help("A strike under this share of a toppling one does nothing; from it up to a toppling one the chance to "
                  "topple grows to its full (else it staggers back, farther the harder it was struck)."),
        slider("Thrown For", vr_kdtrig_thrown_window, 0.25f, 5.f, 0.25f, "%.2f s")
            .help("How long after it leaves your hand a body counts as thrown (held, while your hand swings it)."),
    };
    kdTrigWeightRows(items);
    items.pushBack(open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Thrown)>))
                       .help("Thrown Bodies' chance times each group's share."));
    return items;
}

[[nodiscard]] za::Vector<Item> pageKdTrigTackle()
{
    za::Vector<Item> items{
        toggle("Shoulder Charges", vr_kdtrig_tackle_on)
            .help("Running into a monster with a shoulder leading, your head tucked down, both hands low and close to "
                  "your body (the leading arm tucked) knocks it back: a light one flies back and falls, an ogre staggers "
                  "back, shamblers and bosses stand. You bounce back off it. Needs Ragdoll Triggers and Knockdowns on. "
                  "(Tuned on recorded takes: Motion Recorder > Shoulder Charge.)"),
        slider("Charge Leniency", vr_kdtrig_tackle_leniency, 0.f, 1.f, 0.05f, "%.2f")
            .help("All of the charge's thresholds below together: 0.5 uses them as set; higher is easier (1: the least "
                  "ones halved, the most ones and the speed's shortfall from full doubled), lower stricter (0: 1.5 "
                  "times)."),
        slider("Chance", vr_kdtrig_tackle, 0.f, 1.f, 0.05f, "%.2f").help("The chance a light one falls."),
        slider("Push", vr_kdtrig_tackle_push, 0.25f, 2.f, 0.05f, "%.2fx")
            .help("How hard it is pushed over and back (a two-hand shove: 1; the knock times this)."),
        slider("Knock Back", vr_kdtrig_tackle_knock, 0.f, 200.f, 4.f, "%.0f units")
            .help("How far one of your weight is knocked back before it falls (a lighter one up to 1.5 times; a "
                  "heavier one staggers back 0.6 of it)."),
        slider("Least Speed", vr_kdtrig_tackle_speed, 0.3f, 1.f, 0.05f, "%.2f")
            .help("Your ground speed for it, as a share of the most (walking is about 0.6)."),
        slider("Run-Up", vr_kdtrig_tackle_runup, 0.f, 4.f, 0.1f, "%.1f s")
            .help("How long you must have been running at that speed before it counts (a sprint begun next to the "
                  "monster doesn't; 0: none)."),
        slider("Standing Tall", vr_kdtrig_tackle_stand, 0.f, 1.f, 0.05f, "%.2f")
            .help("Your head at least this share of your standing height (0.8: not crouched or half crouched; 0: any)."),
        slider("Your Weight", vr_kdtrig_tackle_weight, 40.f, 150.f, 5.f, "%.0f kg")
            .help("Your weight for the weight classes below."),
        slider("Side-On Turn", vr_kdtrig_tackle_turn, 10.f, 90.f, 5.f, "%.0f deg")
            .help("Your hands' middle, seen from your head, at least this far round to the side of the run (your torso "
                  "turned side-on, a shoulder leading) as you meet it: you may turn into the shoulder at the last "
                  "moment."),
        slider("Hands Low", vr_kdtrig_tackle_low, 0.f, 30.f, 0.5f, "%.1f units")
            .help("Both hands at least this far below your head (a bit above the chest counts: only hands at or over "
                  "the head fail)."),
        slider("Hands Close", vr_kdtrig_tackle_reach, 8.f, 40.f, 1.f, "%.0f units")
            .help("Both hands within this of your head's line, neither reaching ahead (the leading arm tucked)."),
        slider("Head Down", vr_kdtrig_tackle_head, 0.f, 30.f, 0.5f, "%.1f deg")
            .help("Your head tipped down at least this much (tucked into the charge; a plain run looks ahead)."),
        slider("Facing", vr_kdtrig_tackle_face, 20.f, 90.f, 5.f, "%.0f deg")
            .help("Running within this of the way you look (a sidestep doesn't charge)."),
        slider("Hold", vr_kdtrig_tackle_hold, 0.f, 1.f, 0.05f, "%.2f s").help("The pose held running before it counts."),
        slider("Bounce", vr_kdtrig_tackle_bump, 0.f, 300.f, 10.f, "%.0f u/s").help("You bounce back off it."),
        slider("Cooldown", vr_kdtrig_tackle_cooldown, 0.f, 5.f, 0.1f, "%.1f s").help("Before you charge again."),
        slider("Contact", vr_kdtrig_tackle_contact, 0.f, 24.f, 1.f, "%.0f units")
            .help("How near your body must be to the monster's for it to connect (0: touching)."),
        toggle("Impact Feel", vr_kdtrig_tackle_feel)
            .help("A thud on the body, both controllers rumble and the monster cries out as it goes."),
    };
    kdTrigWeightRows(items);
    items.pushBack(open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Tackle)>))
                       .help("Shoulder Charges' chance times each group's share."));
    return items;
}

[[nodiscard]] za::Vector<Item> pageKdTriggers()
{
    return {
        toggle("Ragdoll Triggers", vr_kdtrig)
            .help("Situations in a fight that knock a grunt, an enforcer, a knight or another humanoid down as a "
                  "ragdoll: by stairs and ledges, on slopes, a close shotgun blast, a strong blow, an explosion, "
                  "badly hurt. It gets up as after a shove. Bigger monsters resist (Monster Resistance). Needs "
                  "Knockdowns."),
        slider("Cooldown", vr_kdtrig_cooldown, 0.f, 20.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("After it gets up, no ragdoll trigger takes it again for this long."),
        header("Triggers"),
        open("Ledges", pageIndex(pageKdTrigLedge)).help("Struck with a drop of Least Drop or more right behind it (along the hit, room for its body to drop), it is pushed off."),
        open("Stairs", pageIndex(pageKdTrigStairs)).help("Struck at the top of steps going down behind it (two or more, the first right behind its feet), it falls down them."),
        open("Slopes", pageIndex(pageKdTrigSlope)).help("Standing on a steep slope, hit down it, it topples down the slope."),
        open("Shotgun Range", pageIndex(pageKdTrigShotgun)).help("A shotgun or super shotgun blast of yours from close by knocks it over."),
        open("Wounded", pageIndex(pageKdTrigWounded)).help("Badly hurt, any hit may knock it down."),
        open("Strong Blows", pageIndex(pageKdTrigBlow)).help("A strong melee blow or a prop you fling knocks it over (up to twice the chance at twice Least Damage: a hard punch is about 30)."),
        open("Explosions", pageIndex(pageKdTrigBlast)).help("A nearby explosion (a rocket, a grenade, a barrel, any blast that leaves it alive) throws it down away from the blast's centre (Least Damage: the damage it took, already less with distance)."),
        open("Thrown Bodies", pageIndex(pageKdTrigThrown)).help("A body you throw or swing into a monster knocks it over by weight and momentum; a heavier one staggers."),
        open("Shoulder Charges", pageIndex(pageKdTrigTackle))
            .help("Running at full speed into a monster side-on, a shoulder leading, knocks it back; a heavier one "
                  "staggers back."),
        open("Monster Resistance", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Resist)>))
            .help("How much less likely each monster group goes down (by its size, or its own)."),
        open("Monster Groups", pageIndex(pageMonsterGroups))
            .help("Which monster groups the triggers take, and each trigger's share for each group (Combat > Monster "
                  "Groups)."),
        header("Other Triggers"),
        open("Shoves, Ledges, Leg Sweep, Hits", pageIndex(pageKnockdowns))
            .help("Shoves and bashes, a shove over a ledge, the leg sweep, strong hits unbalancing (Knockdowns)."),
        open("Lift and Leg Pull", pageIndex(pageHoldingEnemies)).help("Lifting and pulling held enemies off their feet."),
    };
}

// Knockdowns (QC vr_knockdown.qc; vr_box3d.cpp, "Knockdowns"; docs/vr-port/KNOCKDOWNS.md): a shove can
// knock a monster down as a ragdoll, alive; it gets up after a while.
[[nodiscard]] za::Vector<Item> pageKnockdowns()
{
    return {
        toggle("Knockdowns", vr_knockdown)
            .help("A shove can knock a monster down as a ragdoll, alive: hit it, grab it, drag it, throw it. After a few "
                  "seconds it gets up where it lies and fights on. Only monsters with ragdolls (Ragdolls on)."),
        header("Chance"),
        slider("Chance", vr_knockdown_chance, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 100.f)
            .help("Every monster's chance, times this. 0: never; 100: every shove."),
        slider("Damage Taken", vr_knockdown_damage, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("A hurt monster goes down more easily: the chance times 1 + this times the share of its health it has "
                  "lost (2: three times as likely at no health left)."),
        slider("One-Handed Shove", vr_knockdown_onehand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The chance of a shove with one hand, times this."),
        slider("Two-Handed Shove", vr_knockdown_twohand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The chance of a shove with both hands, times this."),
        slider("Weapon Bash", vr_knockdown_bash, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A bash with a held weapon knocks down too, at a shove's chance times this. 0: only open-hand shoves."),
        slider("Stamina", vr_knockdown_stamina, 0.f, 1.f, 0.05f, "%.2f")
            .help("Tired shoves knock down less: the chance times 1 - this times the share of your stamina spent (with "
                  "Stamina on). 0: stamina doesn't matter."),
        toggle("Surprise Shoves Knock Down", vr_knockdown_surprise)
            .help("A shove of a monster that isn't fighting you (asleep, idle, busy with another) or from behind always "
                  "knocks it down, if its group can be knocked down by a shove at all."),
        slider("From Behind Within", vr_knockdown_surprise_angle, 0.f, 90.f, 5.f, "%.0f deg")
            .help("A shove whose way is within this of where the monster faces comes from behind. 0: only one that isn't "
                  "fighting you is surprised."),
        toggle("Over a Ledge, Always", vr_knockdown_ledge)
            .help("A shove that would carry it over the edge of a high drop (Ledge Height or more) always knocks it down, "
                  "so it tumbles off as a ragdoll. Not through a wall or railing; stairs are no ledge. Off: the chance as "
                  "anywhere."),
        slider("Ledge Height", vr_knockdown_ledge_drop, 24.f, 256.f, 8.f, "%.0f units").extend(1.f, 1024.f)
            .help("How high a drop must be to count as a ledge (64: a bit more than you are tall; a stair is 16-18)."),
        slider("Ledge Reach", vr_knockdown_ledge_reach, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("How far ahead a ledge is looked for, times how far the shove would carry it (the hop and the slide). "
                  "1: as far as it goes; less: only ledges close by force the knockdown; more: further ones too."),
        slider("Ledge Margin", vr_knockdown_ledge_margin, 0.f, 128.f, 4.f, "%.0f units").extend(0.f, 512.f)
            .help("How far past that a ledge still counts, in units (16: about a monster's half width)."),
        header("Hits Unbalance"),
        toggle("Strong Hits Unbalance", vr_hit_stagger)
            .help("A strong throw (a heavy prop or a weapon, fast) or melee blow may make a monster lose its balance as a "
                  "shove does: it staggers as a ragdoll and recovers, or by its knockdown chance goes over. Monsters "
                  "without one are pushed and flinch. The stronger the hit and the more of its health it took, the likelier."),
        slider("Hit Chance", vr_hit_stagger_chance, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 100.f)
            .help("Every hit's chance, times this (100: every hit past Least Impact)."),
        slider("Least Impact", vr_hit_stagger_min, 0.f, 0.95f, 0.05f, "%.2f")
            .help("Weaker hits never unbalance (1: a 6 kg crate thrown at 10 m/s, or a 30-damage blow); from here to 1 "
                  "the chance rises to sure."),
        slider("Head Blows", vr_hit_stagger_head, 1.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("A melee blow to the head (fists too) counts this much more."),
        slider("Big Monsters Resist", vr_hit_stagger_resist, 0.f, 2.f, 0.05f, "%.2f")
            .help("The chance times (30 / its full health) to this power: 0.75 leaves the shambler about a tenth, the "
                  "ogre a quarter. 0: all alike."),
        slider("Hit Cooldown", vr_hit_stagger_cooldown, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("After a hit unbalances it, no hit does again for this long (no stunlock). Shoves always can."),
        header("Impacts"),
        toggle("Impacts Hurt", vr_ragimpact)
            .help("A monster you hold or threw as a ragdoll, alive or dead, is hurt where it strikes a wall or the floor "
                  "hard: its speed times the mass behind it. Its head or limb struck wears down too, and pops past its own "
                  "health (a head worn through kills). Slam a held head into a wall again and again; throw a lifted one into "
                  "a wall."),
        slider("Impact Damage", vr_ragimpact_damage, 0.f, 0.5f, 0.01f, "%.2f").extend(0.f, 5.f)
            .help("Damage per kg m/s past Least Speed (a grunt's torso thrown into a wall at 6 m/s: about 15)."),
        slider("Damage Curve", vr_ragimpact_curve, 0.5f, 3.f, 0.1f, "%.1f").extend(0.1f, 5.f)
            .help("How damage grows with speed past Least Speed: 1 in step with it, 2 with its square (hard throws far "
                  "worse, soft ones milder), 0.5 its square root. Every curve deals the same at Curve Pivot."),
        slider("Curve Pivot", vr_ragimpact_curve_ref, 0.5f, 10.f, 0.5f, "%.1f m/s").extend(0.1f, 30.f)
            .help("The speed past Least Speed where every Damage Curve deals the same: slower strikes deal less with a "
                  "steeper curve, faster ones more."),
        slider("Most per Strike", vr_ragimpact_max, 0.f, 200.f, 5.f, "%.0f").extend(0.f, 1000.f)
            .help("No strike (nor a landing's crush) deals more than this, the head's and limbs' multipliers in. 0: no cap."),
        slider("Least Speed", vr_ragimpact_min_speed, 0.5f, 8.f, 0.25f, "%.2f m/s").extend(0.5f, 30.f)
            .help("Slower strikes do nothing (a body dropped from your hands strikes at about 4 m/s)."),
        slider("Held Least Speed", vr_ragimpact_held_speed, 0.25f, 5.f, 0.25f, "%.2f m/s").extend(0.25f, 30.f)
            .help("A part your hand holds (a head slammed) counts from this speed: your hand slows as it drives a body."),
        slider("Dent From", vr_ragimpact_dent, 0.f, 20.f, 0.5f, "%.1f dmg").extend(0.f, 100.f)
            .help("A strike dealing this much damage or more dents and wounds the part where it struck, as a blunt melee "
                  "blow does: a head slammed into a wall gets its face dented. 0: never."),
        slider("Drag Swing", vr_ragimpact_drag_speed, 0.f, 4.f, 0.1f, "%.1f m/s").extend(0.f, 20.f)
            .help("A body you hold strikes nothing unless a hand holding it swung this fast (your hand's own move, not your "
                  "walk) in the last moment: dragged along with the stick it bumps the floor and walls unhurt, a slam "
                  "still hurts. 0: every strike counts."),
        slider("Head Strikes", vr_ragimpact_head, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("A strike on the head deals this much more."),
        slider("Limb Strikes", vr_ragimpact_limb, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("A strike on an arm or a leg deals this much more (the torso: 1)."),
        slider("Head and Limb Health", vr_ragimpact_piece_hp, 0.1f, 2.f, 0.05f, "%.2fx").extend(0.01f, 10.f)
            .help("A head's or limb's own health, times its corpse's (a grunt's 80: 32 at 0.4): struck past it, it pops."),
        slider("Head and Limb Wear", vr_ragimpact_wear, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much of a strike's damage wears the head or limb struck: higher pops them sooner. 0: impacts never "
                  "pop them (the damage still hurts)."),
        slider("Landing Crush", vr_ragimpact_crush, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("Landing on a monster (standing, down or a corpse) crushes the part under your feet (its head, a limb, "
                  "its torso): its damage times this; a head or limb crushed past its health pops. 0: never."),
        slider("Crush Least Speed", vr_ragimpact_crush_speed, 100.f, 1000.f, 25.f, "%.0f u/s").extend(0.f, 3000.f)
            .help("Landing slower than this crushes nothing (a jump from the same level lands at about 270 on the floor, "
                  "200 on top of a body: 180 crushes both; a stair step: 160; a 100-unit drop: 400)."),
        slider("Crush Wear", vr_ragimpact_crush_wear, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of a head's or limb's own health a landing at the least speed wears (0.5: a jump from the "
                  "same level wears about 0.55, the second pops it); faster landings more. 0: landings pop nothing."),
        slider("Crush Pop Speed", vr_ragimpact_crush_pop, 200.f, 1000.f, 10.f, "%.0f u/s").extend(0.f, 3000.f)
            .help("A landing this fast pops the head or limb under your feet at once (380: a drop of about two "
                  "heights onto a body lying down)."),
        slider("Crush Push", vr_ragimpact_crush_push, 0.f, 1.5f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A landing on a body lying down (alive or dead) pushes the part under your feet down and out at this "
                  "times your landing speed, the body jolting with it. 0: it lies still."),
        slider("Crush Big Mass", vr_ragimpact_crush_big, 0.f, 600.f, 10.f, "%.0f kg").extend(0.f, 5000.f)
            .help("A monster heavier than this (an ogre, a fiend, a shambler) is only hurt by landings: its head and "
                  "limbs never pop. 0: none is big."),
        slider("Crush Big Most", vr_ragimpact_crush_big_max, 0.f, 1.f, 0.05f, "%.2f")
            .help("The most of a big one's full health one landing takes (0.25: a quarter). 0: no cap."),
        toggle("Crush Standing Ones", vr_ragimpact_crush_standing)
            .help("Landing on a standing monster's head or shoulders crushes too. Off: only one down or staggering as a "
                  "ragdoll."),
        toggle("Crush Breaks Your Fall", vr_ragimpact_crush_cushion)
            .help("A landing that crushed a monster spares you the fall's damage. Off: Quake's fall damage as ever."),
        toggle("Stomp Knocks Down", vr_ragimpact_stomp)
            .help("Landing on a standing monster's top fast enough knocks it down as a shove does (it gets up again), "
                  "if the crush did not kill it; you bounce off it."),
        slider("Stomp Least Speed", vr_ragimpact_stomp_speed, 200.f, 1000.f, 10.f, "%.0f u/s").extend(0.f, 3000.f)
            .help("Landing slower than this knocks nothing down (420: a drop of about 115 units). Monsters heavier than "
                  "Stomp Heavy Mass need more."),
        slider("Stomp Heavy Mass", vr_ragimpact_stomp_heavy, 0.f, 600.f, 10.f, "%.0f kg").extend(0.f, 5000.f)
            .help("A monster heavier than this needs a faster landing: the least speed times the cube root of its mass "
                  "over this (an ogre: about a 180-unit drop; a shambler: about 315). 0: all alike."),
        slider("Stomp Bounce", vr_ragimpact_stomp_bounce, 0.f, 400.f, 10.f, "%.0f u/s")
            .help("How fast you bounce up off the one you knocked down. 0: you drop as it falls."),
        header("Leg Sweep"),
        toggle("Leg Sweep", vr_legsweep)
            .help("Sweep a humanoid's lower legs with something long and blunt, level and fast, and it goes down as in a "
                  "judo foot sweep: its feet slide the way you swept, the rest of it topples over the other way (forward "
                  "or backward, whichever side you sweep it from). A long gun, the crowbar, a sword's hilt (not its "
                  "blade), a long plank: yes. The axe, the super shotgun, the lightning gun, a rock, a brick, a crate: "
                  "no. Only humanoids (no ogres, fiends, shamblers)."),
        slider("Least Length", vr_legsweep_min_length, 20.f, 150.f, 5.f, "%.0f cm").extend(0.f, 400.f)
            .help("How long a thing must be to sweep legs (its drawn shape along its long axis; Debug > Tests > Holding "
                  "Enemies > Leg Sweep Table lists them). The shotgun is 52 cm, the super shotgun 49."),
        slider("Least Slenderness", vr_legsweep_min_slender, 1.f, 10.f, 0.25f, "%.2f").extend(0.f, 30.f)
            .help("How slender it must be: its length over its typical thickness (a gun's barrel, not its grip). The "
                  "grenade launcher is 4.4, the lightning gun 2.6, a crate 1."),
        slider("Least Speed", vr_legsweep_speed, 0.5f, 6.f, 0.1f, "%.1f m/s").extend(0.f, 20.f)
            .help("How fast the part that meets the legs must move, level (a sweep rising or falling steeply is a blow, "
                  "not a sweep)."),
        slider("Heavy Mass", vr_legsweep_heavy, 0.f, 400.f, 10.f, "%.0f kg").extend(0.f, 2000.f)
            .help("A humanoid heavier than this needs a faster sweep: the least speed times the cube root of its mass "
                  "over this. 0: all alike."),
        slider("Damage", vr_legsweep_damage, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("What a sweep deals once it is down."),
        slider("Topple Speed", vr_legsweep_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("How fast its body turns over about its swept feet. 0: it just drops."),
        slider("Feet Slide", vr_legsweep_feet, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("How fast its feet slide along the sweep, times the sweep's speed."),
        slider("Fall Along Its Facing", vr_legsweep_align, 0.f, 45.f, 5.f, "%.0f deg")
            .help("A sweep within this angle of the enemy's own facing, back or sides falls straight along that line: "
                  "swept from its side it falls cleanly forward or backward, not diagonally. 0: it falls exactly against "
                  "the sweep. Leg pulls (Holding Enemies > Leg Grabs) fall the same way."),
        slider("Legs Stay Straight", vr_legsweep_legs, 0.f, 1.5f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("Swept (or its legs pulled) so that it falls backward, its legs keep their stance this long before they go limp: "
                  "its knees don't fold under it and its feet stay where they slid. 0: limp at once."),
        header("Each Monster's Chance"),
        open("Each Monster Group's Chance", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Shove)>))
            .help("A shove's base chance to knock each monster group down (grunts 0.45, shamblers 0.03; Combat > Monster "
                  "Groups)."),
        open("Each Monster Group's Push", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Push)>))
            .help("How far shoves, bashes and unbalancing hits move each monster group, times (Combat > Monster Groups)."),
        open("Each Monster Group's Hits Unbalance", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Stagger)>))
            .help("Each monster group's share of Hits Unbalance's chance (Combat > Monster Groups)."),
        header("Down"),
        slider("Time Down, Least", vr_knockdown_time_min, 0.f, 10.f, 0.25f, "%.2f s").extend(0.f, 60.f)
            .help("How long it stays down, at least (at random up to Time Down, Most)."),
        slider("Time Down, Most", vr_knockdown_time_max, 0.f, 10.f, 0.25f, "%.2f s").extend(0.f, 60.f),
        slider("Hit Keeps It Down", vr_knockdown_hit_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 10.f)
            .help("Each hit while it lies there (a punch, a shot, a stomp) keeps it down at least this long after the hit, "
                  "past its own time (never past Kept Down, Most in all)."),
        slider("Handling Keeps It Down", vr_knockdown_keep_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 10.f)
            .help("Holding, dragging or lifting it keeps it down while you do and this long after you let go, past its "
                  "own time (never past Kept Down, Most in all)."),
        slider("Kept Down, Most", vr_knockdown_keep_max, 0.f, 15.f, 0.25f, "%.2f s").extend(0.f, 60.f)
            .help("The longest it stays down in all while you keep at it (hits, stomps, holding it). Left alone it gets "
                  "up on its own time, or soon after you stop if that has passed. 0: nothing keeps it down longer."),
        slider("Launch", vr_knockdown_push, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("How hard the shove throws the body, times the shove's own push: the fastest its travel starts (Travel "
                  "sets how far). 0: it drops where it stood."),
        slider("Travel", vr_knockdown_shove_travel, 0.f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("How far a shoved-down enemy goes, times how far the same shove pushes one that stays standing (where "
                  "its hips lie once it is down; one- or two-handed alike). 0: it falls where it stood."),
        slider("Topple Angle", vr_knockdown_shove_topple_angle, 0.f, 150.f, 5.f, "%.0f deg").extend(0.f, 180.f)
            .help("How far it turns over about its feet along the shove, head and chest first: 90 from standing to flat "
                  "on its back or front. 0: no turn, it falls as it will."),
        slider("Topple Time", vr_knockdown_shove_topple_time, 0.2f, 1.5f, 0.05f, "%.2f s").extend(0.05f, 3.f)
            .help("How long it takes to turn that far: slow to start, fastest past halfway, landing. Less: knocked flat "
                  "hard; more: it keels over."),
        slider("Feet Lag", vr_knockdown_shove_feet_lag, 0.f, 1.f, 0.05f, "%.2f")
            .help("How far its feet lag behind as it goes over (its head and chest as far ahead), a share of the "
                  "travel's speed. 0: the whole body goes alike."),
        slider("Max Spin", vr_knockdown_shove_max_spin, 60.f, 720.f, 10.f, "%.0f deg/s").extend(10.f, 2000.f)
            .help("The fastest it ever turns while the shove carries it (over a ledge too, as it tips over the edge and "
                  "falls): no cartwheels, however hard the shove."),
        slider("Topple Over a Ledge", vr_knockdown_shove_ledge_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("Shoved over a ledge, it is pushed whole so it goes over the edge, and turns over its feet as it falls "
                  "this fast (a one-handed shove less fast) until it has turned Topple Angle. Off: it goes over upright."),
        slider("Ledge Push", vr_knockdown_shove_ledge_push, 0.f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("How hard a shove over a ledge throws it off, times the shove's own push (it is always kept going "
                  "until it is past the edge). Less: it drops closer to the ledge's foot."),
        slider("Ledge Carry Past the Edge", vr_knockdown_shove_ledge_over, 0.f, 1.f, 0.05f, "%.2fx")
            .help("Once a shove has taken it past the edge, how much of its speed it keeps as it falls: low, it just tips "
                  "over the lip and drops near the ledge's foot; 1: it flies on as it goes (as before)."),
        cycle("Weapon Stays in Hand", vr_knockdown_weld, {{0.f, "Off"}, {1.f, "In the Hand"}, {2.f, "Where It Was (Old)"}})
            .help("Its weapon stays in its hand while it is down or staggering, lifted or struggling (killed there, it "
                  "drops it as usual): held by the hand that holds it in its rest pose, following that hand. Where It "
                  "Was (Old): fixed to the part nearest it where its frame had it (the enforcer's rifle hung at his side). "
                  "Off: it flops loose as a dead one's."),
        header("Getting Up"),
        slider("Struggling Strength", vr_knockdown_wiggle, 0.f, 3.f, 0.1f, "%.1fx")
            .help("Living knockdowns gently curl their chest and nod their head about their resting pose. Arms and legs follow physically. 0: still. Dead bodies never struggle."),
        slider("Struggling Frequency", vr_knockdown_wiggle_frequency, 0.1f, 5.f, 0.1f, "%.1f Hz"),
        slider("Struggling Pause", vr_knockdown_wiggle_pause, 0.f, 5.f, 0.25f, "%.2f s")
            .help("Pause between bursts of movement. 0: continuous."),
        slider("Room Search", vr_knockdown_search, 0.f, 128.f, 4.f, "%.0f units").extend(0.f, 512.f)
            .help("How far from the body it looks for room to stand, never through a wall, floor or ceiling. No room: it "
                  "stays down (dragged into a tight corner or under something low) and tries again."),
        slider("Search in Water", vr_knockdown_water_search, 0.f, 512.f, 16.f, "%.0f units").extend(0.f, 2048.f)
            .help("Swimming in water, slime or lava: how far it looks for a floor where its head is out of it (the "
                  "shallows, the bank) to paddle to. It gets up on one within Room Search of where it floats."),
        slider("Climb Out of Water", vr_knockdown_water_climb, 0.f, 96.f, 4.f, "%.0f units").extend(0.f, 256.f)
            .help("Getting up in a liquid: how far over its body the floor it gets up onto may be. Higher (carried down "
                  "there, or deep under): it swims up first, or gets up where it floats."),
        slider("Give Up in Water", vr_knockdown_water_giveup, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("No such floor for this long after its time to get up: it gets up where it floats, as a walking "
                  "monster again (it sinks, and can drown). 0: it floats until one is found."),
        slider("Retry", vr_knockdown_retry, 0.1f, 3.f, 0.1f, "%.1f s").extend(0.1f, 10.f)
            .help("How often it tries again to get up when there is no room."),
        slider("Steepest Slope to Stand", vr_knockdown_getup_slope, 0.f, 45.f, 1.f, "%.0f deg").extend(0.f, 60.f)
            .help("Lying on a slope up to this steep, it gets up as on level ground. Steeper: it slides on until there "
                  "is room, or is forced up (below)."),
        slider("Forced Get-Up", vr_knockdown_getup_force, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("Still down this long after it went down, with no room to stand (a steep slope, a cramped spot): it "
                  "gets up where it lies. 0: it waits for room."),
        slider("Blend", vr_knockdown_blend, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("How long the body takes to blend from how it lies into its get-up animation."),
        slider("Get-Up Speed", vr_knockdown_getup_speed, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.1f, 10.f)
            .help("How fast its get-up animation plays."),
        header("Debug"),
        cycle("Print Rolls", vr_knockdown_debug, {{0.f, "Off"}, {1.f, "On"}, {2.f, "And Get-Ups' Motion"}, {3.f, "And Each Frame's"}})
            .help("Prints each shove's chance and roll, and each get-up (developer 1). And Get-Ups' Motion: a line per get-up "
                  "of how it is drawn (how fast it moves, its biggest jump, the frames that go back on the one before: a "
                  "jitter; the switch from its ragdoll to its animation). And Each Frame's: a line a frame as well. Knock "
                  "one down with vr_knockdown_test 0 and get it up with vr_knockdown_test 1."),
    };
}

// A running chainsaw's engine (NOTES.md vrfiringrange_2026-10-01_16-37-54): its exhaust's smoke, its shake in the hand
// and lying on the ground (vr_chainsaw.cpp, QC vr_chainsaw.qc VR_Saw_PropEngine).
[[nodiscard]] za::Vector<Item> pageChainsawEngine()
{
    return {
        header("Exhaust Smoke"),
        slider("Smoke", vr_chainsaw_smoke, 0.f, 30.f, 1.f, "%.0f puffs a second").extend(0.f, 100.f)
            .help("Faint puffs of smoke from the running engine's exhaust, in the hand and lying running. 0: none."),
        slider("Smoke Opacity", vr_chainsaw_smoke_alpha, 0.f, 1.f, 0.02f, "%.2f")
            .help("How opaque each puff is as it leaves the exhaust; it fades out over about 2 s as it spreads and rises."),
        header("Shake"),
        slider("In One Hand", vr_chainsaw_shake, 0.f, 6.f, 0.25f, "%.2f mm").extend(0.f, 20.f)
            .help("How much the running chainsaw (and the hand on it) shakes held in one hand: this far, and turning 0.2 "
                  "degrees a mm; 1.6x with the chain running. Drawn only: the aim and the cuts don't shake. 0: still."),
        slider("In Two Hands", vr_chainsaw_shake_2h, 0.f, 6.f, 0.25f, "%.2f mm").extend(0.f, 20.f)
            .help("The same, steadied by the other hand on its front handle."),
        slider("On the Ground", vr_chainsaw_shake_ground, 0.f, 1.f, 0.05f, "%.2f m/s").extend(0.f, 3.f)
            .help("Let go running (Runs On When Let Go), as it lies it is kicked 20 times a second, mostly up, at a point "
                  "of its engine at random: it rocks and walks about a little, physically. How hard each kick is. 0: "
                  "still."),
        header("Pulling the Cord"),
        slider("Pull Shake", vr_chainsaw_pull_shake, 0.f, 8.f, 0.25f, "%.2f mm").extend(0.f, 20.f)
            .help("Each pull of the cord of a chainsaw not running, with fuel, the engine turning over: the chainsaw "
                  "shakes this much, dying out over Pull Shake Time. A weak pull (too slow), half. With an empty tank, "
                  "no shake, smoke or sparks: only the sound. 0: still."),
        slider("Pull Shake Time", vr_chainsaw_pull_shake_time, 0.05f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("How long a pull's shake lasts."),
        slider("Pull Smoke", vr_chainsaw_pull_smoke, 0.f, 12.f, 1.f, "%.0f puffs").extend(0.f, 40.f)
            .help("Puffs of exhaust smoke each pull makes (as Smoke's, at Smoke Opacity). A weak pull, half. 0: none."),
        slider("Pull Sparks", vr_chainsaw_pull_sparks, 0.f, 40.f, 1.f, "%.0f").extend(0.f, 60.f)
            .help("Tiny sparks out of the exhaust at each pull. A weak pull, half. 0: none."),
        toggle("Model Handle in the Hand", vr_chainsaw_model_handle)
            .help("The cord's handle in your fist is the chainsaw model's own, the one seated on it, easing from its seat "
                  "into the fist's turn as you take it. Off: a plain round handle."),
        cycle("Cord Hint", vr_chainsaw_cord_hint, {{0.f, "Off"}, {1.f, "Until the First Pull"}, {2.f, "Always"}})
            .help("The cord's handle on a chainsaw in your hand dangles a little about its seat, swung by your hand's "
                  "motion, a hint that the engine starts by pulling it. Until the First Pull: still once you have "
                  "pulled a cord (remembered by your game, its saves and the next levels)."),
    };
}

#include "vr_menu_ad_guns.inc" // AD support: Combat > Enemy Weapons > Arcane Dimensions' Guns (pageAdGuns)

// The weapons monsters drop as they die (ROUND21.md, "Enemy weapons: the grunts' shotguns and the enforcers' laser
// rifles"): their base damage and ammo. Their offsets and weights are in the weapon menus (slots _19 .. _23).
// Combat > Armed Grunts (Experimental) (QC vr_armedgrunt.qc; ROUND22.md, "Armed grunts"): grunts holding your weapons.
[[nodiscard]] const char* armedGripSubject()
{
    return aggrip::subjectLine();
}

// Drag Mode (vr_posestudio.cpp): the grips set by hand on the armed humanoid you walk up to; this page reopened after.
void armedGripsDragMode()
{
    (void)posestudio::start(qvr::menu::currentPage());
}

// The Grip Editor (vr_aggrip.cpp): the nearest armed monster's weapon in its hand, live.
[[nodiscard]] za::Vector<Item> pageArmedGrips()
{
    const auto pos = [](const char* label, cvar_t& c, const char* what) {
        return slider(label, c, -8.f, 8.f, 0.1f, "%.1f").extend(-30.f, 30.f).help(what);
    };
    const auto ang = [](const char* label, cvar_t& c, const char* what) {
        return slider(label, c, -180.f, 180.f, 2.5f, "%.1f deg").help(what);
    };
    return {
        header("Editing"),
        action("Drag Mode (Grab and Place)", armedGripsDragMode)
            .help("The Pose Studio: walk up to any armed grunt or enforcer (they hold still) and grab its weapon or a hand "
                  "with your grip: move and turn it as you want it held, let go. The weapon: how it sits in the hand; a "
                  "hand: where it holds the weapon. A panel beside you: Every Weapon / This Weapon, Undo, Reset Grips, "
                  "Frozen, Cancel, Done (prints the grips as code; quakevr/proposed_defaults). B/Y undoes, the menu "
                  "button is Done. (vr_posestudio)"),
        info(armedGripSubject),
        toggle("Freeze Them", vr_agrunt_freeze)
            .help("Armed grunts and enforcers hold still in their idle pose, weapons ready, while you tune (spawn one with "
                  "the toolgun's Armed Humanoid page, or Debug > Tests > Test Scenarios: Armed Grip Editor). "
                  "(vr_agrunt_freeze)"),
        cycle("Hand", vr_agrunt_grip_hand, {{0.f, "Main Hand"}, {1.f, "Off Hand (Dual)"}, {2.f, "Off Hand on the Second Grip"}})
            .help("Main Hand: the weapon in its right hand. Off Hand (Dual): its left hand's own weapon. Second Grip: its "
                  "left hand on a two-handed weapon's second grip (where its hand goes, along the weapon's axes)."),
        cycle("Edits", vr_agrunt_grip_scope, {{0.f, "Every Weapon (This Kind)"}, {1.f, "This Weapon Only"}})
            .help("Every Weapon: the hand's grip for every weapon this kind of monster holds (grunts, enforcers: each "
                  "its own), as you hold them; tune this first. This Weapon Only: one weapon's own nudge on top of it. "
                  "(vr_agrunt_grip_scope; with Posed Arms on, vr_agarms)"),
        header("The Weapon in the Hand"),
        pos("Forward", vr_agrunt_grip_x, "Moves the weapon forward (back: negative) in the hand: units, along the hand's "
                                          "grip (the way its own gun pointed). Second Grip: the hand along the weapon."),
        pos("Left", vr_agrunt_grip_y, "Moves the weapon to the hand's left (units)."),
        pos("Up", vr_agrunt_grip_z, "Moves the weapon up in the hand (units): a sword's handle into the fist, not its guard."),
        ang("Pitch", vr_agrunt_grip_pitch, "Tips the weapon down (up: negative) in the hand."),
        ang("Yaw", vr_agrunt_grip_yaw, "Turns the weapon left (right: negative) in the hand."),
        ang("Roll", vr_agrunt_grip_roll, "Rolls the weapon about its length in the hand."),
        command("Fit Angles to Its Pose", "vr_agrunt_grip_fit")
            .help("Sets the angles so its hand holds the weapon as its pose now has the weapon turned, the wrist "
                  "unturned (a starting point; freeze it first)."),
        header("Keep Them"),
        command("Print as Defaults", "vr_agrunt_grip_dump")
            .help("Prints the changed grips as code for the shipped defaults (the console; vr_agrunt_grip_dump). They are "
                  "kept in your config meanwhile (vr_agrunt_grips)."),
        command("Reset This One", "vr_agrunt_grip_reset").help("This weapon and hand back to the shipped default."),
    };
}

[[nodiscard]] za::Vector<Item> pageArmedGrunts()
{
    const auto weight = [](const char* label, cvar_t& c) {
        return slider(label, c, 0.f, 5.f, 0.25f, "%.2f").extend(0.f, 100.f)
            .help("How often an armed grunt from the pool (the toolgun's Armed Grunt (Pool), a map's with no weapon "
                  "set) holds this, against the others' weights. 0: never.");
    };
    return {
        header("Their Weapons"),
        slider("Damage to You", vr_agrunt_damage_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Their weapon's damage to you, times this (its damage to monsters is your weapon's own): a rocket "
                  "isn't a one-shot (vr_agrunt_damage_scale)."),
        slider("Spare Clips", vr_agrunt_reserve, 0.f, 10.f, 1.f, "%.0f").extend(0.f, 100.f)
            .help("The clips each carries beyond the one in its gun; each reload takes one. With none left it throws the "
                  "gun away and fights with its fists. Not the grunts' and enforcers' rifles: their clip is all they "
                  "have (vr_agrunt_reserve)."),
        slider("Dual Wielding", vr_agrunt_dual_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance one from the pool holding a weapon that pairs gets a second in its left hand: one-handed "
                  "blades and clubs (a sword, the crowbar, the axe) swung in turn or both at once; guns one in each "
                  "hand (not the rocket launcher, the Laser Cannon, the chainsaw, the hammers). Each can be knocked or "
                  "pulled out of its hand, and it goes for another (vr_agrunt_dual_chance)."),
        slider("Dual Guns' Penalty", vr_agrunt_dual_penalty, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("Two guns held one in each hand aim as yours do in one hand: no two-hand spread reduction (spread "
                  "and aim error x2 at 1) and heavy guns kick their aim up a shot by their heft. This times those "
                  "penalties; 0: as accurate as with one gun in both hands (vr_agrunt_dual_penalty)."),
        toggle("Pick Up a Second", vr_agrunt_dual_pickup)
            .help("One holding a single weapon that pairs goes for a second lying about, out of a fight, and holds it in "
                  "its left hand (vr_agrunt_dual_pickup)."),
        slider("Reload Time", vr_agrunt_reload_time, 0.2f, 5.f, 0.1f, "%.1f s").extend(0.f, 20.f)
            .help("How long it stands reloading (vr_agrunt_reload_time)."),
        slider("Aim Error", vr_agrunt_aim_error, 0.f, 15.f, 0.5f, "%.1f deg").extend(0.f, 45.f)
            .help("How far off its aim is at most, each shot, besides the weapon's own spread (vr_agrunt_aim_error)."),
        header("Their Strikes"),
        slider("Torso Twist", vr_agrunt_strike_torso, 0.f, 2.f, 0.05f, "%.2fx")
            .help("How far its torso twists and leans in its melee strikes and punches: wound back, driven through the "
                  "blow, followed through. 0: only its arms move (vr_agrunt_strike_torso)."),
        slider("Shoulder Drive", vr_agrunt_strike_shoulder, 0.f, 2.f, 0.05f, "%.2fx")
            .help("How far the striking shoulder lifts and bends back, its hand drawn back by its head and its elbow "
                  "raised in the wind-up, then driven down through the blow (vr_agrunt_strike_shoulder)."),
        slider("Hips in the Twist", vr_agrunt_strike_hips, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of the torso's twist its hips take; its feet stay planted (vr_agrunt_strike_hips)."),
        slider("Head Steady", vr_agrunt_strike_head, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of the torso's turn its head takes back, its eyes kept on you (vr_agrunt_strike_head)."),
        header("Disarming"),
        toggle("Hits Disarm", vr_agrunt_disarm)
            .help("A hard blow on its gun hand (a shot, a melee blow, a thrown thing) or a knockdown knocks the "
                  "weapon out of its hand, its rounds with it; your shots by Shooting It Out's rule below while that is "
                  "on (vr_agrunt_disarm)."),
        slider("Disarming Blow", vr_agrunt_disarm_damage, 1.f, 50.f, 1.f, "%.0f damage").extend(0.f, 500.f)
            .help("A blow's least damage on its gun hand to disarm it (vr_agrunt_disarm_damage)."),
        toggle("Grab Its Weapon", vr_agrunt_disarm_yank)
            .help("Your empty hand closing on its weapon takes it out of its hand: it is yours (vr_agrunt_disarm_yank)."),
        slider("Blow on Its Weapon", vr_agrunt_disarm_swing, 1.f, 10.f, 0.25f, "%.2f m/s").extend(0.f, 50.f)
            .help("How fast your hand (a punch, or the weapon it holds) must be going when it lands on the weapon an "
                  "armed grunt holds to knock it out of its hands, along the blow. A melee hit lands from 3.2 m/s "
                  "(vr_melee_speed): 4.5 is a deliberate hard swing (vr_agrunt_disarm_swing)."),
        slider("Throw at Its Weapon", vr_agrunt_disarm_throw, 1.f, 100.f, 1.f, "%.0f kg m/s").extend(0.f, 1000.f)
            .help("The momentum (its mass times its speed) a thrown weapon, prop or gib must carry when it lands on the "
                  "weapon an armed grunt holds to knock it out: 20 is a 2 kg weapon thrown at 10 m/s; a gib or a shell "
                  "tossed is far under it (vr_agrunt_disarm_throw)."),
        toggle("Tug-of-War", vr_agrunt_tug)
            .help("Grabbing its weapon starts a struggle: it holds on and pulls back, and may shove you, headbutt you or "
                  "fire. Pull your hand back far enough for long enough to take it. Staggered, knocked down, stunned "
                  "by a parry or badly hurt, it lets go at once; hurt or in pain, sooner. Off: the grab takes it at once "
                  "(vr_agrunt_tug)."),
        toggle("Tug: Physical", vr_agrunt_tug_ragdoll)
            .help("The tug as physics: it stands its ground as an active ragdoll, your hand holding its weapon in its "
                  "hand, its arms pulling back; pull hard enough, or long enough, and its grip breaks: the weapon is "
                  "yours, it staggers a moment and fights on unarmed. Let go first and it keeps it. Off: the old tug "
                  "(the distance and time below; it fights back) (vr_agrunt_tug_ragdoll)."),
        slider("Tug: Break Force", vr_agrunt_tug_break_force, 50.f, 2000.f, 25.f, "%.0f N").extend(0.f, 20000.f)
            .help("A pull this hard on its weapon tears it out of its hand at once (the physical tug; less as it is "
                  "hurt) (vr_agrunt_tug_break_force)."),
        slider("Tug: Break Impulse", vr_agrunt_tug_break_impulse, 10.f, 1000.f, 10.f, "%.0f N s").extend(0.f, 10000.f)
            .help("Pulling over time: the pull's force times its time (counted above Tug: Least Pull) that tears it "
                  "out of its hand (the physical tug; less as it is hurt) (vr_agrunt_tug_break_impulse)."),
        slider("Tug: Least Pull", vr_agrunt_tug_pull_force, 0.f, 1000.f, 10.f, "%.0f N").extend(0.f, 10000.f)
            .help("Only a pull at least this hard counts in the physical tug: its grip gives only while you pull this "
                  "hard, and gets its hold back as you let up (vr_agrunt_tug_pull_force)."),
        slider("Tug: Least Pull Time", vr_agrunt_tug_pull_time, 0.f, 3.f, 0.05f, "%.2f s").extend(0.f, 30.f)
            .help("You must keep pulling at least Tug: Least Pull this long before its weapon can come out: a "
                  "tug-of-war, not a sharp yank. 0: the break force tears it out at once (vr_agrunt_tug_pull_time)."),
        slider("Tug: Yank Tears It Out At", vr_agrunt_tug_snap, 0.f, 10.f, 0.1f, "%.1fx").extend(0.f, 100.f)
            .help("A yank this many times the break force tears the weapon out at once, before the least pull time. "
                  "0: never (vr_agrunt_tug_snap)."),
        slider("Tug: Wounds Weaken Its Grip", vr_agrunt_tug_hurt_ease, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 1.f)
            .help("How much its wounds weaken its grip in the physical tug: 0 not at all, 1 in proportion to the "
                  "health it lost (vr_agrunt_tug_hurt_ease)."),
        slider("Tug: Stumble When Disarmed", vr_agrunt_tug_won_stumble, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 5.f)
            .help("How long it stumbles as you tear its weapon away, before it stands and fights on unarmed (a shove "
                  "or a leg sweep then still floors it) (vr_agrunt_tug_won_stumble)."),
        slider("Grab: Precision", vr_agrunt_grab_slack, -2.f, 10.f, 0.5f, "%.1f cm").extend(-10.f, 50.f)
            .help("How far off its weapon's surface your fist may be and still grab it (0: touching it). Never by "
                  "its arm or chest (vr_agrunt_grab_slack)."),
        slider("Grab: Fastest Hand", vr_agrunt_grab_speed, 0.f, 6.f, 0.1f, "%.1f m/s").extend(0.f, 50.f)
            .help("A hand moving faster than this (a punch, a swipe) doesn't grab its weapon. 0: any speed "
                  "(vr_agrunt_grab_speed)."),
        slider("Tug: Its Pull", vr_agrunt_tug_strength, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 3.f)
            .help("How hard its arms pull its weapon back in the physical tug (vr_agrunt_tug_strength)."),
        slider("Tug: Longest", vr_agrunt_tug_hold_max, 0.f, 20.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("The physical tug lasts at most this long: then it wrenches its weapon free. 0: no limit "
                  "(vr_agrunt_tug_hold_max)."),
        slider("Tug: Pull Distance", vr_agrunt_tug_pull, 2.f, 40.f, 1.f, "%.0f units").extend(0.f, 200.f)
            .help("How far you must pull your hand back from where you grabbed its weapon, away from it, to be "
                  "winning (vr_agrunt_tug_pull)."),
        slider("Tug: Pull Time", vr_agrunt_tug_time, 0.f, 5.f, 0.1f, "%.1f s").extend(0.f, 30.f)
            .help("How long you must keep it pulled that far against a calm grunt at full health; less as it is hurt, "
                  "half while it flinches from a hit. 0: any pull wins (vr_agrunt_tug_time)."),
        slider("Tug: It Fights Back Every", vr_agrunt_tug_fight, 0.f, 5.f, 0.1f, "%.1f s").extend(0.f, 30.f)
            .help("About how often it fights back during the struggle: a shove or a headbutt breaks your grip, a gun "
                  "fires at you. 0: never (vr_agrunt_tug_fight)."),
        slider("Tug: Lets Go When Hurt Below", vr_agrunt_tug_hurt, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 1.f)
            .help("A grab takes the weapon at once from a grunt down to this share of its health "
                  "(vr_agrunt_tug_hurt)."),
        slider("Parry Disarm Chance", vr_agrunt_parry_disarm, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 1.f)
            .help("The chance your weapon's parry of its melee blow knocks its weapon out of its hand; a well-timed "
                  "parry's 1.5 times (vr_agrunt_parry_disarm)."),
        header("Parrying Your Blows (Experimental)"),
        toggle("They Parry", vr_agrunt_parry)
            .help("One holding a weapon defends against a melee blow as you do: seeing it coming it raises and angles its "
                  "weapon into the blow's way, and a blow that meets its weapon before its body is parried (the ring, "
                  "sparks, your hand knocked back). A blow to its head, legs or a side its weapon doesn't cover lands in "
                  "full. Never from behind, unaware of you, staggered or in pain. Monsters' blows on it too "
                  "(vr_agrunt_parry)."),
        slider("Parry: Damage Kept", vr_agrunt_parry_damage, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of a parried blow's damage it still takes (vr_agrunt_parry_damage)."),
        slider("Parry: Parries Before Disarm", vr_agrunt_parry_count, 1.f, 6.f, 0.5f, "%.1f").extend(0.1f, 50.f)
            .help("Its parry stamina: the parry that spends it all knocks its weapon out of its hand. 2: the second "
                  "parry in a row (vr_agrunt_parry_count)."),
        slider("Parry: Stamina Per Parry", vr_agrunt_parry_cost, 0.f, 3.f, 0.25f, "%.2f").extend(0.f, 50.f)
            .help("What each parry spends of it (vr_agrunt_parry_cost)."),
        slider("Parry: Recovery", vr_agrunt_parry_recover, 0.f, 2.f, 0.05f, "%.2f /s").extend(0.f, 20.f)
            .help("The stamina it gets back a second: at 0.35 a parry's worth in about 3 s (vr_agrunt_parry_recover)."),
        slider("Parry: Reaction Time", vr_agrunt_parry_react, 0.f, 1.f, 0.01f, "%.2f s").extend(0.f, 5.f)
            .help("From seeing a blow coming to its weapon raised into its way, at Normal skill (Easy 1.4x, Hard 0.85x, "
                  "Nightmare 0.7x): a quick swing gets through (vr_agrunt_parry_react)."),
        slider("Parry: Chance", vr_agrunt_parry_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it sees a blow coming in time, at Normal skill (Easy 0.6x, Hard 1.15x, Nightmare 1.3x); "
                  "caught out, its weapon stays where it was: a blow still meeting it there is parried "
                  "(vr_agrunt_parry_chance)."),
        toggle("Drops It When Floored", vr_agrunt_fall_drop)
            .help("Knocked down onto the floor (not just staggered by a shove) it drops its weapon beside it, and "
                  "takes it back as it gets up unless you took it (vr_agrunt_fall_drop; with Hits Disarm)."),
        toggle("Keeps It in a Gentle Fall", vr_agrunt_fall_keep)
            .help("A gentle fall (a shove's backpedal to the floor) keeps the weapon in its hand, but while it lies "
                  "there a grab takes it at once and a blow knocks it away. A violent one (below) drops it "
                  "(vr_agrunt_fall_keep)."),
        slider("Fall: Drops It Below", vr_agrunt_fall_keep_height, 0.f, 5.f, 0.05f, "%.2f m").extend(0.f, 50.f)
            .help("Falling to a floor this far below the one it stood on (off a ledge) it lets go "
                  "(vr_agrunt_fall_keep_height)."),
        slider("Fall: Drops It Falling Faster", vr_agrunt_fall_keep_speed, 0.f, 20.f, 0.5f, "%.1f m/s").extend(0.f, 100.f)
            .help("Its body falling faster than this, it lets go (vr_agrunt_fall_keep_speed)."),
        slider("Fall: Drops It Flung Faster", vr_agrunt_fall_keep_fling, 0.f, 20.f, 0.5f, "%.1f m/s").extend(0.f, 100.f)
            .help("A strong force: its body moving faster than this any way but down (thrown, held and flung, blown "
                  "away), it lets go (vr_agrunt_fall_keep_fling)."),
        toggle("Picks Weapons Up", vr_agrunt_rearm)
            .help("Disarmed, it fights with its fists and goes for a weapon lying near (its own or another) when you are "
                  "not nearer to it than it is (vr_agrunt_rearm)."),
        slider("Looks For Weapons Within", vr_agrunt_rearm_range, 64.f, 1024.f, 32.f, "%.0f units").extend(0.f, 4096.f)
            .help("How far it looks for a weapon to pick up (vr_agrunt_rearm_range)."),
        toggle("Re-Arms Out of a Fight", vr_agrunt_rearm_idle)
            .help("Unarmed and not fighting, it also walks over to a weapon lying near and takes it up "
                  "(vr_agrunt_rearm_idle; with Picks Weapons Up)."),
        slider("Gives Up on a Weapon After", vr_agrunt_seek_timeout, 1.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long it keeps going for a weapon it doesn't reach (no nearer for 2.5 s: sooner) before giving "
                  "it up for a while; it only goes for weapons it can walk to on the floor, not in liquid or up a "
                  "ledge (vr_agrunt_seek_timeout)."),
        slider("Leaves It Alone For", vr_agrunt_seek_shun, 0.f, 120.f, 1.f, "%.0f s").extend(0.f, 600.f)
            .help("How long it leaves a weapon it gave up on before trying it again (vr_agrunt_seek_shun)."),
        header("Disarming: Shooting It Out"),
        toggle("Shoot Weapons Out", vr_agrunt_shotdisarm)
            .help("Your shots on the weapon an armed grunt holds, or the hand holding it, add up; each time they pass "
                  "the threshold within the time window, a roll may knock the weapon out of its hand, along your shots. "
                  "A close shotgun blast on the gun likely does, a quick nail burst may, a chip shot from afar seldom. "
                  "Each hand's weapon on its own. While on, your shots disarm only this way (vr_agrunt_shotdisarm)."),
        cycle("What Counts", vr_agrunt_shotdisarm_parts, {{0.f, "The Weapon"}, {1.f, "The Hand"}, {2.f, "Weapon or Hand"}})
            .help("Which hits add up: on the weapon itself, on the hand and forearm holding it, or either "
                  "(vr_agrunt_shotdisarm_parts)."),
        slider("Weapon Hits Count", vr_agrunt_shotdisarm_weapon_w, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("A hit on the weapon counts its damage times this (vr_agrunt_shotdisarm_weapon_w)."),
        slider("Hand Hits Count", vr_agrunt_shotdisarm_hand_w, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("A hit on the hand or forearm counts its damage times this; it hurts the grunt as usual "
                  "(vr_agrunt_shotdisarm_hand_w)."),
        slider("Hit Volume Size", vr_agrunt_shotdisarm_size, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The weapon's and the hand's hit volumes times this: under 1 only precise shots count, over 1 near ones "
                  "too (vr_agrunt_shotdisarm_size)."),
        slider("Near Misses Count Within", vr_agrunt_shotdisarm_near, 0.f, 12.f, 0.5f, "%.1f units").extend(0.f, 64.f)
            .help("A shot passing this close to the weapon or the hand, but not on it, counts at half. 0: only hits "
                  "(vr_agrunt_shotdisarm_near)."),
        slider("Damage to a Roll", vr_agrunt_shotdisarm_damage, 1.f, 100.f, 1.f, "%.0f damage").extend(0.f, 1000.f)
            .help("The damage adding up to a roll: a shotgun blast's six pellets are 24, a nail 9, a super nail 18, a "
                  "bolt 30 (times its weight below). Each crossing rolls once, then it starts over "
                  "(vr_agrunt_shotdisarm_damage)."),
        slider("Time Window", vr_agrunt_shotdisarm_window, 0.05f, 3.f, 0.05f, "%.2f s").extend(0.f, 30.f)
            .help("How long hits add up, from the first: then it starts over (vr_agrunt_shotdisarm_window)."),
        slider("Chance a Roll", vr_agrunt_shotdisarm_chance, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 1.f)
            .help("The roll's chance right at the threshold (vr_agrunt_shotdisarm_chance)."),
        slider("Chance Past the Threshold", vr_agrunt_shotdisarm_chance_scale, 0.f, 2.f, 0.05f, "+%.2f").extend(0.f, 10.f)
            .help("Added to the chance for each threshold's worth past it (half past: half of it); at most 0.95. A "
                  "shotgun blast all on the gun, 24 on 16: 0.25 + 0.4 (vr_agrunt_shotdisarm_chance_scale)."),
        slider("After a Failed Roll", vr_agrunt_shotdisarm_cooldown, 0.f, 3.f, 0.05f, "%.2f s").extend(0.f, 30.f)
            .help("No hits add up for this long after a roll that failed (vr_agrunt_shotdisarm_cooldown)."),
        toggle("Weapon Stops the Shot", vr_agrunt_shotdisarm_absorb)
            .help("A pellet or a nail on the weapon stops there: it doesn't hurt the grunt. Off: it goes on into the body "
                  "behind (vr_agrunt_shotdisarm_absorb)."),
        slider("Shotgun Pellets", vr_agrunt_shotdisarm_w_pellet, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much the shotguns' pellets count (vr_agrunt_shotdisarm_w_pellet)."),
        slider("Nails", vr_agrunt_shotdisarm_w_nail, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much the nailguns' nails count (vr_agrunt_shotdisarm_w_nail)."),
        slider("Lightning", vr_agrunt_shotdisarm_w_lightning, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much the lightning gun's bolts count, 30 each tenth of a second (vr_agrunt_shotdisarm_w_lightning)."),
        slider("Rockets and Grenades", vr_agrunt_shotdisarm_w_blast, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much a rocket or a grenade flying through the weapon or the hand counts, its direct hit's damage "
                  "(vr_agrunt_shotdisarm_w_blast)."),
        slider("Explosions", vr_agrunt_shotdisarm_w_splash, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much an explosion's damage to the grunt counts on its weapons. 0: never "
                  "(vr_agrunt_shotdisarm_w_splash)."),
        header("Behaviour"),
        toggle("Launchers Fired Safely", vr_agrunt_blast_safe)
            .help("With a grenade or rocket launcher or a proximity gun, it fires only when the round's predicted burst "
                  "(its arc, its bounces, a wall just ahead) is far enough from itself; otherwise it backs off or "
                  "steps aside first, and keeps away from its mines (vr_agrunt_blast_safe)."),
        slider("Careful of Friends", vr_agrunt_friendly_care, 0.f, 1.f, 0.05f, "%.2f")
            .help("How careful it is not to shoot or strike its friends: with another monster in its line of fire (a "
                  "few lines for a shotgun's spread), the chance it holds fire and sidesteps for a clear shot. 1: "
                  "always, 0: never, it fires through them as before (vr_agrunt_friendly_care)."),
        slider("Gives Up After", vr_agrunt_friendly_giveup, 0.f, 10.f, 0.5f, "%.1f s")
            .help("Held so long for friends in the way, it fires anyway (a friend stepping in mid-burst can still be "
                  "hit). 0: it never does (vr_agrunt_friendly_giveup)."),
        toggle("Hands on the Grips", vr_agrunt_ik)
            .help("Its arms are posed so its hand holds the weapon's grip and its other hand the second grip of a "
                  "two-handed weapon, or hangs free beside a one-handed one; melee swings carry the arm "
                  "(vr_agrunt_ik)."),
        open("Grip Editor", pageIndex(pageArmedGrips))
            .help("Tune where each weapon sits in an armed grunt's or enforcer's hand, live, on the one nearest you."),
        header("The Pool"),
        weight("Fists", vr_agrunt_w_fists),
        weight("Axe", vr_agrunt_w_axe),
        weight("Knight's Sword", vr_agrunt_w_sword),
        weight("Crowbar", vr_agrunt_w_crowbar),
        weight("Ogre's Chainsaw", vr_agrunt_w_chainsaw),
        weight("Shotgun", vr_agrunt_w_shotgun),
        weight("Super Shotgun", vr_agrunt_w_ssg),
        weight("Nailgun", vr_agrunt_w_nailgun),
        weight("Super Nailgun", vr_agrunt_w_sng),
        weight("Grenade Launcher", vr_agrunt_w_gl),
        weight("Rocket Launcher", vr_agrunt_w_rl),
        weight("Thunderbolt", vr_agrunt_w_lg),
        weight("Grunt's Burst Rifle", vr_agrunt_w_gruntgun),
        weight("Enforcer's Laser Rifle", vr_agrunt_w_enfrifle),
        weight("Mission Packs' (Each)", vr_agrunt_w_packs),
        header("The Pool: Arcane Dimensions (With Its Data)"),
        weight("Rocket Soldier's Launcher", vr_agrunt_w_adrocket),
        weight("Grenade Soldier's Launcher", vr_agrunt_w_adgrenade),
        weight("Plasma Soldier's Rifle", vr_agrunt_w_adplasma),
        weight("Defender's Shotgun", vr_agrunt_w_addefender),
        weight("Centurion's Plasma Gun", vr_agrunt_w_adcenturion),
        weight("Pyro's Flamethrower", vr_agrunt_w_adpyro),
        weight("Fumigator's Poison Sprayer", vr_agrunt_w_adfumigator),
        weight("Crossbow Knight's Crossbow", vr_agrunt_w_adcrossbow),
        weight("Shadow Axe", vr_agrunt_w_shadowaxe),
        weight("Widowmaker", vr_agrunt_w_widowmaker),
        weight("Plasma Gun", vr_agrunt_w_plasmagun),
        weight("AD Swords (Each)", vr_agrunt_w_adsword),
        weight("Hammer and Mace (Each)", vr_agrunt_w_adheavy),
        weight("Dark Prince's Sword", vr_agrunt_w_dprince),
        weight("Justice Knight's Sword", vr_agrunt_w_justice),
        weight("Judicator's Sword", vr_agrunt_w_judge),
        weight("Phantom Sword (Rogue's Data)", vr_agrunt_w_phantom),
    };
}

[[nodiscard]] za::Vector<Item> pageEnemyWeapons()
{
    return {
        header("Their Damage"),
        open("Weapon Damage", pageIndex(pageWeaponDamage))
            .help("The swords', chainsaws', burst rifles' and laser rifles' damage, with every other weapon's."),
        open("Arcane Dimensions' Guns", pageIndex(pageAdGuns)) // AD support (vr_menu_ad_guns.inc)
            .help("Arcane Dimensions' armed soldiers' own guns: the rocket and grenade launchers, plasma guns, the "
                  "defender's shotgun, the pyro's flamethrower and the fumigator's poison sprayer. Their damage, ammo and "
                  "speed, to balance them."),
        header("Lying About"),
        slider("Most Lying About", vr_enemy_weapon_drop_max, 0.f, 128.f, 4.f, "%.0f").extend(0.f, 512.f)
            .help("The grunts' burst rifles, the enforcers' laser rifles and the ogres' chainsaws lying about: past this "
                  "many, the oldest fades away (never one in your hand, pulled to it or in flight). Every other weapon, "
                  "drop and prop stays. 0: no limit (vr_enemy_weapon_drop_max)."),
        toggle("Spent Guns Stand Out", vr_enemy_weapon_depleted_loot)
            .help("Off: an empty burst rifle, laser rifle or Arcane Dimensions soldier's gun, or an ogre's chainsaw out "
                  "of fuel, lying about (nothing ever refills them) has no loot pillar and the force grab skips it; a hand still picks it up. On: as any "
                  "weapon (vr_enemy_weapon_depleted_loot)."),
        header("Ogres' Chainsaws"),
        slider("Fuel When Dropped", vr_chainsaw_drop_fuel_min, 0.f, 100.f, 5.f, "%.0f%% or more")
            .help("The fuel an ogre's chainsaw has as it drops: at least this much of a full tank, at random up to full. "
                  "It is the chainsaw's own: never refilled."),
        slider("Chain Fuel Use", vr_chainsaw_fuel_use, 0.f, 20.f, 0.5f, "%.1f%% a second").extend()
            .help("Each chainsaw has its own fuel (it goes with it, dropped or holstered): what its chain burns a second, "
                  "the trigger held, of a full tank. Out of fuel, it stalls."),
        slider("Idle Fuel Use", vr_chainsaw_idle_fuel_use, 0.f, 5.f, 0.1f, "%.1f%% a second").extend()
            .help("What the engine burns a second while it runs, the trigger or not."),
        slider("Runs On When Let Go", vr_chainsaw_drop_run, 0.f, 10.f, 0.5f, "%.1f s").extend()
            .help("A running chainsaw dropped or thrown runs on this long as it lies, then dies; taken back meanwhile, it is "
                  "still running (no cord to pull). 0: it stops as it leaves the hand."),
        open("Chainsaw Engine", pageIndex(pageChainsawEngine))
            .help("The running engine's exhaust smoke and its shake: in one hand, in two, lying on the ground."),
        slider("Blade Sinks In", vr_chainsaw_overlap, 0.f, 20.f, 1.f, "%.0f cm").extend()
            .help("How deep the running chain's bar may sink into a monster (it cuts in) before it stops at its surface, as "
                  "weapons do."),
        slider("Start Chance", vr_chainsaw_start_chance, 0.05f, 1.f, 0.05f, "%.2f")
            .help("The chance a good pull of the cord starts the engine: a few pulls may be needed."),
        slider("First Pulls Fail", vr_chainsaw_fail_pulls_min, 0.f, 5.f, 1.f, "%.0f")
            .help("Every start, at least this many good pulls of the cord fail before Start Chance applies (a cold engine "
                  "needs priming)."),
        slider("Up To", vr_chainsaw_fail_pulls_max, 0.f, 5.f, 1.f, "%.0f")
            .help("And at most this many: how many fail is drawn at random between the two each start."),
        slider("Cord Pull Distance", vr_chainsaw_pull_distance, 10.f, 60.f, 1.f, "%.0f cm").extend()
            .help("How far the cord must be pulled out for a pull (the handle taken in the other, empty, hand's grip)."),
        slider("Cord Pull Speed", vr_chainsaw_pull_speed, 0.3f, 4.f, 0.1f, "%.1f m/s").extend()
            .help("How fast the hand must pull it there: a slower pull is only a weak one, and never starts it."),
        header("Grunts' Burst Rifles"),
        toggle("Burst Rifles", vr_grunt_burst)
            .help("On: grunts fire 3-round bursts of single bullets, tighter than a shotgun's pellets, and the gun a "
                  "grunt drops fires the same bursts. Off: id's shotgun, the grunts' and the dropped guns' (six "
                  "pellets as yours, a shell a shot: a third of Rounds)."),
        slider("Grunts' Round Damage", vr_grunt_burst_damage, 1.f, 16.f, 1.f, "%.0f").extend()
            .help("A grunt's burst: each round's damage to you (5: about the shotgun's damage over time; its 4 pellets "
                  "did 4 each)."),
        slider("Rounds", vr_gruntgun_ammo, 3.f, 90.f, 3.f, "%.0f").extend()
            .help("The rounds in a dropped burst rifle, in threes (a burst's): its own, never refilled (shell boxes "
                  "fill your shotguns, not it). Empty, it is still a club, or drop it."),
        header("Enforcers' Laser Rifles"),
        slider("Laser Speed", vr_enfrifle_speed, 600.f, 4000.f, 100.f, "%.0f u/s").extend()
            .help("How fast a dropped rifle's lasers fly, in units a second (the enforcer's own: 600; your nails: "
                  "1750)."),
        slider("Shots", vr_enfrifle_ammo, 1.f, 80.f, 1.f, "%.0f").extend()
            .help("The shots in a dropped rifle: its own, never refilled (cells don't). Empty, it is still a club."),
        header("Spent Rifles"),
        slider("Spent Smoke", vr_enemygun_spent_smoke, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("A burst rifle or a laser rifle whose last round you fire smokes this long, in the hand or dropped: it "
                  "is done, drop it (0: never)."),
        slider("Spent Crackle", vr_enemygun_spent_crackle, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 30.f)
            .help("And crackles all over with lightning's arcs this long, as a corpse the lightning struck (0: never)."),
        slider("Crackle Volume", vr_enemygun_spent_volume, 0.f, 1.f, 0.05f, "%.2f")
            .help("The corpses' crackle as it starts, this loud (0: silent)."),
        slider("Spent Shake", vr_enemygun_spent_shake, 0.f, 1.f, 0.05f, "%.2f units").extend(0.f, 3.f)
            .help("While it crackles in your hand the gun shakes this much, fading with its arcs (0: still)."),
        slider("Spent Haptics", vr_enemygun_spent_haptics, 0.f, 1.f, 0.05f, "%.2f")
            .help("And buzzes in your hand this hard (0: not at all)."),
    };
}

// VR Settings > Combat > Weapon Damage: every weapon's base damage in one place, to balance them (NOTES.md
// vrfiringrange_2026-10-01_02-33; ROUND21.md, "Weapon Damage menu"). id's numbers (or the mission packs') as shipped,
// but the crowbar's 20. A gun's applies to your shots only: monsters and traps firing the same keep id's (QC VR_WpnDmg).
// Damage to Enemies (Damage and Knockback) multiplies them all; a melee blow's is also times its speed and its weight.
[[nodiscard]] za::Vector<Item> pageWeaponDamage()
{
    return {
        header("Guns"),
        slider("Shotgun", vr_dmg_shotgun, 0.5f, 20.f, 0.5f, "%.1f a pellet").extend()
            .help("Each of its 6 pellets (id's 4; a head more). A grunt's dropped shotgun (Burst Rifles off) too."),
        slider("Double Shotgun", vr_dmg_super_shotgun, 0.5f, 20.f, 0.5f, "%.1f a pellet").extend()
            .help("Each of its 14 pellets (id's 4)."),
        slider("Nailgun", vr_dmg_nail, 1.f, 60.f, 1.f, "%.0f a nail").extend().help("id's 9 (a head more)."),
        slider("Super Nailgun", vr_dmg_super_nail, 1.f, 100.f, 1.f, "%.0f a nail").extend().help("id's 18 (a head more)."),
        slider("Grenade", vr_dmg_grenade, 10.f, 400.f, 5.f, "%.0f").extend()
            .help("A grenade's blast at its middle, less further out (id's 120): the launcher's, a hand grenade's and an "
                  "ogre's grenade caught and thrown back."),
        slider("Rocket", vr_dmg_rocket, 10.f, 400.f, 5.f, "%.0f").extend()
            .help("A direct hit, and up to a fifth more at random (id's 100); its blast on what's around 1.2 times it."),
        slider("Lightning Gun", vr_dmg_lightning, 1.f, 100.f, 1.f, "%.0f a bolt").extend()
            .help("Each bolt, 10 a second (id's 30). Its shock fired in water: Lightning Gun in Water."),
        header("Scourge of Armagon"),
        slider("Proximity Gun", vr_dmg_proximity, 10.f, 400.f, 5.f, "%.0f").extend().help("A mine's blast (95)."),
        slider("Laser Cannon", vr_dmg_laser, 1.f, 100.f, 1.f, "%.0f a bolt").extend()
            .help("Each bolt (18; one in ten a third more). A bolt bouncing back into what it hit does half."),
        slider("Mjolnir's Lightning", vr_dmg_mjolnir_lightning, 5.f, 300.f, 5.f, "%.0f").extend()
            .help("Its first strike from the ground (80); the strikes after it 3/8 of it."),
        header("Dissolution of Eternity"),
        slider("Lava Nails", vr_dmg_lava_nail, 1.f, 60.f, 1.f, "%.0f a nail").extend()
            .help("A nail to a monster (15; to a player 3/5 of it)."),
        slider("Super Lava Nails", vr_dmg_super_lava_nail, 1.f, 100.f, 1.f, "%.0f a nail").extend()
            .help("A nail to a monster (30; to a player 3/5 of it)."),
        slider("Multi-Grenade", vr_dmg_multi_grenade, 10.f, 300.f, 5.f, "%.0f a bomblet").extend()
            .help("Each of the 5 bomblets it bursts into (90)."),
        slider("Multi-Rocket", vr_dmg_multi_rocket, 5.f, 300.f, 5.f, "%.0f").extend()
            .help("A rocket's direct hit, and up to a quarter more (60); its blast 1.25 times it."),
        slider("Plasma Gun", vr_dmg_plasma, 5.f, 300.f, 5.f, "%.0f").extend()
            .help("A ball's direct hit, and up to a quarter more (80); its blast 7/8 of it, its arcs 5/8."),
        header("Dawn of the Machine"),
        slider("Super Axe", vr_dmg_superaxe, 1.f, 150.f, 1.f, "%.0f").extend()
            .help("A blow (40, twice the axe's), before its speed and weight; a zombie takes three times it, a blow that "
                  "kills twice it (it gibs). Its lightning: Mjolnir's Lightning. Held when the Dawn of the Machine data is "
                  "there (any campaign: Debug > Tests > Dawn of the Machine Weapons)."),
        slider("Super Axe Burst Window", vr_superaxe_burst_window, 0.3f, 4.f, 0.1f, "%.1f s").extend(0.1f, 10.f)
            .help("A second blow on the same monster within this long of the first fires the Super Axe's lightning burst "
                  "(15 cells; the head glows while it is ready). Dawn of the Machine's own: 0.5 s, too quick for real swings."),
        header("Melee"),
        slider("Fist", vr_dmg_fist, 1.f, 60.f, 1.f, "%.0f").extend()
            .help("A punch (10), before its speed (twice Swing Speed: a full blow) and Melee's Punch Damage Mult."),
        slider("Axe", vr_dmg_axe, 1.f, 100.f, 1.f, "%.0f").extend().help("A blow (id's 20), before its speed and weight."),
        slider("Crowbar", vr_crowbar_damage, 1.f, 100.f, 1.f, "%.0f").extend()
            .help("A blow (20, the axe's: 25 before), before its speed and weight; its hook hits 20% harder, its chisel "
                  "end 40% softer. Crowbars lie in the firing range's prop area."),
        slider("Gun as a Club", vr_dmg_gun_bash, 1.f, 60.f, 1.f, "%.0f").extend()
            .help("A gun swung at something (12), before its speed and weight."),
        slider("Mjolnir", vr_dmg_mjolnir, 1.f, 100.f, 1.f, "%.0f").extend()
            .help("A blow (25), before its speed and weight. Its lightning: above."),
        header("Enemy Weapons"),
        slider("Knight's Sword", vr_sword_damage_mult, 0.5f, 3.f, 0.05f, "%.2fx 20").extend()
            .help("Knights and hell knights always drop their sword: a swing's damage, times 20 (1.5: 30), before its "
                  "speed and weight; the hell knight's sword 25% more."),
        slider("Ogre's Chainsaw Swung", vr_dmg_chainsaw_swing, 1.f, 100.f, 1.f, "%.0f").extend()
            .help("A blow with it, the engine off or on (20), before its speed and its weight (heavy)."),
        slider("Ogre's Chainsaw Chain", vr_chainsaw_damage, 10.f, 300.f, 5.f, "%.0f a second").extend()
            .help("Ogres always drop their chainsaw: its running chain cuts what its bar is in, this much damage a second "
                  "(a grunt has 30 health, an ogre 200). Its fuel and cord: Enemy Weapons."),
        slider("Grunt's Burst Rifle", vr_gruntgun_damage, 1.f, 30.f, 1.f, "%.0f a round").extend()
            .help("Grunts always drop their gun: a trigger pull fires a 3-round burst, each round this much damage (a "
                  "head more; a grunt has 30 health). Burst Rifles off: the Shotgun's pellets."),
        slider("Enforcer's Laser Rifle", vr_enfrifle_damage, 1.f, 60.f, 1.f, "%.0f a shot").extend()
            .help("Enforcers always drop their laser rifle: it fires the enforcer's laser (the enforcer's: 15). As your "
                  "nails, it strikes corpses, gibs, props and breakables too."),
        // AD support: Arcane Dimensions' weapons (QC vr_ad_bridge.qc, vr_ad_enemyguns.qc, vr_ad_melee.qc): the upgrades
        // you find, the soldiers' guns and the melee monsters' weapons they drop (their other numbers: Arcane
        // Dimensions' Guns).
        header("Arcane Dimensions"),
        slider("Shadow Axe", vr_dmg_ad_shadowaxe, 1.f, 150.f, 1.f, "%.0f").extend()
            .help("Arcane Dimensions' axe upgrade: a blow (AD's 35, the axe's 20), before its speed and weight. It gibs "
                  "a zombie in pain for good and chops bodies on the floor."),
        slider("Widowmaker", vr_dmg_ad_widowmaker, 0.5f, 20.f, 0.5f, "%.1f a pellet").extend()
            .help("Arcane Dimensions' super shotgun upgrade, here an auto shotgun with a tube: each pellet of a shot."),
        slider("Widowmaker Pellets", vr_ad_widowmaker_shot_pellets, 1.f, 40.f, 1.f, "%.0f").extend(1.f, 100.f)
            .help("Its pellets a shot, a shell each (the Double Shotgun's 7 a shell; AD's 21 took three shells)."),
        slider("Widowmaker Fire Rate", vr_ad_widowmaker_refire, 0.05f, 1.f, 0.05f, "%.2f s").extend(0.02f, 3.f)
            .help("Seconds between its shots while the trigger is held (the Double Shotgun's 0.7)."),
        slider("Plasma Gun", vr_dmg_ad_plasmagun, 5.f, 300.f, 5.f, "%.0f").extend()
            .help("Arcane Dimensions' lightning gun upgrade: a bolt's direct hit (AD's 45; a map can set its own), a "
                  "cell a bolt, 5 a second."),
        slider("Plasma Gun Blast", vr_dmg_ad_plasmagun_splash, 0.f, 200.f, 5.f, "%.0f").extend().help("AD's 20. 0: none."),
        slider("Plasma Gun Bolt Speed", vr_ad_plasmagun_speed, 200.f, 2500.f, 25.f, "%.0f u/s").extend(100.f, 5000.f),
        slider("Plasma Blast Push", vr_ad_plasma_push, 0.f, 200.f, 5.f, "%.0f").extend()
            .help("A plasma bolt's blast (the Plasma Gun's, the plasma monsters') throws props and bodies as a blast of "
                  "this much damage would (a rocket's 120; Rogue's plasma 70). Its own blast (20) when that is more."),
        slider("Plasma Soldier's Rifle", "vr_adgun_plasma_damage", 0.f, 100.f, 1.f, "%.0f a bolt").extend(0.f, 500.f)
            .help("The plasma soldier's dropped rifle (its blast, speed, bolts: Arcane Dimensions' Guns)."),
        slider("Centurion's Plasma Gun", "vr_adgun_centurion_damage", 0.f, 100.f, 1.f, "%.0f a bolt").extend(0.f, 500.f)
            .help("The centurion's and the eliminator's dropped plasma gun."),
        slider("Defender's Shotgun", "vr_adgun_defender_damage", 0.f, 20.f, 0.5f, "%.1f a pellet").extend(0.f, 100.f),
        slider("Pyro's Flamethrower", "vr_adgun_pyro_damage", 0.f, 30.f, 0.5f, "%.1f a flame").extend(0.f, 200.f)
            .help("Each flame's damage to what it touches; what it touches burns (Arcane Dimensions' Guns)."),
        slider("Fumigator's Poison Sprayer", "vr_adgun_fumigator_damage", 0.f, 30.f, 0.5f, "%.1f a puff").extend(0.f, 200.f)
            .help("Each puff's damage; its poison: Arcane Dimensions' Guns."),
        slider("Crossbow Knight's Crossbow", "vr_adgun_crossbow_damage", 1.f, 60.f, 1.f, "%.0f a bolt").extend(0.f, 200.f),
        slider("Melee Monsters' Swords", "vr_admelee_sword_damage", 0.25f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The death fury's, death knight's, zombie knight's, death guard's, Dark Prince's, justice knight's "
                  "and judicator's dropped swords: times the Knight's Sword's."),
        slider("Hunter Ogres' Hammer and Mace", "vr_admelee_heavy_damage", 0.25f, 4.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("And the Phantom Swordsman's sword: held in both hands, times the Knight's Sword's (one hand's "
                  "share: Arcane Dimensions' Guns)."),
        open("Arcane Dimensions' Guns", pageIndex(pageAdGuns))
            .help("The soldiers' guns' every number: rounds, rate, speed, blast, fuel, reach, poison."),
        header("Thrown"),
        slider("All Throws", "vr_weapon_throw_damage_mult", 0.05f, 5.f, 0.05f, "%.2fx").extend()
            .help("Every throw's damage times this: thrown weapons (from 20 for a gun to 60 for a sword at full speed, "
                  "more the faster it flies), rocks, boxes and gibs. 0.4 (the default; 0.5 from 2026-10-02, "
                  "0.35 from 2026-10-07): 1 is what throws did before 2026-10-02. Each weapon's own: Weapon Weights' "
                  "Throw Damage."),
        slider("Two-Hand Throws", vr_throw_2h_damage, 0.5f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A weapon or prop thrown with both hands: its hits' damage times this (one hand: 1x), on top of All "
                  "Throws. 1.25 as shipped."),
        header("More"),
        open("Damage and Knockback", pageIndex(pageDamage))
            .help("Damage to Enemies (all of these at once), Damage to You, headshots, knockback."),
        open("Weight and Damage", pageIndex(pageWeightDamage)).help("How a held weapon's weight changes its blows."),
    };
}

// VR Settings > Sound: the spatial audio (vr_audio.cpp; ROUND21.md, "Spatial audio (Steam Audio)"). Each feature off at
// 0; without Steam Audio (phonon.dll) only the underwater filter, the hands' and the moving sounds work.
[[nodiscard]] za::Vector<Item> pageSound()
{
    return {
        cycle("Spatial Audio", vr_snd_spatial, {{0.f, "Off"}, {1.f, "In VR"}, {2.f, "Always (headphones)"}})
            .help("Steam Audio over Quake's mixer: the features below. Off: Quake's left/right panning as it always was. "
                  "Always: on the desktop too (for headphones)."),
        header("Direction"),
        toggle("Sounds Around Your Head (HRTF)", vr_snd_hrtf)
            .help("Binaural: each sound shaped as your ears would hear it from where it is, by your head's position and "
                  "turn: in front, behind, above and below, not just left or right. Off: Quake's panning."),
        cycle("HRTF Smoothing", vr_snd_hrtf_interp, {{1.f, "Smooth (Bilinear)"}, {0.f, "Nearest (Cheaper)"}})
            .help("Between the measured directions: blended (moving sounds glide), or the nearest one."),
        cycle("Full-Band Sound", vr_snd_fullband,
            {{0.f, "Off (Quake's 11 kHz)"}, {1.f, "Binaural Sounds"}, {2.f, "All Sounds"}})
            .help("Quake's 11 kHz low-pass (sndspeed 11025, the default) cuts everything above 5.5 kHz, where the "
                  "binaural sounds' cues for ahead, behind, above and below are. Binaural Sounds: those skip it, each "
                  "resampled cleanly (no hiss from Quake's old 11 kHz sounds; the 22 and 44 kHz ones keep their highs). "
                  "All Sounds: the panned ones too. Off: everything through it, as Quake."),
        toggle("HRTF Anti-Aliasing", vr_snd_antialias)
            .help("With Full-Band Sound off and Quake's 11 kHz sound, the binaural sounds are filtered before "
                  "Quake's own low-pass, which keeps every fourth sample and so folds their highs down into the lows: "
                  "off, a sound at your side is far more one-sided than its HRTF and the balance near the front jumps "
                  "about. Off only to compare."),
        slider("HRTF Volume", vr_snd_hrtf_gain, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("The binaural sounds' volume against the others (1.25: as loud as Quake's panning, all round; 1.5 by default: a little louder)."),
        slider("Spatial Voices", vr_snd_voices, 4.f, 64.f, 4.f, "%.0f")
            .help("How many of the loudest sounds are rendered this way at once; the rest are panned as ever."),
        header("Walls and Rooms"),
        slider("Occlusion", vr_snd_occlusion, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 2.f)
            .help("A sound behind a wall or a closed door is quieter and duller (doors and lifts move in it as they move). "
                  "0 off, 1 full; 0.8 keeps monsters behind walls audible."),
        slider("Occlusion Rays", vr_snd_occlusion_samples, 1.f, 32.f, 1.f, "%.0f")
            .help("Rays from each sound (1: hidden or not; more: partly hidden, round corners). On the thread pool."),
        slider("Sound Source Size", vr_snd_occlusion_radius, 0.1f, 2.f, 0.1f, "%.1f m")
            .help("The sphere a sound comes from, for partial occlusion."),
        toggle("Air Absorption", vr_snd_air).help("Far sounds lose their highs in the air."),
        slider("Distance Falloff", vr_snd_falloff, 0.f, 2.f, 0.05f, "%.2fx")
            .help("How fast sounds get quieter with distance: 1 as Quake (a sound gone at about 30 m), 0.75 (the default) "
                  "a bit further, 0.5 twice as far, 0 never quieter. Every sound, spatial audio on or off."),
        slider("Room Reverb", vr_snd_reverb, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 2.f)
            .help("The reverb of the space around your head, simulated from the map (a small room rings short, a big hall "
                  "long): how loud. 0 off."),
        cycle("Reverb Quality", vr_snd_reverb_quality, {{0.f, "Low (Parametric)"}, {1.f, "Medium (Convolution)"}, {2.f, "High (Convolution, Long)"}})
            .help("Low: a reverb driven by the simulated decay times (cheapest, no echoes). Medium: the simulated response "
                  "itself (2 s). High: 3 s from twice the rays."),
        slider("Reverb Update", vr_snd_reverb_interval, 0.1f, 1.f, 0.05f, "%.2f s").extend(0.05f, 5.f)
            .help("Seconds between the room's simulations (on the thread pool)."),
        slider("Underwater Muffle", "snd_waterfx", 0.f, 2.f, 0.1f, "%.1f")
            .help("Sounds muffled while your head is in water, slime or lava (your head's place, not your body's). 0 off."),
        header("Movement and Nearness"),
        toggle("Weapons From Your Hands", vr_snd_hands)
            .help("Your guns' shots, your blows and what your hands do (a parry, a reload, drawing and holstering, a dry "
                  "click, the grenade pouch, a chainsaw's cord) sound from the hand doing it, not from the middle of your "
                  "head."),
        toggle("Sounds Follow Things", vr_snd_follow)
            .help("A sound stays with what made it as it moves (a monster, a door, a hook in flight)."),
        slider("Doppler", vr_snd_doppler, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 4.f)
            .help("Things coming at you sound higher, going away lower (1: as physics has it; 0 off)."),
        slider("Pitch Variation", vr_snd_pitch_jitter, 0.f, 10.f, 1.f, "%.0f%%").extend(0.f, 25.f)
            .help("The most frequent sounds (punches and blows, swings, squishes, clicks, knocks, casings, footsteps) "
                  "each a little higher or lower at random, up to this much, so that one recording isn't heard the same "
                  "every time (0 off)."),
        slider("Blow Material Layer", vr_snd_hit_layer, 0.f, 1.f, 0.1f, "%.1f")
            .help("A punch, a gun's butt, a headbutt or a thrown thing landing also sounds of what it hit: a slap of "
                  "flesh on a body, a clink of armour on a knight or an enforcer, a knock of wood on a crate, your "
                  "knuckles' knock on a wall. How loud, under the blow's thud (0 none)."),
        slider("Near Field", vr_snd_nearfield, 0.f, 2.f, 0.1f, "%.1f")
            .help("A sound within a metre of your head: the nearer ear louder, the farther one quieter and duller, by how "
                  "near and how much to the side. 0 off."),
        header("Advanced"),
        toggle("Mix Limiter", vr_snd_limiter)
            .help("Loud sounds piling up (explosive boxes blowing up together) are brought down smoothly just under full "
                  "volume instead of clipping, which crackles; quieter sounds untouched. Spatial audio on or off. Off: "
                  "Quake's hard clip, 6 dB lower."),
        slider("Limiter Ceiling", vr_snd_limiter_ceiling, -6.f, 0.f, 0.5f, "%.1f dB").extend(-12.f, 0.f)
            .help("The loudest the mix gets, under full volume (-1 by default; -6: Quake's clip, quieter explosions)."),
        cycle("Frame Size", vr_snd_frame, {{256.f, "256 (5.8 ms)"}, {512.f, "512"}, {1024.f, "1024 (Cheapest)"}})
            .help("Samples a voice renders at a time: larger is cheaper, a moving sound's direction coarser."),
        command("Spatial Audio Info", "vr_snd_info")
            .help("vr_snd_info: in the console, Steam Audio loaded or not, the voices, the scene and the simulations' times."),
    };
}

// Split from Gameplay: damage multipliers, positional damage and knockback.
[[nodiscard]] za::Vector<Item> pageBloodFeedback()
{
    return {
        header("When Hit"),
        toggle("Hit Blood Vignette", vr_blood_hit)
            .help("Blood-red edges after a hit, including armor-only damage. The centre stays clear. Independent of Screen Tints and low-health pulses."),
        slider("Hit Duration", vr_blood_hit_duration, 0.f, 2.f, 0.05f, "%.2f s").extend(0.f, 5.f)
            .help("Seconds until a hit fades away. 0: no hit effect. Pausing freezes it."),
        slider("Hit Intensity", vr_blood_hit_intensity, 0.f, 0.8f, 0.02f, "%.2f")
            .help("Strongest opacity of a hit. Hits taking less than 20 health plus armor are lighter. Overlapping hits remain bounded."),
        slider("Hit Coverage", vr_blood_hit_size, 0.f, 1.f, 0.05f, "%.2f")
            .help("How far the hit's red edges reach into your view. Larger covers more area without changing intensity or direction. 0: none; 1: widest. A clear centre always remains. Independent of Health Coverage."),
        slider("Directional Bias", vr_blood_hit_bias, 0.f, 1.f, 0.05f, "%.2f")
            .help("0: equal on every edge. 1: strongest towards the source, weakest opposite. Front: upper edge; rear: lower; above/below: upper/lower. Turns follow the world direction. Unknown, near-player and known self/environment hits are even."),
        header("Low Health"),
        toggle("Low Health Blood Vignette", vr_blood_health)
            .help("Blood-red edges pulse below the health threshold. Losing health makes them stronger and more frequent; healing eases them. Independent of hits."),
        slider("Health Threshold", vr_blood_health_threshold, 1.f, 100.f, 1.f, "%.0f").extend(1.f, 200.f)
            .help("Pulses below this health, from the local player's actual health stat. None at or above it, or while dead."),
        slider("Health Intensity", vr_blood_health_intensity, 0.f, 0.8f, 0.02f, "%.2f")
            .help("Peak opacity as health approaches zero. The centre stays clear. Both blood effects together never exceed 0.8 opacity."),
        slider("Health Coverage", vr_blood_health_size, 0.f, 1.f, 0.05f, "%.2f")
            .help("How far low-health pulses reach into your view. Larger covers more area without changing intensity or pulse timing. 0: none; 1: widest. A clear centre always remains. Independent of Hit Coverage."),
        slider("Pulse Duration", vr_blood_health_duration, 0.05f, 2.f, 0.05f, "%.2f s")
            .help("Width of each beat. Independent of rate: wide beats overlap smoothly when pulses become frequent."),
        slider("Pulse Rate", vr_blood_health_rate, 0.1f, 3.f, 0.05f, "%.2f Hz")
            .help("Beats per second near the threshold. Low-health Rate Scale increases this as health falls, up to 6 Hz."),
        slider("Low-health Rate Scale", vr_blood_health_rate_scale, 0.f, 4.f, 0.1f, "%.1f")
            .help("Rate is Pulse Rate times (1 + severity times this). 0: constant rate; 2: up to three times as frequent."),
        slider("Health Scaling Curve", vr_blood_health_curve, 0.25f, 4.f, 0.05f, "%.2f")
            .help("Severity is the missing share below the threshold, raised to this power. 1: linear; above 1: gentler near the threshold; below 1: stronger sooner. Both opacity and rate use severity."),
    };
}

[[nodiscard]] za::Vector<Item> pageDamage()
{
    return {
        header("Hit Detection"),
        toggle("Precise Hit Detection", vr_hit_precise)
            .help("Shots, projectiles, the grappling hook, melee blows and thrown things hit a monster's model as you see "
                  "it, not the big box round it: aim at the body. Off: Quake's boxes."),
        slider("Guns Tolerance", vr_hit_tolerance_guns, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to shots and projectiles: a near miss still hits."),
        slider("Grappling Hook Tolerance", vr_hit_tolerance_grapple, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to the grappling hook."),
        slider("Melee Tolerance", vr_hit_tolerance_melee, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to your melee blows and shoves."),
        slider("Melee Hitbox Size (of Melee Tolerance)", vr_hit_melee_scale, 0.25f, 4.f, 0.05f, "%.2fx").extend()
            .help("The melee probe's size: Melee Tolerance times this, so a monster's model grown by that many units is "
                  "what a blow meets (1: 6 units; 0.25: 1.5; 4: 24). It does not change the striking points' own 3-unit "
                  "thickness, nor the box the model is found out of, nor how far a swing reaches."),
        toggle("Head Priority", vr_hit_head_priority)
            .help("Where a hit point, or a whole melee sweep's way through a body, lies in more than one zone, the "
                  "head's zone decides it. Off: the limb and leg zones claim it first."),
        slider("Thrown Tolerance", vr_hit_tolerance_thrown, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to things you throw."),
        header("Damage"),
        slider("Damage to Enemies", vr_damage_to_enemies, 0.25f, 4.f, 0.05f, "%.2fx").extend().help("Damage you deal to monsters."),
        slider("Damage to You", vr_damage_to_player, 0.f, 4.f, 0.05f, "%.2fx").extend().help("Damage monsters, traps and falls deal to you."),
        slider("Self Damage", vr_damage_self, 0.f, 2.f, 0.05f, "%.2fx").extend().help("Damage your own rockets and grenades deal to you."),
        header("Positional Damage"),
        toggle("Positional Damage", vr_positional_damage).help("Headshots, arm and leg shots on humanoid monsters."),
        toggle("Melee Positional Damage", vr_melee_positional)
            .help("Your melee blows take the same head / limb / leg multipliers as your shots, from the point on the "
                  "model they met: a head is a head for a blade and for a shot. Off: flat melee damage, as before. The "
                  "looser melee tolerance stays what connects only."),
        slider("Headshot Damage", vr_headshot_mult, 1.f, 5.f, 0.1f, "%.1fx").extend(),
        slider("Arm Shot Damage", vr_limbshot_mult, 0.1f, 1.f, 0.05f, "%.2fx").extend(),
        slider("Leg Shot Damage", vr_legshot_mult, 0.1f, 1.f, 0.05f, "%.2fx").extend(),
        slider("Headshot Sound", vr_headshot_sound, 0.f, 1.f, 0.1f, "%.1f").help("Volume of the crack you hear when you land a headshot (0 off)."),
        header("Knockback"),
        slider("Knockback", vr_push, 0.f, 2.f, 0.05f, "%.2fx").extend().help("All knockback; the settings below scale this."),
        slider("Your Melee Hits", vr_melee_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far your melee blows (and headbutts) push what they hit. A ragdoll or a corpse moves by mass instead (below)."),
        slider("Blows on Ragdolls: Bounce", vr_melee_ragdoll_bounce, 0.f, 1.f, 0.05f, "%.2f").extend().help("A blow or a weapon meeting a body on the floor, knocked down or staggering moves it as two masses meeting: the weapon's (and the arm's behind it) against the body's at the part struck. How lively it comes off: 0 dead, 1 elastic."),
        slider("Blows on Ragdolls: Most", vr_melee_ragdoll_most, 0.f, 15.f, 0.5f, "%.1f m/s").extend().help("The fastest a blow sends the part of a ragdoll it strikes. 0: no cap."),
        slider("Weapon Hits", vr_hit_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far heavy weapon hits shove monsters."),
        slider("Killing Blows", vr_kill_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far killing blows and explosions throw the bodies."),
        slider("Parry Pushes Enemy", vr_parry_push_enemy, 0.f, 3.f, 0.05f, "%.2fx").extend()
            .help("How far a parry pushes the attacker back (Parry's Parry Pushback, the same setting)."),
        slider("Parry Pushes You", vr_parry_push_player, 0.f, 3.f, 0.05f, "%.2fx").extend(),
        slider("Monsters' Blows Push You", vr_melee_push_player, 0.f, 3.f, 0.05f, "%.2fx").extend(),
        header("When You're Hit"),
        toggle("Hits Knock Your Hands", vr_pain_knock)
            .help("Being hit knocks your hands, arms and what they hold away from where the hit came from, more the "
                  "hand on that side, and they ease back. Looks only: your aim and your shots don't move."),
        slider("Knock per Damage", vr_pain_knock_strength, 0.f, 1.f, 0.05f, "%.2f cm").extend(0.f, 5.f)
            .help("How far the hands are knocked for each point of damage, for small hits (a grunt's volley: about 16); "
                  "harder ones curve towards Largest Knock."),
        slider("Largest Knock", vr_pain_knock_max, 0.f, 15.f, 0.5f, "%.1f cm").extend(0.f, 50.f)
            .help("How far the hardest hits knock the hands (a rocket nearly that far; a harder hit always a little "
                  "further; hits in quick succession add up to no more). Away from the hit and up."),
        slider("Knock Tip", vr_pain_knock_tip, 0.f, 5.f, 0.25f, "%.2f deg/cm").extend(0.f, 20.f)
            .help("How much the hands and what they hold tip up for each cm they are knocked (a gun's barrel kicks up), "
                  "about the wrist."),
        toggle("Knock Across Your View", vr_pain_knock_seen)
            .help("On: the knock's size is what you see, across your view (a hit from ahead throws the hands up and "
                  "towards you, from the side sideways). Off: straight away from the hit and up, which for hands held "
                  "ahead of and below your eyes is mostly towards your eyes, where it barely shows."),
        slider("Knock Time", vr_pain_knock_time, 0.1f, 1.5f, 0.05f, "%.2f s").extend(0.05f, 3.f)
            .help("How long the knock takes: out quickly, held a moment, then eased back."),
        slider("Hit Buzz", vr_pain_haptics, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("How hard the controllers buzz when you're hit: more the harder the hit, and in the hand on the side it "
                  "came from (0: none)."),
    };
}

// Blocking and shoving with a guard, and counter-attacks after a parry (the parry settings of Melee joined them).
[[nodiscard]] za::Vector<Item> pageParryBash()
{
    return {
        header("Parry"),
        toggle("Parry", "vr_parry").help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block monsters' melee blows."),
        slider("Parry Angle", vr_parry_angle, 15.f, 80.f, 5.f, "%.0f deg").extend()
            .help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block a monster's melee blow: how far it may be tilted off level."),
        slider("Parry Reach", vr_parry_reach, 0.5f, 2.5f, 0.1f, "%.1f m").extend().help("How far in front of you a held weapon still parries."),
        slider("Parry Stagger", vr_parry_stagger, 0.1f, 1.5f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("With Parry Balance off: how long the monster pauses after a successful parry before it can move and "
                  "attack again."),
        toggle("Parry Stagger as Ragdoll", vr_parry_ragdoll).extend()
            .help("A parried humanoid (its monster group's Ragdoll Triggers on: Combat > Monster Groups) staggers back as an active ragdoll, as a shove's stagger, for Parry Stagger; on level ground it never falls, with stairs, a ledge or a slope behind it it may go down. Off: its pain animation's stagger (Stagger Frames, Stagger Sway)."),
        slider("Stagger Frames", vr_parry_stagger_frames, 1.f, 4.f, 1.f, "%.0f")
            .help("How many frames of its pain animation a parried monster steps through (a tenth of a second each), "
                  "then rocks back and forth over the last two until it recovers. 1: it holds its first pain pose."),
        slider("Stagger Sway", vr_parry_stagger_sway, 0.f, 10.f, 0.5f, "%.1f deg").extend(0.f, 15.f)
            .help("How far a parried monster's body sways, dazed, while it is staggered. 0: none."),
        slider("Parry Pushback", vr_parry_push_enemy, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("How far a parry pushes the attacker back (times Knockback; the distance goes about as its square: "
                  "0.55 about half as far as 0.8). More makes room; less keeps it within reach of a counter-attack. "
                  "Separate from shoves (Shove and Bash Push below)."),
        slider("Parry Damage Reduction", "vr_parry_reduction", 0.f, 1.f, 0.05f, "%.2f").help("Share of a parried blow's damage taken away."),
        slider("Parry Drop Chance", "vr_parry_drop_chance", 0.f, 1.f, 0.05f, "%.2f").help("Chance a one-handed parry knocks the weapon out of your hand (two hands: never). Not used with Parry Stamina on (Parry, Bash and Headbutt), which replaces it."),
        slider("Parry Arm Knock", "vr_parry_wobble", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much a parried blow knocks your hand and arm."),
        toggle("Unarmed Parry", vr_parry_unarmed).help("Cross your arms in an X in front of you to block a blow with your forearms."),
        slider("Unarmed Parry Reduction", vr_parry_unarmed_reduction, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Parry Cooldown", vr_parry_cooldown, 0.f, 1.5f, 0.05f, "%.2f s")
            .help("Per monster: a blow parried within this long of its last parried blow is still parried (its damage cut, "
                  "both pushed apart) "
                  "but makes no sound or sparks, opens no new counter window (the first one runs on) and costs no "
                  "stamina: one parry for an attack of quick hits (the ogre's chainsaw, a knight's swing). Its next "
                  "attack is a full parry again. 0: every blow a full parry."),
        toggle("Parry Cooldown: Whole Attack", vr_parry_cooldown_attack).extend()
            .help("Every blow of the attack whose blow you last parried is a cooldown blow, however long between them "
                  "(a fiend's two claws, the Overlord's double smash, a shambler's chained swings): one parry and one "
                  "stamina cost per attack. Off: only the Parry Cooldown's time."),
        header("Parry Balance"),
        toggle("Parry Balance", vr_parry_balance)
            .help("A parry's strength (its tool, times its grip, times its timing) over the monster's Parry Poise (Monster "
                  "Groups) decides what it does: under Blocked Below the blow is only blocked (its damage cut, its attack "
                  "goes on), under 1 it flinches, up to 2 it staggers back as a ragdoll, more a longer stagger that may "
                  "take it down stairs or a slope. Off: every parry staggers it for Parry Stagger."),
        slider("Bare Hands", vr_parry_tool_hand, 0.f, 2.f, 0.05f, "%.2f").extend(0.f, 5.f)
            .help("A parry's strength with crossed arms (the grip's two hands times this)."),
        slider("Weapon", vr_parry_tool_weapon, 0.f, 2.f, 0.05f, "%.2f").extend(0.f, 5.f)
            .help("A parry's strength with any weapon (a light one)."),
        slider("Heavy Weapon", vr_parry_tool_heavy, 0.f, 3.f, 0.05f, "%.2f").extend(0.f, 5.f)
            .help("A parry's strength with the heaviest weapon (Heft 1, Weapon Weights); lighter ones by their Heft."),
        slider("Two Hands", vr_parry_grip_2h, 1.f, 3.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A parry with a weapon held in two hands, or the arms crossed, times this (one hand: 1)."),
        slider("Well Timed", vr_parry_timed, 1.f, 3.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A well-timed parry times this: the guard raised just before the blow (Timing Window), not held up all "
                  "along."),
        slider("Timing Window", vr_parry_timed_window, 0.05f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("A parry is well timed when its guard (the parry pose, facing ahead) came up at most this long before "
                  "the blow."),
        slider("Blocked Below", vr_parry_block_below, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 3.f)
            .help("Strength over poise under this: the blow is only blocked (no stagger: its attack goes on)."),
        slider("Flinch", vr_parry_flinch, 0.1f, 1.f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("Strength over poise from Blocked Below to 1: it flinches this long (its pain pose, no ragdoll)."),
        slider("Stagger at 1", vr_parry_stagger_min, 0.1f, 2.f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("Strength over poise 1: it staggers back as a ragdoll this long (to Stagger at 2 at 2)."),
        slider("Stagger at 2", vr_parry_stagger_max, 0.1f, 2.f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("Strength over poise 2: its ragdoll stagger. Up to 2 it never goes down."),
        slider("Heavy Stagger", vr_parry_stagger_heavy, 0.1f, 3.f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("Strength over poise 3 and over: its stagger (from Stagger at 2 over 2); over 2 it may go down stairs, "
                  "a ledge or a slope behind it."),
        slider("Repeat Stagger", vr_parry_repeat, 0.05f, 1.f, 0.05f, "%.2fx")
            .help("Each stagger of the same monster within Repeat Window of its last lasts this much of what it would "
                  "(again for each in a row); a heavy group (poise over 1) to the power of its poise: it recovers faster."),
        slider("Repeat Window", vr_parry_repeat_window, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("How soon after its last stagger a monster's next one is shorter (0: never)."),
        slider("Poise Against Melee", vr_poise_chances, 0.f, 2.f, 0.1f, "%.1f").extend(0.f, 5.f)
            .help("A heavy group (Parry Poise over 1; Off as 3): your shoves', blows' and flung props' chances to knock "
                  "it down or unbalance it divided by its poise to this power, so weapons and explosives stay the "
                  "answer to ogres and the like (0: not)."),
        header("Bash and Shove"),
        toggle("Bash", vr_bash).help("The parry stance (a weapon level across in front, one hand or two) pushed straight forward bashes: knocks monsters back and staggers them. Open palms facing a monster pushed at it shove it. Two hands (a weapon held two-handed, a palm pushing on the blade, both palms) push harder and further."),
        slider("Bash Speed", vr_bash_speed, 0.3f, 3.f, 0.1f, "%.1f m/s").extend().help("How fast the stance (a weapon level across, held half a second) must be pushed forward, both its ends going ahead. A swing passing through the stance doesn't bash."),
        slider("Shove Speed", vr_shove_speed, 0.8f, 5.f, 0.1f, "%.1f m/s").extend().help("How fast open palms (facing ahead, not holding anything) must be pushed out, the arm extending, to shove. Hands waved or patted at a monster don't shove."),
        slider("Bash Damage", vr_bash_damage, 0.f, 40.f, 1.f, "%.0f").extend(),
        slider("Shove and Bash Push", vr_bash_push, 0.f, 3.f, 0.05f, "%.2fx").extend()
            .help("How far a shove or a bash throws what it hits (times Knockback), then times the hands' push below. "
                  "Knocked down by it, it goes Knockdowns' Travel of that."),
        slider("One-Hand Push", vr_shove_push_onehand, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A one-handed shove's or bash's push, times Shove and Bash Push."),
        slider("Two-Hand Push", vr_shove_push_twohand, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A two-handed shove's or bash's push (both palms, or a weapon held in two hands), times Shove and "
                  "Bash Push. Its distance goes by the push's square (1.12: 1.25 times as far as 1)."),
        slider("Running Shove Carry", vr_shove_run_carry, 0.f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("Shoving or bashing while you run: this share of your speed along the shove is added to its push (by "
                  "the monster's Shove Push over its weight, at most all of it), so it flies farther and faster. "
                  "Standing shoves are unchanged. 0: off."),
        slider("Bash and Parry Sounds", vr_bash_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the sounds that tell a shove, a weapon bash, a counter bash (a bash right after a parry) and a parry apart from your blows (0: silent)."),
        header("Counter-Attacks"),
        toggle("Counter-Attacks", vr_counter)
            .help("After a parry (a weapon's or crossed arms), your next melee attack in the window is a counter and hits harder: a blow with either hand, a bash or a shove. One a parry."),
        slider("Counter Window", vr_counter_window, 0.25f, 4.f, 0.05f, "%.2f s").extend().help("How long after a parry a counter may land."),
        slider("Counter Damage", vr_counter_damage, 1.f, 3.f, 0.05f, "%.2fx").extend().help("A counter's damage (a bash's or a shove's knockback too)."),
        slider("Counter Sounds", vr_counter_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of a blade's shing as the window opens and of the heavy strike as a counter lands (0 off)."),
        toggle("Counter Window Glow", vr_counter_glow)
            .help("While the counter's window is open, what your hands hold (a bare fist too) glows gold round its edges and sheds golden embers, fading as the window closes. Off as shipped."),
        slider("Counter Pulses", vr_counter_haptic, 0.f, 1.f, 0.1f, "%.1f")
            .help("Strength of the soft pulses in both hands while the window is open, fading as it closes (0 off)."),
        header("Training Dummy Attacks"),
        slider("Time Between Blows", vr_dummy_attack_period, 1.f, 8.f, 0.1f, "%.1f s").extend()
            .help("With the firing range's Dummy Attacks button on, the training dummy strikes at you this often, to practise parrying."),
        slider("Randomness", vr_dummy_attack_jitter, 0.f, 2.f, 0.05f, "%.2f s").extend()
            .help("How much sooner or later than that each blow may come, at random."),
        slider("Wind-Up", vr_dummy_attack_windup, 0.1f, 2.f, 0.05f, "%.2f s")
            .help("How long before each blow the dummy gives it away: a sound, a glow, the rifle raised as it rears back."),
        slider("Reach", vr_dummy_attack_reach, 16.f, 150.f, 2.f, "%.0f units")
            .help("How close you must be for it to strike (a knight's reach is 60). Further away it waits; step out of reach during the wind-up and it misses."),
        slider("Damage", vr_dummy_attack_damage, 0.f, 50.f, 1.f, "%.0f").extend()
            .help("A blow's damage, as a monster's: armour, the parry and god mode apply."),
    };
}

// Split from Parry, Bash and Headbutt: the stamina parries, shoves and blows cost.
[[nodiscard]] za::Vector<Item> pageStamina()
{
    return {
        header("Parry Stamina"),
        toggle("Parry Stamina", vr_parry_stamina)
            .help("Parrying tires you: each parry with a weapon costs stamina (less with the weapon in two hands), and the parry that leaves you none knocks the weapon out of your hand. It comes back once you stop parrying for a moment. On, it replaces the Parry Drop Chance (Melee Settings). Crossed arms cost nothing."),
        slider("Stamina", vr_parry_stamina_max, 20.f, 300.f, 5.f, "%.0f").extend().help("Your stamina when rested. Both hands share it."),
        slider("One-Handed Parry Cost", vr_parry_stamina_cost, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a parry with the weapon in one hand costs (30 of 100: the fourth in a row knocks it away)."),
        slider("Two-Handed Parry Cost", vr_parry_stamina_cost_2h, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a parry with the weapon in both hands costs (12 of 100: the ninth in a row knocks it away)."),
        slider("Rest Before Recovering", vr_parry_stamina_delay, 0.f, 6.f, 0.25f, "%.2f s").extend()
            .help("How long you must go without parrying, shoving, striking or hanging from a hold (those that cost stamina) before stamina starts coming back."),
        slider("Recovery Rate", vr_parry_stamina_regen, 1.f, 100.f, 1.f, "%.0f /s").extend().help("Stamina a second it then comes back at."),
        slider("Tiring Warning", vr_parry_stamina_warn, 0.f, 1.f, 0.1f, "%.1f")
            .help("A breath and a throb in the hand when one more one-handed parry would knock the weapon away; a gasp and a long buzz when it does: their volume and strength (0 off)."),
        open("Stamina on the Gadget: Screens", pageIndex(pageScreens)).help("What the gadget shows (stamina and counters) is on HUD and Menus > Screens."),
        header("Shove and Strike Stamina"),
        toggle("Shove Stamina", vr_shove_stamina)
            .help("Shoves and bashes that land cost stamina from the same pool as parries. Without enough left, they throw back less and hurt less (Exhausted Knockback, Exhausted Damage)."),
        slider("One-Handed Shove Cost", vr_shove_stamina_cost, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a shove or bash with one hand costs (15 of 100)."),
        slider("Two-Handed Shove Cost", vr_shove_stamina_cost_2h, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a shove or bash with both hands costs (20 of 100: it pushes further than a one-handed one)."),
        toggle("Strike Stamina", vr_strike_stamina)
            .help("Blows that land on something that takes damage cost stamina from the same pool, once a swing, however many things it goes through. Without enough left, they hurt less (Exhausted Damage). Blows on walls cost nothing."),
        slider("Punch Cost", vr_strike_stamina_punch, 0.f, 100.f, 1.f, "%.0f").extend().help("Stamina a punch costs (4 of 100)."),
        slider("Weapon Strike Cost", vr_strike_stamina_cost, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a blow with a weapon held in one hand costs: a sword, an axe, Mjolnir, a gun, their pommel or butt too (8 of 100)."),
        slider("Two-Handed Strike Cost", vr_strike_stamina_cost_2h, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a blow with a weapon held in both hands costs (6 of 100: two arms share the work)."),
        slider("Exhausted Damage", vr_stamina_exhausted_damage, 0.1f, 1.f, 0.05f, "%.2fx")
            .help("Damage of a blow, shove or bash made with no stamina left for it (1: no penalty). With part of its cost left, in between."),
        slider("Exhausted Knockback", vr_stamina_exhausted_push, 0.1f, 1.f, 0.05f, "%.2fx")
            .help("How far a shove or bash made with no stamina left for it throws back and staggers (1: no penalty). With part of its cost left, in between."),        header("Climbing Stamina"),
        toggle("Climbing Stamina", vr_climb_stamina)
            .help("Hanging from a ledge or a rung (Climbing) spends stamina from the same pool, and none comes back while you hang. With none left your hands let go. More on the Climbing page."),
        slider("Hanging Cost, One Hand", vr_climb_stamina_rate, 0.f, 30.f, 0.5f, "%.1f /s").extend()
            .help("Stamina a second hanging from one hand costs (5 of 100: 20 s)."),
        slider("Hanging Cost, Two Hands", vr_climb_stamina_rate_2h, 0.f, 30.f, 0.5f, "%.1f /s").extend()
            .help("Stamina a second hanging from both hands costs, the two together (2 of 100: 50 s)."),
        header("Tired Arms"),
        slider("Shake From Stamina", vr_fatigue_shake_from, 0.f, 1.f, 0.05f, "%.2f").help("Below this share of your stamina your arms shake while you hang, more the less is left. 0: never. Looks only: your aim and shots stay steady."),
        slider("Shake Distance", vr_fatigue_shake, 0.f, 2.f, 0.1f, "%.1f cm").extend(0.f, 10.f).help("How far your drawn hands, arms and what they hold shake at most, with no stamina left. 0: no moving shake."),
        slider("Shake Turn", vr_fatigue_shake_angle, 0.f, 5.f, 0.25f, "%.2f deg").extend(0.f, 20.f).help("How far they turn as they shake at most, with no stamina left. 0: no turning shake."),
        slider("Shake Speed", vr_fatigue_shake_speed, 0.25f, 2.f, 0.05f, "%.2fx").extend(0.05f, 5.f).help("How quick the tremor is (1: 5 to 13 shakes a second)."),
        toggle("Shake Whenever Tired", vr_fatigue_shake_always).help("Off: your arms shake only while you hang from a ledge or a rung. On: whenever your stamina is low (after parries, shoves and blows too)."),
        header("Tired Legs"),
        toggle("Slower When Tired", vr_stamina_speed)
            .help("As your stamina runs low, you can't run as fast: full speed from half your stamina (From Stamina, on the Aiming page's Tired Arms) down to Slowest Run with none left, on the same curve as the weight."),
        slider("Slowest Run", vr_stamina_speed_min, 0.1f, 1.f, 0.05f, "%.2fx")
            .help("Times your most running speed with no stamina left (0.5: half; 1: never slower)."),
        open("Heavy When Tired (Aiming)", pageIndex(pageAimingSettings)).help("Tired hands get heavy too (what they hold, and an empty hand): Tired Arms, on the Aiming page."),
    };
}

// Batting projectiles back (Parry, Bash and Headbutt) and catching grenades (Carrying and Gibs).
[[nodiscard]] za::Vector<Item> pageBatting()
{
    return {
        header("Batting Projectiles"),
        toggle("Bat Back Projectiles", vr_deflect).help("Swing a weapon (a gun, a melee weapon, or a club: a wall torch, a brick) through a monster's spike, laser, spit or grenade to bat it away as a bat hits a ball: off the weapon's face and the way it swings, faster the harder you swing. A bash sends it the way you push. Empty hands never bat: they catch grenades."),
        slider("Batting Reach", vr_deflect_radius, 4.f, 32.f, 1.f, "%.0f units").extend().help("How near the weapon's blade, barrel or club a projectile must pass to be batted back."),
        slider("Batting Swing Speed", vr_deflect_speed, 0.2f, 1.5f, 0.05f, "%.2fx").extend().help("How fast a batting swing must be, times Swing Speed (a hit needs 1x, and more for a swung weapon)."),
        slider("Batting Timing", vr_deflect_window, 0.f, 0.5f, 0.05f, "%.2f s").extend().help("How early you may swing: the weapon's path keeps batting this long after it passed."),
        toggle("Batting Needs a Swing", vr_deflect_swing).extend().help("Only a real swing bats: the hand itself moving fast (Batting Hand Speed), from some way off, not a weapon waved back and forth. Off: any weapon moving fast enough at its end bats, a turn of the wrist or a gun brought round to aim too."),
        slider("Batting Hand Speed", vr_deflect_wrist, 0.5f, 4.f, 0.1f, "%.1f m/s").extend().help("How fast the hand itself must move to bat (its turn taken out: a flick of the wrist moves the weapon's end, not the hand)."),
        slider("Batting Swing Length", vr_deflect_run, 0.f, 0.6f, 0.05f, "%.2f m").extend().help("How far a batting swing must have come from."),
        slider("Bash Batting Reach", vr_bash_deflect_radius, 4.f, 48.f, 1.f, "%.0f units").extend()
            .help("A bash (a weapon or a club held across, pushed) bats back projectiles that pass this near the weapon. A shove with open hands bats nothing."),
        slider("Bash Batting Timing", vr_bash_deflect_window, 0.f, 1.f, 0.05f, "%.2f s").extend().help("How long a bash goes on batting after the push."),
        slider("Batting Bounce", vr_deflect_bounce, 0.f, 1.f, 0.05f, "%.2f")
            .help("How lively a batted projectile comes off the weapon: 0 dead (it takes only the swing's speed), 1 as a rubber ball (a ball off a bat is about 0.5). A swing across sends it off to the side; the weapon's face driven at the thrower sends it back, the harder the faster."),
        slider("Batting Aim Assist", vr_deflect_aim_assist, 0.f, 1.f, 0.05f, "%.2f")
            .help("A batted or bashed projectile going very near a monster (within a few degrees; its thrower a little more) bends this much of the way towards it. 0: only your swing aims."),
        header("Grenades"),
        cycle("Catch Grenades", vr_grenade_catch, {{0.f, "Off"}, {1.f, "Ogres'"}, {2.f, "Ogres' and yours"}})
            .help("Grenades bounce and roll as physics objects, and you can catch them (by hand or force grab) and throw them "
                  "back; thrown, they burst on a monster as an ogre's does on you. \"And yours\" (the default): your grenade "
                  "launcher's too."),
        slider("Held Grenade Fuse", vr_grenade_held_fuse, 1.f, 5.f, 0.1f, "%.1f s")
            .help("A grenade you catch fizzes and ticks, and goes off this long after (it never shortens the fuse). Hold it too "
                  "long and it goes off in your hand."),
        toggle("Fuse Resets Every Catch", vr_grenade_fuse_regrab)
            .help("Off: the fuse is set once, as you catch the monster's throw; dropped and caught again, it keeps running. On: "
                  "every catch sets it again."),
        slider("Catch Radius", vr_grenade_catch_radius, 5.f, 30.f, 1.f, "%.0f cm")
            .help("How near your palm a grenade in flight must pass for an empty hand to catch it: grip held, or closing as it "
                  "arrives. A punching or shoving hand doesn't catch it (nor bat it: only a weapon does)."),
        slider("Catch Window", vr_grenade_catch_window, 0.f, 0.4f, 0.01f, "%.2f s")
            .help("A grenade that flies into your open palm stays there this long for your grip to close on it; then it drops "
                  "from the hand. 0: only a grip already closing catches."),
        toggle("Returned Grenades Hit Like Yours", vr_grenade_return_full)
            .help("A grenade you throw or bat back (an ogre's or your own) goes off as your grenade launcher's: its damage and "
                  "radius, credited to you, with your Quad. Off: an ogre's keeps its own, weaker blast."),
        toggle("Shoot Grenades", vr_grenade_shoot)
            .help("A shot, a nail, a rocket, the lightning, a thrown thing or another blast sets a grenade off where it is: "
                  "an ogre's in flight, yours on the ground, a hand grenade's dud left as a trap. Whoever set it off gets "
                  "the kills. Not one in your hand."),
        slider("Grenade Shot Size", vr_grenade_shoot_pad, 0.f, 8.f, 0.5f, "%.1f units").extend(0.f, 12.f)
            .help("How far beyond the grenade's model a shot still hits it (0: the model's own box, a few units across)."),
        toggle("Blows Set Grenades Off", vr_grenade_shoot_melee)
            .help("A very strong melee blow (the speeds and damage below) or a running chainsaw sets an enemy's grenade "
                  "off. Any other blow bats it away: one lying or rolling flies off the way the blow went; one in flight "
                  "is batted by a weapon's swing and caught by an empty hand. Your own grenades your blows pass through. "
                  "Off: blows pass through grenades."),
        slider("Blow Speed to Set Off", vr_grenade_melee_speed, 5.f, 40.f, 0.5f, "%.1f m/s").extend(0.f, 80.f)
            .help("How fast a punch, a gun's swing, a pommel or a gun's butt must go to set a grenade off rather than bat "
                  "it (the arm's speed, as the melee events print it). Your strongest punches go about 20-25."),
        slider("Weapon Blow Speed to Set Off", vr_grenade_melee_speed_weapon, 5.f, 60.f, 0.5f, "%.1f m/s")
            .extend(0.f, 100.f)
            .help("The same for a sword's, the axe's, the crowbar's, a club's or a stopped chainsaw's blade or head: they "
                  "are swung faster (an ordinary sword slash goes about 28). A running chainsaw always sets it off."),
        slider("Blow Damage to Set Off", vr_grenade_melee_damage, 0.f, 60.f, 1.f, "%.0f").extend(0.f, 200.f)
            .help("And the least damage the blow must deal (a full-speed punch deals about 25; tired blows less). 0: the "
                  "speed alone."),
        header("Hand Grenades"),
        toggle("Hand Grenades", vr_handgrenade)
            .help("Reach for the pouch on your lower back with an empty hand and grip: a grenade from it, while you have "
                  "rockets (the grenade launcher's ammo; one leaves your ammo with each grenade). Throw it as anything you "
                  "carry: it goes off as the launcher's grenade. Where the pouch is: below."),
        cycle("Arm Hand Grenades", vr_handgrenade_arm, {{0.f, "Trigger pulls the pin"}, {1.f, "When let go of"}})
            .help("Trigger: press it while holding the grenade to pull the pin (it fizzes; the fuse runs); let go of unarmed, "
                  "it is a dud you can pick up again. When let go of: the lever flies off as it leaves your hand. Either "
                  "way, let go of unarmed at the pouch, it goes back in."),
        slider("Hand Grenade Fuse", vr_handgrenade_fuse, 1.f, 5.f, 0.1f, "%.1f s")
            .help("From the pin (or the throw) to the blast. The launcher's grenades take 2.5 s. Hold it too long and it goes "
                  "off in your hand."),
        header("Grenade Pouch"),
        open("Grenade Pouch: Hip Holsters", pageIndex(pageHipHolsters)).help("Where the grenade pouch is and how a grenade sits in the hand: Weapons > Hip Holsters."),
        open("Pouch Turn (Hip Holsters)", pageIndex(pageHipHolsters))
            .help("The same place, and how the pouch is turned, with the hip holsters."),
    };
}
