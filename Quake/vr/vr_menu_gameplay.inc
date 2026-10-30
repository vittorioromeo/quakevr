// vr_menu_gameplay.inc -- VR Settings > Gameplay: enemies (shoves, melee, holding, knockdowns, weapons), damage, sound,
// blood feedback, parry, stamina, batting, the Survival Arena's waves. Part of vr_menu.cpp (included where it was: the
// same translation unit, so its anonymous namespace).

[[nodiscard]] za::Vector<Item> pageGameplay()
{
    return {
        header("Monsters"),
        toggle("Enemies Hurt by Liquids", vr_enemy_liquid_damage).help("Monsters burn in slime/lava and drown after 12 seconds with their heads underwater, including knocked-down ragdolls. Fish, bosses and lava dwellers are immune."),
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
        cycle("Ammo Box Weapon Drops", "vr_ammobox_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random ammo box weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
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
        cycle("Armed Humanoids' Shove", "vr_agrunt_shove", {{0.f, "Model Frames, Guns"}, {1.f, "Posed, Guns"},
            {2.f, "Posed, Every Weapon"}})
            .help("How armed grunts and enforcers shove you: posed (the Shove pose's keys, per weapon class: the off "
                  "hand pushing beside a one-handed gun, a long gun pushed across the body in both hands, the palms "
                  "out holding two weapons or the fists), with guns only (the default; melee and fist humanoids just "
                  "strike) or with every weapon and their fists; or as before (their model's shove frames, the arms held as they aim, guns only)."),
        toggle("Vores Shove", vr_enemy_shove_vore)
            .help("Vores shove you too, swinging their arms down at you: the hardest shove of all. They reach a little "
                  "farther (they are wider)."),
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
        toggle("Kind Multipliers", vr_enemy_melee_kind)
            .help("Some kinds' own arcs on top of their size: a knight's quick sword thrust is narrower (0.85: 55 deg), so "
                  "it is easier to sidestep than an ogre's chainsaw. Off: size alone."),
        header("Swinging"),
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
        header("Effect"),
        slider("Slow Down", vr_foegrab_slow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of a fully held enemy's own movement (and turning) taken off, times its hold."),
        slider("Follow the Hand", vr_foegrab_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of your holding hand's own movement the enemy follows, times its hold: pull your hand back, or "
                  "walk off holding it, and a grunt comes along, an ogre a little, a shambler hardly."),
        slider("Pull", vr_foegrab_drag, 0.f, 20.f, 0.5f, "%.1f /s")
            .help("How quickly your hand pulls the spot it holds towards it, times the hold (0: not at all): takes up "
                  "what Follow the Hand leaves."),
        cycle("One Hand Drags By", "vr_foegrab_drag_onehand",
            {{1.f, "Torso, Shoulders"}, {2.f, "Torso, Shoulders, Head"}, {0.f, "Anywhere"}})
            .help("Where one hand alone (the other off it) drags a standing enemy along by Follow the Hand and Pull. "
                  "Torso, Shoulders: a hand on its legs only pulls its legs out (Leg Grabs, One-Hand Pull), on its head "
                  "only holds it, on a forearm or hand neither. Anywhere: as before."),
        header("Their Shoves"),
        cycle("Held Enemy's Shove", "vr_foegrab_shove", {{0.f, "Resisted"}, {1.f, "Breaks Free"}})
            .help("Resisted: a grunt or enforcer you hold barely pushes you (by Shove Resistance and its hold), and comes "
                  "along with you. Breaks Free: its shove pushes you fully, as ever, and tears it from your hands."),
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
        toggle("Throw Away From You", vr_foegrab_throw_away)
            .help("Off: a turn that would throw the enemy straight away from you does nothing (that's a shove's job). On: "
                  "it throws too."),
        cycle("Who Can Be Thrown", "vr_foegrab_throw_by_mass", {{0.f, "By Group"}, {1.f, "By Mass"}})
            .help("By Group: each monster group's Two-Hand Throw (Combat > Monster Groups: always, only when hurt, or "
                  "never). By Mass: the two masses below."),
        open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Throw)>))
            .help("Which groups are always thrown, only when hurt, or never (Combat > Monster Groups)."),
        toggle("Hold On After Throw", vr_foegrab_throw_hold)
            .help("On: the thrown enemy stays in your hands as a ragdoll, held softly: it still topples and falls the way "
                  "you threw it, and you can steer it, drag it along, pull it about; let go of the grips to drop it or "
                  "throw it. Off: both hands let go as it is thrown."),
        slider("Passes Through You", vr_foegrab_throw_grace, 0.f, 2.f, 0.05f, "%.2f s").extend(0.f, 5.f)
            .help("The thrown enemy passes through your hands, and you, this long as it falls: your hands, still in it and "
                  "moving on, or you standing where you pull it, no longer knock it back over the other way. 0: they do."),
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
        slider("One-Hand Pull", vr_foegrab_legpull_onehand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("One hand alone on its leg (the other free, hanging on a ledge or holding something) pulls it out too, "
                  "at Pull Speed times this: a harder pull than with both hands. 0: both hands only."),
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
        toggle("Broken Neck Flops", vr_foegrab_necksnap_loose)
            .help("A snapped neck's joint goes loose: its head flops and rolls as if the vertebrae broke. Off: it stays "
                  "as stiff as any body's."),
    };
}

// Ragdoll Triggers (QC vr_kdtrig.qc; ROUND22.md, "The flinch removed; ragdoll triggers"): Combat > Ragdoll Triggers, a page per
// trigger and Monster Resistance; the shove's, the leg sweep's, the leg pull's and the lift's on their own pages.
[[nodiscard]] za::Vector<Item> pageKdTrigLedge()
{
    return {
        toggle("Ledges", vr_kdtrig_ledge_on)
            .help("Struck with a drop of Least Drop or more right behind it (along the hit, room for its body to drop), it is pushed off. Needs Ragdoll Triggers and Knockdowns on."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigStairs()
{
    return {
        toggle("Stairs", vr_kdtrig_stairs_on)
            .help("Struck at the top of steps going down behind it (two or more, the first right behind its feet), it falls down them. Needs Ragdoll Triggers and Knockdowns on."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigSlope()
{
    return {
        toggle("Slopes", vr_kdtrig_slope_on)
            .help("Standing on a steep slope, hit down it, it topples down the slope. Needs Ragdoll Triggers and Knockdowns on."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigShotgun()
{
    return {
        toggle("Shotgun Range", vr_kdtrig_shotgun_on)
            .help("A shotgun or super shotgun blast of yours from close by knocks it over. Needs Ragdoll Triggers and Knockdowns on."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigWounded()
{
    return {
        toggle("Wounded", vr_kdtrig_wounded_on)
            .help("Badly hurt, any hit may knock it down. Needs Ragdoll Triggers and Knockdowns on."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigBlow()
{
    return {
        toggle("Strong Blows", vr_kdtrig_blow_on)
            .help("A strong melee blow or a prop you fling knocks it over (up to twice the chance at twice Least Damage: a hard punch is about 30). Needs Ragdoll Triggers and Knockdowns on."),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigBlast()
{
    return {
        toggle("Explosions", vr_kdtrig_blast_on)
            .help("A nearby explosion (a rocket, a grenade, a barrel, any blast that leaves it alive) throws it down away from the blast's centre (Least Damage: the damage it took, already less with distance). Needs Ragdoll Triggers and Knockdowns on."),
    };
}

// Thrown Bodies' and Tackles' weight classes (both pages).
void kdTrigWeightRows(za::Vector<Item>& items)
{
    items.pushBack(header("Weight Classes"));
}

[[nodiscard]] za::Vector<Item> pageKdTrigThrown()
{
    za::Vector<Item> items{
        toggle("Thrown Bodies", vr_kdtrig_thrown_on)
            .help("A body you throw (a corpse or a knocked-down enemy: let go of, or a two-hand throw) or swing in your "
                  "hand into a living monster knocks it over the way the body goes, by their weights and the body's "
                  "momentum. Needs Ragdoll Triggers and Knockdowns on."),
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
        slider("Chance", vr_kdtrig_tackle, 0.f, 1.f, 0.05f, "%.2f").help("The chance a light one falls."),
        slider("Run-Up", vr_kdtrig_tackle_runup, 0.f, 4.f, 0.1f, "%.1f s")
            .help("How long you must have been running at that speed before it counts (a sprint begun next to the "
                  "monster doesn't; 0: none)."),
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
        slider("One-Handed Shove", vr_knockdown_onehand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The chance of a shove with one hand, times this."),
        slider("Two-Handed Shove", vr_knockdown_twohand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The chance of a shove with both hands, times this."),
        toggle("Surprise Shoves Knock Down", vr_knockdown_surprise)
            .help("A shove of a monster that isn't fighting you (asleep, idle, busy with another) or from behind always "
                  "knocks it down, if its group can be knocked down by a shove at all."),
        toggle("Over a Ledge, Always", vr_knockdown_ledge)
            .help("A shove that would carry it over the edge of a high drop (Ledge Height or more) always knocks it down, "
                  "so it tumbles off as a ragdoll. Not through a wall or railing; stairs are no ledge. Off: the chance as "
                  "anywhere."),
        header("Hits Unbalance"),
        toggle("Strong Hits Unbalance", vr_hit_stagger)
            .help("A strong throw (a heavy prop or a weapon, fast) or melee blow may make a monster lose its balance as a "
                  "shove does: it staggers as a ragdoll and recovers, or by its knockdown chance goes over. Monsters "
                  "without one are pushed and flinch. The stronger the hit and the more of its health it took, the likelier."),
        header("Impacts"),
        toggle("Impacts Hurt", vr_ragimpact)
            .help("A monster you hold or threw as a ragdoll, alive or dead, is hurt where it strikes a wall or the floor "
                  "hard: its speed times the mass behind it. Its head or limb struck wears down too, and pops past its own "
                  "health (a head worn through kills). Slam a held head into a wall again and again; throw a lifted one into "
                  "a wall."),
        slider("Crush Least Speed", vr_ragimpact_crush_speed, 100.f, 1000.f, 25.f, "%.0f u/s").extend(0.f, 3000.f)
            .help("Landing slower than this crushes nothing (a jump from the same level lands at about 270 on the floor, "
                  "200 on top of a body: 180 crushes both; a stair step: 160; a 100-unit drop: 400)."),
        slider("Crush Push", vr_ragimpact_crush_push, 0.f, 1.5f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A landing on a body lying down (alive or dead) pushes the part under your feet down and out at this "
                  "times your landing speed, the body jolting with it. 0: it lies still."),
        toggle("Crush Standing Ones", vr_ragimpact_crush_standing)
            .help("Landing on a standing monster's head or shoulders crushes too. Off: only one down or staggering as a "
                  "ragdoll."),
        toggle("Crush Breaks Your Fall", vr_ragimpact_crush_cushion)
            .help("A landing that crushed a monster spares you the fall's damage. Off: Quake's fall damage as ever."),
        toggle("Stomp Knocks Down", vr_ragimpact_stomp)
            .help("Landing on a standing monster's top fast enough knocks it down as a shove does (it gets up again), "
                  "if the crush did not kill it; you bounce off it."),
        header("Leg Sweep"),
        toggle("Leg Sweep", vr_legsweep)
            .help("Sweep a humanoid's lower legs with something long and blunt, level and fast, and it goes down as in a "
                  "judo foot sweep: its feet slide the way you swept, the rest of it topples over the other way (forward "
                  "or backward, whichever side you sweep it from). A long gun, the crowbar, a sword's hilt (not its "
                  "blade), a long plank: yes. The axe, the super shotgun, the lightning gun, a rock, a brick, a crate: "
                  "no. Only humanoids (no ogres, fiends, shamblers)."),
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
        toggle("Weapon Stays in Hand", vr_knockdown_weld)
            .help("Its weapon stays in its hand while it is down or staggering, lifted or struggling (killed there, it "
                  "drops it as usual): held by the hand that holds it in its rest pose, following that hand. Off: it "
                  "flops loose as a dead one's."),
        header("Getting Up"),
        slider("Struggling Strength", vr_knockdown_wiggle, 0.f, 3.f, 0.1f, "%.1fx")
            .help("Living knockdowns gently curl their chest and nod their head about their resting pose. Arms and legs follow physically. 0: still. Dead bodies never struggle."),
        slider("Struggling Frequency", vr_knockdown_wiggle_frequency, 0.1f, 5.f, 0.1f, "%.1f Hz"),
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
        header("Pulling the Cord"),
        toggle("Model Handle in the Hand", vr_chainsaw_model_handle)
            .help("The cord's handle in your fist is the chainsaw model's own, the one seated on it, easing from its seat "
                  "into the fist's turn as you take it. Off: a plain round handle."),
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
                  "(vr_agrunt_grip_scope)"),
        header("The Weapon in the Hand"),
        pos("Forward", vr_agrunt_grip_x, "Moves the weapon forward (back: negative) in the hand: units, along the hand's "
                                          "grip (the way its own gun pointed). Second Grip: the hand along the weapon."),
        pos("Left", vr_agrunt_grip_y, "Moves the weapon to the hand's left (units)."),
        pos("Up", vr_agrunt_grip_z, "Moves the weapon up in the hand (units): a sword's handle into the fist, not its guard."),
        ang("Pitch", vr_agrunt_grip_pitch, "Tips the weapon down (up: negative) in the hand."),
        ang("Yaw", vr_agrunt_grip_yaw, "Turns the weapon left (right: negative) in the hand."),
        ang("Roll", vr_agrunt_grip_roll, "Rolls the weapon about its length in the hand."),
        header("Keep Them"),
        command("Print as Defaults", "vr_agrunt_grip_dump")
            .help("Prints the changed grips as code for the shipped defaults (the console; vr_agrunt_grip_dump). They are "
                  "kept in your config meanwhile (vr_agrunt_grips)."),
        command("Reset This One", "vr_agrunt_grip_reset").help("This weapon and hand back to the shipped default."),
    };
}

// Combat > Armed Grunts > Armour (QC vr_agarmor.qc; ROUND23.md, "Armour for armed humanoids, P1"): armed grunts' breakable
// armour, its rules and how often they wear it.
[[nodiscard]] za::Vector<Item> pageArmedArmour()
{
    const auto skillRows = [](za::Vector<Item>& out, const char* name, cvar_t& chance, cvar_t& type, cvar_t& pieces) {
        out.pushBack(header(name));
        out.pushBack(slider("Chance Armoured", chance, 0.f, 1.f, 0.05f, "%.2f")
                .help("The chance an armed grunt spawns wearing armour at this skill."));
        out.pushBack(slider("Best Armour", type, 1.f, 3.f, 1.f, "%.0f")
                .help("Its armour is rolled up to this type: 1 green (saves 30% of a hit, 100 in all), 2 yellow (60%, "
                      "150), 3 red (80%, 200)."));
        out.pushBack(slider("Pieces", pieces, 1.f, 10.f, 1.f, "%.0f")
                .help("How many pieces it wears: 1 the cuirass, 2 and the helmet, more: pauldrons, bracers, thigh and "
                      "shin plates picked at random; 10 the full set."));
    };
    za::Vector<Item> items{
        header("Rules"),
        cycle("Absorb", "vr_agarmor_rule", {{0.f, "Player's Rule"}, {1.f, "Plate Blocks All"}})
            .help("Player's Rule: a plate saves its type's share of a hit (green 30%, yellow 60%, red 80%) from its own "
                  "pool, as your armour does. Plate Blocks All: a plate stops the whole hit until its pool is gone. "
                  "A plate at 0 breaks off and falls."),
        toggle("Helmets Stop Headshots", vr_agarmor_helmet_head)
            .help("On: a helmet also halves a headshot's extra damage and no head pops off while it holds. Off: it only "
                  "absorbs, as any plate."),
        toggle("Plates Stop Cuts", vr_agarmor_block_cuts)
            .help("No limb is cut or popped off under an unbroken plate: break it first."),
        slider("Broken Plates Last", vr_agarmor_debris_time, 1.f, 60.f, 1.f, "%.0f s")
            .help("A broken plate falls as debris (you can't force-grab it) and fades out after this long."),
    };
    skillRows(items, "Easy", vr_agarmor_skill0, vr_agarmor_skill0_type, vr_agarmor_skill0_pieces);
    skillRows(items, "Normal", vr_agarmor_skill1, vr_agarmor_skill1_type, vr_agarmor_skill1_pieces);
    skillRows(items, "Hard", vr_agarmor_skill2, vr_agarmor_skill2_type, vr_agarmor_skill2_pieces);
    skillRows(items, "Nightmare", vr_agarmor_skill3, vr_agarmor_skill3_type, vr_agarmor_skill3_pieces);
    return items;
}

// Combat > Armed Grunts > Worn Backpacks (QC vr_wornpack.qc; vr_box3d_wornpack.cpp; ROUND23.md, "Worn backpacks"):
// experimental, off by default.
[[nodiscard]] za::Vector<Item> pageArmedPacks()
{
    return {
        header("Worn Backpacks"),
        toggle("Worn Backpacks", vr_worn_backpacks)
            .help("Experimental. Armed grunts wear the backpack they would drop on their back, as ragdolls too (it weighs "
                  "and collides with the body, and you can drag the body by it). It tears off with a rip on a hard pull, "
                  "a hard landing or a hit on the pack; torn off a living one, its spare ammo goes with it. Only a dead "
                  "one's pack can be taken. Grunts spawned after it is turned on."),
        header("Tearing Off"),
        slider("Pull That Tears It", vr_worn_backpack_rip_force, 500.f, 4000.f, 100.f, "%.0f N")
            .help("A hand holding a ragdoll by its pack and yanking this hard (kept up a moment) tears the pack off into "
                  "the hand. A slow drag pulls about 1500 N; a hold's spring gives 4000 N at most (Ragdoll Grab Force)."),
        slider("Hit Chance", vr_worn_backpack_hit_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("A blow, a throw, a shot or a projectile on the pack tears it off with this chance at the Hit Damage "
                  "below (more damage: likelier)."),
        slider("Hit Damage", vr_worn_backpack_hit_ref, 1.f, 100.f, 1.f, "%.0f")
            .help("The damage of a hit whose chance is the Hit Chance."),
        slider("Landing That Tears It", vr_worn_backpack_impact, 2.f, 30.f, 0.5f, "%.1f m/s")
            .help("A ragdoll striking the level at least this fast loses its pack."),
        slider("Spare Ammo in It", vr_worn_backpack_reserve, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of a grunt's spare rounds it carries in its pack: lost with the pack while it lives (it "
                  "fights on with what its gun holds)."),
        slider("Rip Volume", vr_worn_backpack_volume, 0.f, 1.f, 0.05f, "%.2f")
            .help("The fabric rip's loudness."),
        header("Fit"),
        slider("Size", vr_worn_backpack_scale, 0.3f, 1.2f, 0.05f, "%.2f").help("The pack's size on the back."),
        slider("Forward", vr_worn_backpack_x, -8.f, 8.f, 0.25f, "%.2f").help("Moves the pack into the back (out: negative)."),
        slider("Left", vr_worn_backpack_y, -8.f, 8.f, 0.25f, "%.2f"),
        slider("Up", vr_worn_backpack_z, -12.f, 12.f, 0.25f, "%.2f"),
        slider("Pitch", vr_worn_backpack_pitch, -45.f, 45.f, 1.f, "%.0f"),
        slider("Yaw", vr_worn_backpack_yaw, -180.f, 180.f, 5.f, "%.0f"),
        slider("Roll", vr_worn_backpack_roll, -45.f, 45.f, 1.f, "%.0f"),
        slider("Weight", vr_worn_backpack_mass, 0.5f, 20.f, 0.5f, "%.1f kg").help("Its weight on a ragdoll's chest."),
    };
}

// Combat > Armed Grunts > Armed Humanoid Replacements (QC vr_armedrepl.qc; ROUND22.md, "Armed humanoid replacements"):
// a map's grunts and enforcers as armed ones. Everything off by default; a map's new loadouts at its next load.
[[nodiscard]] za::Vector<Item> pageArmedRepl()
{
    const auto chance = [](const char* label, cvar_t& c, const char* what) {
        return slider(label, c, 0.f, 1.f, 0.05f, "%.2f").help(what);
    };
    return {
        header("Replacements"),
        toggle("Replace All Humanoids", vr_agrunt_repl)
            .help("Every grunt and enforcer a map spawns (its own, its spawners', its waiting ones) is its armed version "
                  "holding its own gun: the same damage, rate of fire, aim, health and loot as the one it replaces, but "
                  "you can knock or pull the gun out of its hands, win it in a tug, parry it, shoot it out, and its "
                  "ragdoll keeps hold of it. Takes effect at the next map load (vr_agrunt_repl)."),
        header("Weapon Variety (on top)"),
        chance("Common: Shotgun, Nail Pistol", vr_agrunt_repl_common,
            "The chance one holds a shotgun or a nail pistol instead of its own gun (vr_agrunt_repl_common)."),
        chance("Uncommon: Nailguns, Super Shotgun", vr_agrunt_repl_uncommon,
            "The chance of a nailgun, a super nailgun or a super shotgun, each only once you own that weapon "
            "(vr_agrunt_repl_uncommon)."),
        chance("Rare: Launchers, Thunderbolt, Melee", vr_agrunt_repl_rare,
            "The chance of a grenade or rocket launcher, the thunderbolt, a melee weapon, a mission pack's gun or AD's "
            "soldiers' guns (in AD maps): each only once you own it, and only later in an episode "
            "(vr_agrunt_repl_rare)."),
        slider("Rare From Map", vr_agrunt_repl_rare_map, 0.f, 8.f, 1.f, "%.0f")
            .help("Rare weapons only from this map of an episode on (e1m4, hip1m4, r2m4, ...); maps with no number in "
                  "their name never. 0: any map (vr_agrunt_repl_rare_map)."),
        chance("Dual Wielding", vr_agrunt_repl_dual,
            "The chance of a second weapon that pairs with the first: two guns not too big, two one-handed blades "
            "(vr_agrunt_repl_dual)."),
        chance("Gun and Axe", vr_agrunt_repl_gunaxe,
            "The chance of a gun in the right hand and an axe in the left: it fires the gun, takes to the axe when "
            "you get close or its gun runs dry, back to the gun when you back off (vr_agrunt_repl_gunaxe)."),
        header("Difficulty"),
        slider("Shuffle", vr_agrunt_repl_seed, 0.f, 100.f, 1.f, "%.0f")
            .help("Each humanoid's loadout is the same every time you play a map (its spawn point and the map pick it); "
                  "another number deals every map's out anew (vr_agrunt_repl_seed)."),
        header("Where"),
        toggle("Quake's Campaigns", vr_agrunt_repl_id)
            .help("Quake, Dimension of the Past, Dimension and Dawn of the Machine, and custom maps "
                  "(vr_agrunt_repl_id)."),
        toggle("Mission Packs", vr_agrunt_repl_mp)
            .help("Scourge of Armagon and Dissolution of Eternity (vr_agrunt_repl_mp)."),
        toggle("Arcane Dimensions", vr_agrunt_repl_ad)
            .help("Arcane Dimensions' maps too: their encounters are designed around their own monsters, so off by "
                  "default (vr_agrunt_repl_ad)."),
    };
}

// Combat > Armed Grunts > Weapon Holds (vr_agholds.inc; vr_aggrip.cpp agholds; QC vr_armedgrunt_pose.qc VR_AGH_Hands;
// ROUND22.md, "Armed humanoids' weapon holds"): each kind's hold of each weapon, live.
void armedHoldsReset()
{
    agholds::reset();
}

// Combat > Armed Grunts > IK Stretch (agpose::stretch; vr_agrunt_ik_stretch; ROUND22.md, "IK stretch per kind and
// class"): a row per kind and pose class, each pick a step of 0.05 up to the most, then back to 1. The Rig Studio's Pose
// Studio Data window has the same as a slider.
constexpr float ikStretchStep = 0.05f;
char ikStretchText[agpose::numKinds * agpose::numClassOrder][64]{}; // (each row's text, made as it is drawn)

[[nodiscard]] const char* ikStretchRow(int arg)
{
    const int kind = arg / 100, cls = arg % 100;
    int slot = 0;
    for(int i = 0; i < agpose::numClassOrder; i++)
    {
        slot = agpose::classOrder[i] == cls ? kind * agpose::numClassOrder + i : slot;
    }
    const float by = agpose::stretch(kind, cls);
    q_snprintf(ikStretchText[slot], sizeof(ikStretchText[slot]), "%s: %s", agpose::classTitle(cls),
        by > 1.0001f ? va("%.2fx", static_cast<double>(by)) : "1 (none)");
    return ikStretchText[slot];
}

[[nodiscard]] const char* ikStretchHelp(int)
{
    return "How far this kind's arms may lengthen to put its hands on their goals in this class's poses: the upper arm and "
           "the forearm alike, only as far as a hand needs (in reach: none), eased; the skin stretches with them. Each "
           "pick adds 0.05, past 1.5 back to 1. The Rig Studio's Pose Studio Data window has it as a slider "
           "(vr_agrunt_ik_stretch).";
}

void ikStretchPick(int arg)
{
    const int kind = arg / 100, cls = arg % 100;
    const float by = agpose::stretch(kind, cls) + ikStretchStep;
    agpose::setStretch(kind, cls, by > agpose::stretchMost + 0.001f ? 1.f : by);
}

void ikStretchReset()
{
    for(int k = 0; k < agpose::numKinds; k++)
    {
        for(const int c : agpose::classOrder)
        {
            agpose::setStretch(k, c, agpose::shippedStretch(k, c));
        }
    }
}

[[nodiscard]] za::Vector<Item> pageArmedStretch()
{
    za::Vector<Item> items;
    items.pushBack(action("Reset to Defaults", ikStretchReset).help("Every kind and class back to its shipped IK stretch."));
    for(int k = 0; k < agpose::numKinds; k++)
    {
        items.pushBack(header(k == 0 ? "Grunts" : "Enforcers"));
        for(const int c : agpose::classOrder)
        {
            items.pushBack(row(ikStretchRow, ikStretchHelp, ikStretchPick, k * 100 + c, -1));
        }
    }
    return items;
}

[[nodiscard]] za::Vector<Item> pageArmedHolds()
{
    za::Vector<Item> items;
    items.pushBack(action("Reset to Defaults", armedHoldsReset)
            .help("Every weapon back to Auto for every kind (vr_agholds_reset)."));
    for(int k = 0; k < agholds::numKinds; k++)
    {
        items.pushBack(header(k == 0 ? "Grunts" : "Enforcers")); // (agpose::kindsName's, titled)
        for(int r = 0; r < agholds::count(); r++)
        {
            const bool autoOne = agholds::autoHands(k, r) == 1;
            items.pushBack(cycle(agholds::label(r), agholds::setting(k, r),
                {{0.f, autoOne ? "Auto (One-Handed)" : "Auto (Two-Handed)"}, {1.f, "One-Handed"}, {2.f, "Two-Handed"}})
                    .help("How this kind holds this weapon (one with two weapons, one in each hand, as it is). One-Handed: "
                          "the off hand free, hanging or in its carry pose, the weapon aimed and kicked by the one hand. "
                          "Two-Handed: the off hand on the weapon's second grip (a foregrip, a front handle), else under "
                          "and round the other on its handle. Auto: the built-in hold (the axe and the enforcer's pistols "
                          "in one hand). Takes effect at once (vr_agrunt_hold_<grunt|enforcer>_<weapon>)."));
            if(!agholds::isGun(r))
            {
                continue;
            }
            const int autoCls = agholds::autoClass(r);
            const char* autoLabel = autoCls == agpose::Pistol        ? "Auto (Pistol)"
                                    : autoCls == agpose::Launcher    ? "Auto (Shoulder Launcher)"
                                    : autoCls == agpose::Sprayer     ? "Auto (Sprayer)"
                                    : autoCls == agpose::LaserCannon ? "Auto (Laser Cannon)"
                                    : autoCls == agpose::Widowmaker  ? "Auto (Widowmaker)"
                                                                     : "Auto (Rifle)";
            items.pushBack(cycle(agholds::poseLabel(r), agholds::categorySetting(k, r),
                {{0.f, autoLabel}, {1.f, "Rifle"}, {2.f, "Pistol"}, {3.f, "Shoulder Launcher"}, {4.f, "Sprayer"},
                    {5.f, "Laser Cannon"}, {6.f, "Widowmaker"}})
                    .help("The pose category this kind's poses take with this weapon: the Pose Studio's keys of that "
                          "category (its aim, carry, run and pickup; Rig Studio > Pose Studio Data, or the VR Pose Studio). "
                          "Auto: the built-in one (pistols, rifles, shoulder launchers, sprayers, the laser cannon and the "
                          "Widowmaker their own). A category not tuned yet poses as the Rifle. Takes effect at once "
                          "(vr_agrunt_posecat_<grunt|enforcer>_<weapon>)."));
        }
    }
    return items;
}

// Combat > Armed Grunts > AI Behaviour (QC vr_armedgrunt_ai.qc; ROUND22.md, "Armed humanoids' AI behaviour"): every
// armed humanoid's decision in one place, the knobs (1: as before at Normal) and the skill's impact on them.
[[nodiscard]] za::Vector<Item> pageArmedAI()
{
    const auto knob = [](const char* label, cvar_t& c, const char* what) {
        return slider(label, c, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f).help(what);
    };
    return {
        header("Difficulty"),
        slider("Difficulty Impact", vr_agrunt_ai_skill, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much the skill level moves the knobs below, Normal always as set. At 1: Easy aggressiveness "
                  "0.85x, accuracy 0.8x, reaction 1.3x, fire rate 0.9x, retreat 0.85x; Hard 1.2x, 1.25x, 0.7x, 1.15x, "
                  "1.15x; Nightmare 1.4x, 1.5x, no reaction delay, 1.15x, 1.3x (and their parry's chance and speed "
                  "by skill). 0: the skill changes none of them (Quake's own rules for every monster stay: "
                  "Nightmare's attacks without pauses) (vr_agrunt_ai_skill)."),
        header("Fighting"),
        knob("Aggressiveness", vr_agrunt_ai_aggression,
            "How readily they attack and press you: their chance to attack whenever they can, times this, and their "
            "best and furthest fighting distances nearer as it rises (times 2 / (1 + this): at 2 two-thirds as far, "
            "at 0.5 a third further). Under 1 they hold back and fire less (vr_agrunt_ai_aggression)."),
        knob("Accuracy", vr_agrunt_ai_accuracy,
            "Their aim error and their guns' spread divided by this: 2 half as much, 0.5 twice. An enforcer's own "
            "laser (no error of its own) strays under 1 (vr_agrunt_ai_accuracy)."),
        slider("Aim Error", vr_agrunt_aim_error, 0.f, 15.f, 0.5f, "%.1f deg").extend(0.f, 45.f)
            .help("How far off its aim is at most, each shot, besides the weapon's own spread; divided by Accuracy "
                  "(vr_agrunt_aim_error)."),
        slider("Reaction Time", vr_agrunt_ai_reaction, 0.f, 3.f, 0.05f, "%.2f s").extend(0.f, 10.f)
            .help("From first seeing you to their first attack, at Normal (Quake's 1 s; by Difficulty Impact "
                  "Nightmare none) (vr_agrunt_ai_reaction)."),
        knob("Fire Rate", vr_agrunt_ai_firerate,
            "Their pause after an attack (a shot or a burst, a swing) divided by this: 2 half the pause "
            "(vr_agrunt_ai_firerate)."),
        header("Fighting Distance"),
        toggle("Keep Their Distance", vr_agrunt_range)
            .help("With a gun it fights at its weapon's distance: a super shotgun close but out of your reach, a "
                  "shotgun near, nail guns and the enforcer's rifle further; nearer than that it backs off facing you "
                  "(further while you hold a melee weapon or charge it), about it it strafes round you, further it comes "
                  "on. Off: it closes in as before. Melee weapons always close in (vr_agrunt_range)."),
        knob("Retreat Readiness", vr_agrunt_ai_retreat,
            "How readily a gun's holder backs off: the distance it backs away from you nearer than (its weapon's "
            "least), times this, and the health share it turns warier below (0.35 times this). 0: it never backs "
            "off (only from its own blast) (vr_agrunt_ai_retreat)."),
        header("Aiming"),
        toggle("Aim From the Muzzle", vr_agrunt_aim_converge)
            .help("Its shots go from its gun's muzzle to your chest, its aim's error kept. Off: aimed from its grip and "
                  "fired beside that line, a miss to one side at any distance (vr_agrunt_aim_converge)."),
        toggle("Aim at What They Can Hit", vr_agrunt_aim_exposed)
            .help("Partly behind cover, they aim at a part of you their gun's line reaches (your chest first, then your "
                  "head, then whatever shows: shoulders, hips, legs; a rocket the floor at your feet), and hold fire while "
                  "nothing shows. Off: always your chest, firing whenever they see your head (vr_agrunt_aim_exposed)."),
        header("Friends and Blasts"),
        slider("Careful of Friends", vr_agrunt_friendly_care, 0.f, 1.f, 0.05f, "%.2f")
            .help("How careful it is not to shoot or strike its friends: with another monster in its line of fire (a "
                  "few lines for a shotgun's spread), the chance it holds fire and sidesteps for a clear shot. 1: "
                  "always, 0: never, it fires through them as before (vr_agrunt_friendly_care)."),
        toggle("Launchers Fired Safely", vr_agrunt_blast_safe)
            .help("With a grenade or rocket launcher or a proximity gun, it fires only when the round's predicted burst "
                  "(its arc, its bounces, a wall just ahead) is far enough from itself; otherwise it backs off or "
                  "steps aside first, and keeps away from its mines (vr_agrunt_blast_safe)."),
        header("Weapons Lying About"),
        toggle("Picks Weapons Up", vr_agrunt_rearm)
            .help("Disarmed, it fights with its fists and goes for a weapon lying near (its own or another) when you are "
                  "not nearer to it than it is (vr_agrunt_rearm)."),
        slider("Weapon Search Distance", vr_agrunt_rearm_range, 64.f, 1024.f, 32.f, "%.0f units").extend(0.f, 4096.f)
            .help("How far an unarmed one goes for a weapon lying on the floor (its own or another) rather than fight "
                  "with its fists, when you are not nearer to it than it is (vr_agrunt_rearm_range)."),
        slider("Punches Instead Within", vr_agrunt_rearm_punch, 0.f, 512.f, 16.f, "%.0f units").extend(0.f, 4096.f)
            .help("Unarmed with you this near, it fights with its fists rather than going for a weapon further than "
                  "64 units from it (one at its feet it still takes up). 0: off, as before (vr_agrunt_rearm_punch)."),
        toggle("Re-Arms Out of a Fight", vr_agrunt_rearm_idle)
            .help("Unarmed and not fighting, it also walks over to a weapon lying near and takes it up "
                  "(vr_agrunt_rearm_idle; with Picks Weapons Up)."),
        toggle("Pick Up a Second", vr_agrunt_dual_pickup)
            .help("One holding a single weapon that pairs goes for a second lying about, out of a fight, and holds it in "
                  "its left hand (vr_agrunt_dual_pickup)."),
        header("Loadout"),
        slider("Dual Wielding", vr_agrunt_dual_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance one from the pool holding a weapon that pairs gets a second in its left hand: one-handed "
                  "blades and clubs (a sword, the crowbar, the axe) swung in turn or both at once; guns one in each "
                  "hand (not the rocket launcher, the Laser Cannon, the chainsaw, the hammers). Each can be knocked or "
                  "pulled out of its hand, and it goes for another (vr_agrunt_dual_chance)."),
        slider("Flare Gun First", vr_agrunt_flaregun, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance one from the pool (or a replacement) holding one weapon carries a flare gun too: it fires "
                  "its two flares first (setting you on fire), throws it away and takes its own weapon "
                  "(vr_agrunt_flaregun)."),
        header("Parrying Your Blows (Experimental)"),
        toggle("They Parry", vr_agrunt_parry)
            .help("One holding a weapon defends against a melee blow as you do: seeing it coming it raises and angles its "
                  "weapon into the blow's way, and a blow that meets its weapon before its body is parried (the ring, "
                  "sparks, your hand knocked back). A blow to its head, legs or a side its weapon doesn't cover lands in "
                  "full. Never from behind, unaware of you, staggered or in pain. Monsters' blows on it too "
                  "(vr_agrunt_parry)."),
        toggle("Parry: Real Strikes Only", vr_agrunt_parry_strikes_only)
            .help("It raises its parry only for a real strike: a held weapon swung at it, or a closed fist punching "
                  "faster than a melee hit's least (vr_melee_speed). Your hands near it, an open hand or a grab reaching "
                  "for its weapon never (0: any hand within 30 units of its chest, as before; "
                  "vr_agrunt_parry_strikes_only)."),
        slider("Parry: Reaction Time", vr_agrunt_parry_react, 0.f, 1.f, 0.01f, "%.2f s").extend(0.f, 5.f)
            .help("From seeing a blow coming to its weapon raised into its way, at Normal skill (Easy 1.4x, Hard 0.85x, "
                  "Nightmare 0.7x): a quick swing gets through (vr_agrunt_parry_react)."),
        slider("Parry: Chance", vr_agrunt_parry_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance it sees a blow coming in time, at Normal skill (Easy 0.6x, Hard 1.15x, Nightmare 1.3x); "
                  "caught out, its weapon stays where it was: a blow still meeting it there is parried "
                  "(vr_agrunt_parry_chance)."),
        header("Manner"),
        toggle("Taunt on First Sight", vr_agrunt_taunt)
            .help("An armed grunt seeing you for the first time plays its taunt before it comes for you "
                  "(vr_agrunt_taunt)."),
        slider("Strafe Lean", vr_agrunt_strafe_lean, 0.f, 15.f, 0.5f, "%.1f deg").extend(0.f, 30.f)
            .help("Moving sideways (strafing round you, side-stepping), its pelvis and legs tip toward the way it goes, "
                  "this far at a strafe's speed (less slower), eased in and out; its upper body stays aimed. 0: "
                  "none (vr_agrunt_strafe_lean)."),
        slider("Strafe Turn", vr_agrunt_strafe_turn, 0.f, 45.f, 1.f, "%.0f deg").extend(0.f, 60.f)
            .help("Moving sideways, its pelvis and legs turn toward the way it goes (hips and feet partly along the "
                  "step), this far at a strafe's speed (less slower), eased in and out; the spine turns back so its "
                  "chest and gun stay aimed at you. Backing off to a side turns the same way. 0: none "
                  "(vr_agrunt_strafe_turn)."),
    };
}

// Combat > Survival Arena (QC vr_arena.qc and vr_arena_perks.qc; docs/vr-port/ROUND23.md, "Survival Arena"): the wave
// director's knobs, how far the game's difficulty (skill) moves them, and the perks a boss wave's clear offers. Which
// foes a wave brings is the ladder in vr_arena.qc; this page is what a run is built from. The waves follow the Skill
// below exactly as the armed humanoids' fighting follows its own Difficulty Impact above -- the two knobs are separate
// and neither touches the other's feature.
[[nodiscard]] za::Vector<Item> pageArena()
{
    return {
        header("Difficulty"),
        slider("Difficulty Impact", vr_arena_skill_impact, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much the skill level (the Skill below) moves the arena's waves, Normal always as set. At 1: Easy "
                  "0.75x, Hard 1.25x, Nightmare 1.5x of a wave's size, of how many foes are up at once, of a boss's bulk, "
                  "of a kill's and a wave clear's score and of how many perks a boss clear offers, and the other way of "
                  "the waits (6 s between waves is 8 at Easy, 4.8 at Hard, 4 at Nightmare) and of a wave clear's supplies "
                  "(2 medkits is 3 at Easy, 1 at Nightmare; an armour every 4 waves is one every 3 at Easy, 6 at "
                  "Nightmare). The ladder's gates move in whole waves: the second rung at wave 13 / 10 / 8 / 6 (Easy / "
                  "Normal / Hard / Nightmare), a knight 7 / 4 / 2 / 2, a dog 9 / 6 / 4 / 2, an ogre 11 / 8 / 6 / 4, fiends "
                  "15 / 12 / 10 / 8, death knights 19 / 16 / 14 / 12, and the first waves' unarmed humanoids 0.30 / 0.20 / "
                  "0.15 / 0.10 of them; an Easy boss wave comes a wave less often, a Nightmare one carries two bosses. "
                  "0: the skill changes nothing about the arena; 2: those steps doubled (0.5x at Easy, 2x at Nightmare); "
                  "a count never below 1 and a score never below 1 (vr_arena_skill_impact)."),
        cycle("Skill", "skill", {{0.f, "Easy"}, {1.f, "Normal"}, {2.f, "Hard"}, {3.f, "Nightmare"}})
            .help("skill: the game's difficulty, which the arena's waves follow through the Difficulty Impact above (its "
                   "own knob: the arena's scaling is nothing to do with vr_agrunt_ai_skill). The opening line names it "
                   "(ARENA - Nightmare: unarmed, wave 1 coming). From the next map, or Restart the Map (Debug > "
                   "Cheats)."),
        header("The Waves"),
        toggle("Arena In Any Map", vr_arena)
            .help("1: the waves run in this map too, off its info_arena_spawn and info_arena_item spots; the map vrarena "
                   "runs them without it (vr_arena)."),
        slider("Rest Between Waves", vr_arena_wave_rest, 1.f, 60.f, 0.5f, "%.1f s")
            .help("From a wave cleared to the next wave's countdown: this many seconds at Normal (by Difficulty Impact "
                   "5 is 6.7 at Easy, 4 at Hard, 3.3 at Nightmare) (vr_arena_wave_rest)."),
        slider("Wave Countdown", vr_arena_countdown, 0.f, 10.f, 1.f, "%.0f s")
            .help("After the rest, the next wave counts down in the middle of the view (Wave 7 in 5... 4... 3...), "
                   "a boss wave naming its boss; the same at every difficulty. 0: no countdown (vr_arena_countdown)."),
        slider("Countdown Sound", vr_arena_countdown_sound, 0.f, 1.f, 0.05f, "%.2f")
            .help("The countdown's beeps: a tick each second, higher for the last three, and a chime as the wave "
                   "comes in (Quake's own menu sounds at other pitches). Their volume; 0: silent "
                   "(vr_arena_countdown_sound)."),
        slider("Wave Size", vr_arena_wave_size, 0.25f, 4.f, 0.25f, "%.2fx")
            .help("A wave's crowds (its grunts, enforcers and the like) times this; its one signature foe and its boss "
                   "stay one. 1 is the ladder's own counts; at 1.75 (the default) wave 10 brings 10 foes instead of 6 "
                   "and wave 20 brings 13 instead of 7. "
                   "The difficulty then takes its share as it does of every count (vr_arena_wave_size)."),
        slider("Alive Pool", vr_arena_alive_max, 1.f, 16.f, 1.f, "%.0f")
            .help("How many of a wave's foes are up at once, at its start and at Normal: the rest come in one by one as "
                   "these fall. The difficulty takes its share (6 is 4 at Easy, 7 at Hard, 9 at Nightmare), and it "
                   "never passes Foes Up At Once (vr_arena_alive_max)."),
        slider("Pool Grows Every", vr_arena_alive_grow, 0.f, 20.f, 1.f, "%.0f waves")
            .help("The alive pool takes one more foe every this many waves (4: 6 at wave 1, 7 at 5, 8 at 9 ...), up to "
                   "Foes Up At Once; 0: it never grows (vr_arena_alive_grow)."),
        slider("Opening Burst", vr_arena_burst, 1.f, 16.f, 1.f, "%.0f")
            .help("A wave opens with this many foes at once (never more than its pool); the rest trickle in "
                   "(vr_arena_burst)."),
        slider("Spawn Gap", vr_arena_spawn_gap, 0.5f, 10.f, 0.25f, "%.2f s")
            .help("Once a foe falls, the next one comes in this long after, up to twice as long at random (1.5: 1.5 to 3 "
                   "s), at Normal: the harder the difficulty, the shorter (2 / 1.5 / 1.2 / 1). A wave's boss comes in "
                   "halfway through it, 3 s after its warning, instead (vr_arena_spawn_gap)."),
        slider("Foes Up At Once", vr_arena_max_alive, 1.f, 16.f, 1.f, "%.0f")
            .help("The ceiling on a wave's alive pool, at Normal -- the difficulty takes its share of this (8 is 6 at "
                   "Easy, 10 at Hard, 12 at Nightmare; 16 at most). New foes come in out of your sight and not right "
                   "next to you when the map has such a place (vr_arena_max_alive)."),
        header("Boss Waves"),
        slider("Boss Every", vr_arena_boss_every, 0.f, 20.f, 1.f, "%.0f")
            .help("A wave with a marked big one in it every this many waves (0: none) -- an enforcer boss early, an ogre "
                   "from wave 10, a death knight from wave 16 at Normal (earlier the harder the skill). At Easy the gap is "
                   "one longer (5 is 6); Normal, Hard and Nightmare keep it, and a Nightmare boss wave carries two, the "
                   "second the same kind at half its bulk (vr_arena_boss_every)."),
        slider("Boss Bulk", vr_arena_boss_health, 1.f, 8.f, 0.05f, "%.2fx")
            .help("A boss's health times this and the difficulty's factor: an enforcer boss at 1.25 stands at 76 health "
                   "at Easy, 101 at Normal, 126 at Hard, 151 at Nightmare (a Nightmare boss wave's second at half that) "
                   "(vr_arena_boss_health)."),
        header("Armour"),
        toggle("Armoured Foes", vr_arena_foe_armor)
            .help("1: the waves' grunts and enforcers may wear armour plates (Combat > Armed Grunts > Armour's rules), "
                   "more of them, in more plates and heavier kinds as the waves go and the harder the skill: green "
                   "cuirass and helmet first, yellow from wave 12 at Normal, red from wave 22. A grunt or enforcer boss "
                   "always wears a kind above its wave's. In the arena this replaces the skill's own armour roll; 0: no "
                   "wave foe wears any (vr_arena_foe_armor)."),
        slider("Armour Chance", vr_arena_foe_armor_scale, 0.f, 4.f, 0.05f, "%.2fx")
            .help("The armour's chance times this: at 1 a foe at the armour's first wave has 10% at Normal, 3% more each "
                   "wave after (34% at wave 12, 58% at wave 20), times the difficulty's factor, never past 85%. A boss "
                   "wave's foes have 15% more. 0: none (vr_arena_foe_armor_scale)."),
        slider("Armour From Wave", vr_arena_foe_armor_from, 2.f, 30.f, 1.f, "%.0f")
            .help("The first wave a foe may wear armour, at Normal; the difficulty moves it as it moves every gate "
                   "(4 is wave 7 at Easy, 4 at Normal, 2 at Hard and Nightmare). Its plates grow from there: the "
                   "cuirass and the helmet, one limb plate more every 3 waves (vr_arena_foe_armor_from)."),
        header("Wave Clears"),
        slider("Medkits", vr_arena_medkits_after, 0.f, 4.f, 1.f, "%.0f")
            .help("Medkits every wave clear drops (0: none), the difficulty dividing them: 2 is 3 at Easy, 2 at Normal "
                   "and Hard, 1 at Nightmare (vr_arena_medkits_after)."),
        slider("An Armour Every", vr_arena_armor_every, 0.f, 12.f, 1.f, "%.0f waves")
            .help("A wave clear adds an armour every this many waves (0: none); the harder the skill the longer its gap: "
                   "4 is one every 3 waves at Easy, 4 at Normal, 5 at Hard, 6 at Nightmare. From wave 12 of the run they "
                   "alternate with the heavier kind -- what you have earned is yours, whatever the difficulty "
                   "(vr_arena_armor_every)."),
        slider("Armour Shards", vr_arena_shards, 0.f, 4.f, 1.f, "%.0f")
            .help("On top of the medkits and armour, every wave clear drops 1 to this many armour shards at random "
                   "(+5 armour each, up to 200). 0: none (vr_arena_shards)."),
        toggle("Supplies Anywhere", vr_arena_scatter)
            .help("1: a wave clear's medkits, armour and shards land at random places all over the arena -- the floor, "
                   "the tower tops, the wall walks, anywhere you can walk to -- never right next to you, so you go "
                   "looking for them; the middle of your view says what came. 0: they land on the map's own item "
                   "points (vr_arena_scatter)."),
        slider("Score For A Kill", vr_arena_score_kill, 1.f, 10000.f, 5.f, "%.0f")
            .help("A kill's score, times the rung its holder was on (a boss four of that) and the difficulty: at 100 a "
                   "first rung kill is worth 75 / 100 / 125 / 150 (Easy / Normal / Hard / Nightmare), a third rung boss "
                   "900 / 1200 / 1500 / 1800; an armoured foe a quarter more per kind (green 1.25x, yellow 1.5x, red "
                   "1.75x). A wave clear adds 250 times its wave, by the difficulty too (wave 3: 563 / 750 / 938 / 1125) "
                   "(vr_arena_score_kill)."),
        header("Perks"),
        toggle("Boss Perks", vr_arena_perks)
            .help("1: clearing a boss wave offers your choice of perks -- cards floating in front of you, the arena "
                   "waiting (no countdown, no next wave) until you point at one and pull the trigger (or press its "
                   "number); that perk is yours for the rest of the run. 0: it offers nothing (vr_arena_perks)."),
        slider("Perks Offered", vr_arena_perk_count, 1.f, 8.f, 1.f, "%.0f")
            .help("How many of the pool's eight a boss clear offers, at Normal -- the difficulty takes its share (3 is 2 "
                   "at Easy, 4 at Hard, 5 at Nightmare); never the same perk twice (vr_arena_perk_count)."),
        header("Cleanup"),
        slider("Bodies Fade After", vr_arena_corpse_fade, 0.f, 60.f, 1.f, "%.0f s")
            .help("A fallen foe's body (its head, a gibbed one's) lies this long, give or take 3 s, then fades out and "
                   "is gone, as the Horde's do. 0: never by time. One in your hand, or still dying, waits for you: let "
                   "go of, it lies 3 s more (vr_arena_corpse_fade)."),
        slider("Bodies Kept", vr_arena_corpse_max, 0.f, 64.f, 1.f, "%.0f")
            .help("At most this many fallen bodies lie at once: past it the oldest fades first (one in your hand "
                   "stays: the next oldest goes). 0: no limit (vr_arena_corpse_max)."),
        slider("Supplies Last", vr_arena_item_waves, 0.f, 10.f, 1.f, "%.0f waves")
            .help("A wave clear's medkits and armour still lying there this many wave clears later fade out and are "
                   "gone (one in your hand stays). 0: they stay for the run (vr_arena_item_waves)."),
        slider("Dropped Guns Kept", vr_arena_gun_max, 0.f, 96.f, 1.f, "%.0f")
            .help("At most this many of the guns the foes dropped lie about: past it the oldest fades away. Never a "
                   "gun in your hands or holsters, one you dropped or threw, one carried, pulled by a force grab or in "
                   "flight. The Spoils perk keeps twice as many. 0: no limit (vr_arena_gun_max)."),
        header("This Run"),
        slider("Wave Seed", vr_arena_seed, 0.f, 1000.f, 1.f, "%.0f")
            .help("0: a different ladder of guns every run; a number: that seed's waves, so one difficulty's run can be "
                   "compared against another's (vr_arena_seed). Not saved: set it every map."),
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
        open("AI Behaviour", pageIndex(pageArmedAI))
            .help("Their fighting: aggressiveness, accuracy, reaction time, fire rate, retreat, their fighting "
                  "distance and aim, weapons lying about, their loadout and parries, and how much the skill "
                  "level changes them (Difficulty Impact). Also their strafe lean."),
        open("Armed Humanoid Replacements", pageIndex(pageArmedRepl))
            .help("A map's grunts and enforcers as armed ones holding their own guns (their balance kept), and weapon "
                  "variety on top. All off by default."),
        open("Armour", pageIndex(pageArmedArmour))
            .help("Armed grunts wearing breakable armour (a cuirass, a helmet, pauldrons, bracers, thigh and shin "
                  "plates): how it absorbs, helmets and headshots, cuts, and how often they wear it at each skill."),
        open("Worn Backpacks", pageIndex(pageArmedPacks))
            .help("Experimental: armed grunts wearing their backpack, ragdolls too, torn off by pulls, landings and hits."),
        open("Weapon Holds", pageIndex(pageArmedHolds))
            .help("Per weapon, for grunts and for enforcers: held in one hand or two (Auto: the axe and the enforcer's "
                  "pistols in one hand, the rest as built in)."),
        open("IK Stretch", pageIndex(pageArmedStretch))
            .help("Per pose class (rifle, pistol, shoulder launcher, ...), for grunts and for enforcers: how far their "
                  "arms may lengthen to put their hands on their weapons, as your own Arm Stretch (1: none)."),
        header("Their Weapons"),
        slider("Damage to You", vr_agrunt_damage_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Their weapon's damage to you, times this (its damage to monsters is your weapon's own): a rocket "
                  "isn't a one-shot (vr_agrunt_damage_scale)."),
        slider("Spare Clips", vr_agrunt_reserve, 0.f, 10.f, 1.f, "%.0f").extend(0.f, 100.f)
            .help("The clips each carries beyond the full one in its gun; each reload takes one. With none left it throws "
                  "the gun away and fights with its fists. The guns whose clip is a shot or two carry more at the same "
                  "setting (the double-barrelled shotgun 4x, the flare gun 2x, the super nailgun 1.5x). Its backpack "
                  "holds what is left of them (vr_agrunt_reserve)."),
        slider("Dual Guns' Penalty", vr_agrunt_dual_penalty, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("Two guns held one in each hand aim as yours do in one hand: no two-hand spread reduction (spread "
                  "and aim error x2 at 1) and heavy guns kick their aim up a shot by their heft. This times those "
                  "penalties; 0: as accurate as with one gun in both hands (vr_agrunt_dual_penalty)."),
        header("Their Strikes"),
        slider("Torso Twist", vr_agrunt_strike_torso, 0.f, 2.f, 0.05f, "%.2fx")
            .help("How far its torso twists and leans in its melee strikes and punches: wound back, driven through the "
                  "blow, followed through. 0: only its arms move (vr_agrunt_strike_torso)."),
        slider("Shoulder Drive", vr_agrunt_strike_shoulder, 0.f, 2.f, 0.05f, "%.2fx")
            .help("How far the striking shoulder lifts and bends back, its hand drawn back by its head and its elbow "
                  "raised in the wind-up, then driven down through the blow (vr_agrunt_strike_shoulder)."),
        header("Disarming"),
        toggle("Hits Disarm", vr_agrunt_disarm)
            .help("A hard blow on its gun hand (a shot, a melee blow, a thrown thing) or a knockdown knocks the "
                  "weapon out of its hand, its rounds with it; your shots by Shooting It Out's rule below while that is "
                  "on (vr_agrunt_disarm)."),
        toggle("Grab Its Weapon", vr_agrunt_disarm_yank)
            .help("Your empty hand closing on its weapon takes it out of its hand: it is yours (vr_agrunt_disarm_yank)."),
        toggle("Deliberate Blows Only", vr_agrunt_disarm_deliberate)
            .help("Only a deliberate blow knocks its weapon out: a closed fist or something held, swung over more than "
                  "a frame. An open hand reaching for it, or your idle hands jolted as you are pushed or walk into it, "
                  "never (vr_agrunt_disarm_deliberate)."),
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
        slider("Tug: Let Go Past", vr_agrunt_tug_let_go, 20.f, 100.f, 5.f, "%.0f cm").extend(0.f, 300.f)
            .help("Physical tug: your hand lets go of its weapon (you lose the tug) once your real hand is this far from "
                  "where it holds it, pulled away or stepped back before its grip gives; till then your hand is drawn on "
                  "the weapon. A hard yank's pull reaches about 40 cm before it tears the weapon out "
                  "(vr_agrunt_tug_let_go; 0: 2.4 m, as before)."),
        slider("Grab: Fastest Hand", vr_agrunt_grab_speed, 0.f, 6.f, 0.1f, "%.1f m/s").extend(0.f, 50.f)
            .help("A hand moving faster than this (a punch, a swipe) doesn't grab its weapon: its speed against you or "
                  "against the weapon (going with a swing to catch it), the lesser, in real time (bullet time too). 0: "
                  "any speed (vr_agrunt_grab_speed)."),
        slider("Tug: Its Pull", vr_agrunt_tug_strength, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 3.f)
            .help("How hard its arms pull its weapon back in the physical tug (vr_agrunt_tug_strength)."),
        slider("Tug: Pull Time", vr_agrunt_tug_time, 0.f, 5.f, 0.1f, "%.1f s").extend(0.f, 30.f)
            .help("How long you must keep it pulled that far against a calm grunt at full health; less as it is hurt, "
                  "half while it flinches from a hit. 0: any pull wins (vr_agrunt_tug_time)."),
        slider("Tug: It Fights Back Every", vr_agrunt_tug_fight, 0.f, 5.f, 0.1f, "%.1f s").extend(0.f, 30.f)
            .help("About how often it fights back during the struggle: a shove or a headbutt breaks your grip, a gun "
                  "fires at you. 0: never (vr_agrunt_tug_fight)."),
        slider("Parry Disarm Chance", vr_agrunt_parry_disarm, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 1.f)
            .help("The chance your weapon's parry of its melee blow knocks its weapon out of its hand; a well-timed "
                  "parry's 1.5 times (vr_agrunt_parry_disarm)."),
        toggle("Drops It When Floored", vr_agrunt_fall_drop)
            .help("Knocked down onto the floor (not just staggered by a shove) it drops its weapon beside it, and "
                  "takes it back as it gets up unless you took it (vr_agrunt_fall_drop; with Hits Disarm)."),
        toggle("Keeps It in a Gentle Fall", vr_agrunt_fall_keep)
            .help("A gentle fall (a shove's backpedal to the floor) keeps the weapon in its hand, but while it lies "
                  "there a grab takes it at once and a blow knocks it away. A violent one (below) drops it "
                  "(vr_agrunt_fall_keep)."),
        slider("Fall: Drops It on Impact", vr_agrunt_fall_keep_impact, 0.f, 20.f, 0.5f, "%.1f m/s").extend(0.f, 100.f)
            .help("Lifted, judo-thrown, shoulder-charged or shoved over, it holds on to its weapon until its body meets "
                  "the ground this hard (the speed it loses as it lands); a gentle landing keeps it, a slam drops it. "
                  "Never while your hands hold it. 0: the older rules below instead (vr_agrunt_fall_keep_impact)."),
        slider("Fall: Flung Within", vr_agrunt_fall_keep_window, 0.f, 5.f, 0.1f, "%.1f s").extend(0.f, 60.f)
            .help("Flung counts only this long after it goes down: lying there, your body or hand nudging it as you "
                  "come to take its weapon doesn't make it let go. 0: any time while down (vr_agrunt_fall_keep_window)."),
        toggle("Drops Its Chainsaw Running", vr_agrunt_saw_drop_running)
            .help("One that was fighting you with a chainsaw drops it with the engine running (killed, disarmed, a tug "
                  "lost): it runs on as yours does when let go (Runs On When Let Go), so taken up meanwhile it needs no "
                  "cord pull. One that never saw you drops it off (vr_agrunt_saw_drop_running)."),
        header("Disarming: Shooting It Out"),
        toggle("Shoot Weapons Out", vr_agrunt_shotdisarm)
            .help("Your shots on the weapon an armed grunt holds, or the hand holding it, add up; each time they pass "
                  "the threshold within the time window, a roll may knock the weapon out of its hand, along your shots. "
                  "A close shotgun blast on the gun likely does, a quick nail burst may, a chip shot from afar seldom. "
                  "Each hand's weapon on its own. While on, your shots disarm only this way (vr_agrunt_shotdisarm)."),
        cycle("What Counts", vr_agrunt_shotdisarm_parts, {{0.f, "The Weapon"}, {1.f, "The Hand"}, {2.f, "Weapon or Hand"}})
            .help("Which hits add up: on the weapon itself, on the hand and forearm holding it, or either "
                  "(vr_agrunt_shotdisarm_parts)."),
        slider("Hit Volume Size", vr_agrunt_shotdisarm_size, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The weapon's and the hand's hit volumes times this: under 1 only precise shots count, over 1 near ones "
                  "too (vr_agrunt_shotdisarm_size)."),
        slider("Chance a Roll", vr_agrunt_shotdisarm_chance, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 1.f)
            .help("The roll's chance right at the threshold (vr_agrunt_shotdisarm_chance)."),
        slider("Chance Past the Threshold", vr_agrunt_shotdisarm_chance_scale, 0.f, 2.f, 0.05f, "+%.2f").extend(0.f, 10.f)
            .help("Added to the chance for each threshold's worth past it (half past: half of it); at most 0.95. A "
                  "shotgun blast all on the gun, 24 on 16: 0.25 + 0.4 (vr_agrunt_shotdisarm_chance_scale)."),
        toggle("Weapon Stops the Shot", vr_agrunt_shotdisarm_block)
            .help("A pellet or a nail whose first hit is the held weapon (not its hand) stops there: sparks and a tink, "
                  "no damage to the one holding it (each pellet on its own). Explosions and lightning hurt it as ever. "
                  "Off: it goes on into the body behind (vr_agrunt_shotdisarm_block)."),
        header("Their Hands"),
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
        toggle("Spent Guns Stand Out", vr_enemy_weapon_depleted_loot)
            .help("Off: an empty burst rifle, laser rifle or Arcane Dimensions soldier's gun, or an ogre's chainsaw out "
                  "of fuel, lying about (nothing ever refills them) has no loot pillar and the force grab skips it; a hand still picks it up. On: as any "
                  "weapon (vr_enemy_weapon_depleted_loot)."),
        header("Ogres' Chainsaws"),
        open("Chainsaw Engine", pageIndex(pageChainsawEngine))
            .help("The running engine's exhaust smoke and its shake: in one hand, in two, lying on the ground."),
        header("Grunts' Burst Rifles"),
        toggle("Burst Rifles", vr_grunt_burst)
            .help("On: grunts fire 3-round bursts of single bullets, tighter than a shotgun's pellets, and the gun a "
                  "grunt drops fires the same bursts. Off: id's shotgun, the grunts' and the dropped guns' (six "
                  "pellets as yours, a shell a shot: a third of Rounds)."),
        header("Spent Rifles"),
        slider("Spent Smoke", vr_enemygun_spent_smoke, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("A burst rifle or a laser rifle whose last round you fire smokes this long, in the hand or dropped: it "
                  "is done, drop it (0: never)."),
        slider("Spent Crackle", vr_enemygun_spent_crackle, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 30.f)
            .help("And crackles all over with lightning's arcs this long, as a corpse the lightning struck (0: never)."),
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
        slider("Nailgun", vr_dmg_nail, 1.f, 60.f, 1.f, "%.0f a nail").extend().help("id's 9 (a head more)."),
        slider("Super Nailgun", vr_dmg_super_nail, 1.f, 100.f, 1.f, "%.0f a nail").extend().help("id's 18 (a head more)."),
        slider("Nail Speed", vr_nail_speed, 500.f, 6000.f, 5.f, "%.0f u/s").extend(100.f, 20000.f)
            .help("The nailgun's and the super nailgun's nails, yours and the monsters' (Quake's 1000; 1750 until round "
                  "23), plus the hand's."),
        slider("Nail Pistol Speed", vr_nailpistol_speed, 500.f, 6000.f, 10.f, "%.0f u/s").extend(100.f, 20000.f)
            .help("Its nails' speed (the nailgun's Nail Speed), plus the hand's."),
        slider("Nail Pistol Cooldown", vr_nailpistol_cooldown, 0.02f, 1.f, 0.01f, "%.2f s").extend(0.02f, 3.f)
            .help("Semi-automatic: a shot each pull of the trigger, at least this long apart."),
        slider("Nail Pistol Spread", vr_nailpistol_spread, 0.f, 5.f, 0.05f, "%.2f deg").extend(0.f, 30.f)
            .help("How far its nails stray from its aim at most."),
        slider("Nail Pistol Slide Travel", vr_nailpistol_slide_travel, 0.f, 3.f, 0.1f, "%.1f units")
            .help("How far its slide goes back each shot, and stays back while it is empty (the gun's own units: it is "
                  "about 13 long). A magazine with rounds put in racks it closed."),
        toggle("Nail Pistol Screen Rides Slide", vr_nailpistol_screen_ride)
            .help("Its ammo screen, mounted on the slide's left side, goes back and home with the slide (each shot, "
                  "locked back while empty). Off: it stays put on the gun."),
        toggle("Flares Set Things on Fire", vr_flare_burn)
            .help("A flare sets what it hits on fire (monsters, corpses, crates; Combat > Burning) and sets off "
                  "explosive boxes. Off: its hit's damage only."),
        slider("Stuck Flare Burns", vr_flare_stick_time, 1.f, 120.f, 1.f, "%.0f s").extend(0.5f, 3600.f)
            .help("How long a flare stuck in a wall or lying on the floor burns, flickering red with sparks and smoke."),
        slider("Air Flare Burns", vr_flare_air_time, 1.f, 60.f, 1.f, "%.0f s").extend(0.5f, 3600.f)
            .help("How long a flare fired up into the air burns, drifting down from the top of its climb and lighting "
                  "a wide area."),
        slider("Air Flare Sinks", vr_flare_air_fall, 2.f, 100.f, 1.f, "%.0f u/s").extend(1.f, 400.f)
            .help("How fast a drifting flare sinks."),
        slider("Heavy Revolver Big Bodies", vr_revolver_heavy_mult, 1.f, 4.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A slug into a big monster (Big Body Health or more: an ogre, a fiend, a shambler) deals this many times "
                  "as much: an ogre takes 4-5 in the body."),
        slider("Heavy Revolver Cooldown", vr_revolver_cooldown, 0.05f, 1.f, 0.01f, "%.2f s").extend(0.05f, 3.f)
            .help("Double action: a shot each pull of the trigger, at least this long apart."),
        slider("Grenade", vr_dmg_grenade, 10.f, 400.f, 5.f, "%.0f").extend()
            .help("A grenade's blast at its middle, less further out (id's 120): the launcher's, a hand grenade's and an "
                  "ogre's grenade caught and thrown back."),
        slider("Rocket", vr_dmg_rocket, 10.f, 400.f, 5.f, "%.0f").extend()
            .help("A direct hit, and up to a fifth more at random (id's 100); its blast on what's around 1.2 times it."),
        header("Scourge of Armagon"),
        slider("Laser Cannon", vr_dmg_laser, 1.f, 100.f, 1.f, "%.0f a bolt").extend()
            .help("Each bolt (18; one in ten a third more). A bolt bouncing back into what it hit does half."),
        header("Dissolution of Eternity"),
        slider("Lava Nails", vr_dmg_lava_nail, 1.f, 60.f, 1.f, "%.0f a nail").extend()
            .help("A nail to a monster (15; to a player 3/5 of it)."),
        header("Melee"),
        slider("Crowbar", vr_crowbar_damage, 1.f, 100.f, 1.f, "%.0f").extend()
            .help("A blow (20, the axe's: 25 before), before its speed and weight; its hook hits 20% harder, its chisel "
                  "end 40% softer. Crowbars lie in the firing range's prop area."),
        header("Enemy Weapons"),
        slider("Knight's Sword", vr_sword_damage_mult, 0.5f, 3.f, 0.05f, "%.2fx 20").extend()
            .help("Knights and hell knights always drop their sword: a swing's damage, times 20 (1.5: 30), before its "
                  "speed and weight; the hell knight's sword 25% more."),
        slider("Grunt's Burst Rifle", vr_gruntgun_damage, 1.f, 30.f, 1.f, "%.0f a round").extend()
            .help("Grunts always drop their gun: a trigger pull fires a 3-round burst, each round this much damage (a "
                  "head more; a grunt has 30 health). Burst Rifles off: the Shotgun's pellets."),
        // AD support: Arcane Dimensions' weapons (QC vr_ad_bridge.qc, vr_ad_enemyguns.qc, vr_ad_melee.qc): the upgrades
        // you find, the soldiers' guns and the melee monsters' weapons they drop (their other numbers: Arcane
        // Dimensions' Guns).
        header("Arcane Dimensions"),
        open("Arcane Dimensions' Guns", pageIndex(pageAdGuns))
            .help("The soldiers' guns' every number: rounds, rate, speed, blast, fuel, reach, poison."),
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
        header("Gunfire"),
        toggle("Punchier Gunfire", vr_gunsfx)
            .help("The extras below for every gun, yours and the enemies'. Off: every shot, click and buzz as before."),
        slider("Shot Layers", vr_gunsfx_layers, 0.f, 2.f, 0.1f, "%.1f")
            .help("Under each shot a close mechanical layer (a hammer, a feed's tick, a launcher's clunk) and a low thump: "
                  "how loud (0 none)."),
        slider("Room Tail", vr_gunsfx_tail, 0.f, 2.f, 0.1f, "%.1f")
            .help("Each shot's tail sized to where it is fired: short and tight in a small room, long in a big hall, long "
                  "with a distant echo under the open sky. How loud (0 none)."),
        slider("Shot Variation", vr_gunsfx_vary, 0.f, 10.f, 1.f, "%.0f%%")
            .help("Each shot (and its layers) a little higher or lower, at random up to this much, and half as much louder "
                  "or quieter, so a stream of shots isn't one recording again and again (0 off)."),
        toggle("Casings by Surface", vr_gunsfx_casings)
            .help("Spent casings ring on metal and knock on wood; stone as before, water's plips as before. Off: the one "
                  "tink everywhere."),
        slider("Surface Impacts", vr_gunsfx_impacts, 0.f, 2.f, 0.1f, "%.1f")
            .help("A bullet or a nail hitting a wall sounds of what it hit, from where it hit: a dull thud on stone, "
                  "duller on dirt, a ricochet's ping on metal, a knock on wood (a nail's lighter, under its own tink). "
                  "A shotgun's pellets or a nailgun's stream make one or two at a time, not a dozen. How loud (0 none)."),
        slider("Dry Fire Click", vr_gunsfx_dryfire, 0.f, 2.f, 0.1f, "%.1f")
            .help("An empty gun's click: how loud, with a small tick in that hand (0 silent)."),
        toggle("Actions From the Action", vr_gunsfx_action)
            .help("A gun's mechanical sounds (breaking open and closing, a magazine in or out, a shell in) heard from that "
                  "part of the gun, not its muzzle."),
        slider("Haptic Punch", vr_gunsfx_punch, 0.f, 2.f, 0.1f, "%.1fx")
            .help("Each shot punches the controller: a sharp spike, then a short rumble as long as the gun's recoil takes to "
                  "come back, as hard as its recoil (each weapon's own: Weapon Offsets > Effects > Haptic Punch). 0: the "
                  "old flat buzz."),
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
        slider("Melee Tolerance", vr_hit_tolerance_melee, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to your melee blows and shoves."),
        toggle("Head Priority", vr_hit_head_priority)
            .help("Where a hit point, or a whole melee sweep's way through a body, lies in more than one zone, the "
                  "head's zone decides it. Off: the limb and leg zones claim it first."),
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
    };
}

// Blocking and shoving with a guard, and counter-attacks after a parry (the parry settings of Melee joined them).
[[nodiscard]] za::Vector<Item> pageParryBash()
{
    return {
        header("Parry"),
        toggle("Parry", "vr_parry").help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block monsters' melee blows."),
        slider("Parry Stagger", vr_parry_stagger, 0.1f, 1.5f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("With Parry Balance off: how long the monster pauses after a successful parry before it can move and "
                  "attack again."),
        toggle("Parry Stagger as Ragdoll", vr_parry_ragdoll).extend()
            .help("A parried humanoid (its monster group's Ragdoll Triggers on: Combat > Monster Groups) staggers back as an active ragdoll, as a shove's stagger, for Parry Stagger; on level ground it never falls, with stairs, a ledge or a slope behind it it may go down. Off: its pain animation's stagger (Stagger Frames, Stagger Sway)."),
        slider("Parry Pushback", vr_parry_push_enemy, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("How far a parry pushes the attacker back (times Knockback; the distance goes about as its square: "
                  "0.55 about half as far as 0.8). More makes room; less keeps it within reach of a counter-attack. "
                  "Separate from shoves (Shove and Bash Push below)."),
        slider("Parry Damage Reduction", "vr_parry_reduction", 0.f, 1.f, 0.05f, "%.2f").help("Share of a parried blow's damage taken away."),
        slider("Parry Drop Chance", "vr_parry_drop_chance", 0.f, 1.f, 0.05f, "%.2f").help("Chance a one-handed parry knocks the weapon out of your hand (two hands: never). Not used with Parry Stamina on (Parry, Bash and Headbutt), which replaces it."),
        slider("Parry Arm Knock", "vr_parry_wobble", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much a parried blow knocks your hand and arm."),
        toggle("Unarmed Parry", vr_parry_unarmed).help("Cross your arms in an X in front of you to block a blow with your forearms."),
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
        slider("Stagger at 1", vr_parry_stagger_min, 0.1f, 2.f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("Strength over poise 1: it staggers back as a ragdoll this long (to Stagger at 2 at 2)."),
        slider("Stagger at 2", vr_parry_stagger_max, 0.1f, 2.f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("Strength over poise 2: its ragdoll stagger. Up to 2 it never goes down."),
        slider("Heavy Stagger", vr_parry_stagger_heavy, 0.1f, 3.f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("Strength over poise 3 and over: its stagger (from Stagger at 2 over 2); over 2 it may go down stairs, "
                  "a ledge or a slope behind it."),
        slider("Poise Against Melee", vr_poise_chances, 0.f, 2.f, 0.1f, "%.1f").extend(0.f, 5.f)
            .help("A heavy group (Parry Poise over 1; Off as 3): your shoves', blows' and flung props' chances to knock "
                  "it down or unbalance it divided by its poise to this power, so weapons and explosives stay the "
                  "answer to ogres and the like (0: not)."),
        header("Bash and Shove"),
        toggle("Bash", vr_bash).help("The parry stance (a weapon level across in front, one hand or two) pushed straight forward bashes: knocks monsters back and staggers them. Open palms facing a monster pushed at it shove it. Two hands (a weapon held two-handed, a palm pushing on the blade, both palms) push harder and further."),
        slider("Bash Speed", vr_bash_speed, 0.3f, 3.f, 0.1f, "%.1f m/s").extend().help("How fast the stance (a weapon level across, held half a second) must be pushed forward, both its ends going ahead. A swing passing through the stance doesn't bash."),
        slider("One-Hand Push", vr_shove_push_onehand, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A one-handed shove's or bash's push, times Shove and Bash Push."),
        slider("Two-Hand Push", vr_shove_push_twohand, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("A two-handed shove's or bash's push (both palms, or a weapon held in two hands), times Shove and "
                  "Bash Push. Its distance goes by the push's square (1.12: 1.25 times as far as 1)."),
        slider("Bash and Parry Sounds", vr_bash_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the sounds that tell a shove, a weapon bash, a counter bash (a bash right after a parry) and a parry apart from your blows (0: silent)."),
        header("Counter-Attacks"),
        toggle("Counter-Attacks", vr_counter)
            .help("After a parry (a weapon's or crossed arms), your next melee attack in the window is a counter and hits harder: a blow with either hand, a bash or a shove. One a parry."),
        slider("Counter Sounds", vr_counter_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of a blade's shing as the window opens and of the heavy strike as a counter lands (0 off)."),
        toggle("Counter Window Glow", vr_counter_glow)
            .help("While the counter's window is open, what your hands hold (a bare fist too) glows gold round its edges and sheds golden embers, fading as the window closes. Off as shipped."),
    };
}

// Split from Parry, Bash and Headbutt: the stamina parries, shoves and blows cost.
[[nodiscard]] za::Vector<Item> pageStamina()
{
    return {
        header("Parry Stamina"),
        toggle("Parry Stamina", vr_parry_stamina)
            .help("Parrying tires you: each parry with a weapon costs stamina (less with the weapon in two hands), and the parry that leaves you none knocks the weapon out of your hand. It comes back once you stop parrying for a moment. On, it replaces the Parry Drop Chance (Melee Settings). Crossed arms cost nothing."),
        open("Stamina on the Gadget: Screens", pageIndex(pageScreens)).help("What the gadget shows (stamina and counters) is on HUD and Menus > Screens."),
        header("Shove and Strike Stamina"),
        toggle("Shove Stamina", vr_shove_stamina)
            .help("Shoves and bashes that land cost stamina from the same pool as parries. Without enough left, they throw back less and hurt less (Exhausted Knockback, Exhausted Damage)."),
        toggle("Strike Stamina", vr_strike_stamina)
            .help("Blows that land on something that takes damage cost stamina from the same pool, once a swing, however many things it goes through. Without enough left, they hurt less (Exhausted Damage). Blows on walls cost nothing."),
                header("Climbing Stamina"),
        toggle("Climbing Stamina", vr_climb_stamina)
            .help("Hanging from a ledge or a rung (Climbing) spends stamina from the same pool, and none comes back while you hang. With none left your hands let go. More on the Climbing page."),
        header("Tired Arms"),
        slider("Shake Distance", vr_fatigue_shake, 0.f, 2.f, 0.1f, "%.1f cm").extend(0.f, 10.f).help("How far your drawn hands, arms and what they hold shake at most, with no stamina left. 0: no moving shake."),
        slider("Shake Turn", vr_fatigue_shake_angle, 0.f, 5.f, 0.25f, "%.2f deg").extend(0.f, 20.f).help("How far they turn as they shake at most, with no stamina left. 0: no turning shake."),
        toggle("Shake Whenever Tired", vr_fatigue_shake_always).help("Off: your arms shake only while you hang from a ledge or a rung. On: whenever your stamina is low (after parries, shoves and blows too)."),
        header("Tired Legs"),
        toggle("Slower When Tired", vr_stamina_speed)
            .help("As your stamina runs low, you can't run as fast: full speed from half your stamina (From Stamina, on the Aiming page's Tired Arms) down to Slowest Run with none left, on the same curve as the weight."),
        open("Heavy When Tired (Aiming)", pageIndex(pageAimingSettings)).help("Tired hands get heavy too (what they hold, and an empty hand): Tired Arms, on the Aiming page."),
    };
}

// Batting projectiles back (Parry, Bash and Headbutt) and catching grenades (Carrying and Gibs).
[[nodiscard]] za::Vector<Item> pageBatting()
{
    return {
        header("Batting Projectiles"),
        toggle("Bat Back Projectiles", vr_deflect).help("Swing a weapon (a gun, a melee weapon, or a club: a wall torch, a brick) through a monster's spike, laser, spit or grenade to bat it away as a bat hits a ball: off the weapon's face and the way it swings, faster the harder you swing. A bash sends it the way you push. Empty hands never bat: they catch grenades."),
        slider("Batting Swing Speed", vr_deflect_speed, 0.2f, 1.5f, 0.05f, "%.2fx").extend().help("How fast a batting swing must be, times Swing Speed (a hit needs 1x, and more for a swung weapon)."),
        slider("Batting Timing", vr_deflect_window, 0.f, 0.5f, 0.05f, "%.2f s").extend().help("How early you may swing: the weapon's path keeps batting this long after it passed."),
        toggle("Batting Needs a Swing", vr_deflect_swing).extend().help("Only a real swing bats: the hand itself moving fast (Batting Hand Speed), from some way off, not a weapon waved back and forth. Off: any weapon moving fast enough at its end bats, a turn of the wrist or a gun brought round to aim too."),
        slider("Bash Batting Timing", vr_bash_deflect_window, 0.f, 1.f, 0.05f, "%.2f s").extend().help("How long a bash goes on batting after the push."),
        header("Grenades"),
        cycle("Catch Grenades", vr_grenade_catch, {{0.f, "Off"}, {1.f, "Ogres'"}, {2.f, "Ogres' and yours"}})
            .help("Grenades bounce and roll as physics objects, and you can catch them (by hand or force grab) and throw them "
                  "back; thrown, they burst on a monster as an ogre's does on you. \"And yours\" (the default): your grenade "
                  "launcher's too."),
        toggle("Fuse Resets Every Catch", vr_grenade_fuse_regrab)
            .help("Off: the fuse is set once, as you catch the monster's throw; dropped and caught again, it keeps running. On: "
                  "every catch sets it again."),
        toggle("Returned Grenades Hit Like Yours", vr_grenade_return_full)
            .help("A grenade you throw or bat back (an ogre's or your own) goes off as your grenade launcher's: its damage and "
                  "radius, credited to you, with your Quad. Off: an ogre's keeps its own, weaker blast."),
        toggle("Shoot Grenades", vr_grenade_shoot)
            .help("A shot, a nail, a rocket, the lightning, a thrown thing or another blast sets a grenade off where it is: "
                  "an ogre's in flight, yours on the ground, a hand grenade's dud left as a trap. Whoever set it off gets "
                  "the kills. Not one in your hand."),
        toggle("Blows Set Grenades Off", vr_grenade_shoot_melee)
            .help("A very strong melee blow (the speeds and damage below) or a running chainsaw sets an enemy's grenade "
                  "off. Any other blow bats it away: one lying or rolling flies off the way the blow went; one in flight "
                  "is batted by a weapon's swing and caught by an empty hand. Your own grenades your blows pass through. "
                  "Off: blows pass through grenades."),
        header("Hand Grenades"),
        toggle("Hand Grenades", vr_handgrenade)
            .help("Reach for the pouch on your lower back with an empty hand and grip: a grenade from it, while you have "
                  "rockets (the grenade launcher's ammo; one leaves your ammo with each grenade). Throw it as anything you "
                  "carry: it goes off as the launcher's grenade. Where the pouch is: below."),
        cycle("Arm Hand Grenades", vr_handgrenade_arm, {{0.f, "Trigger pulls the pin"}, {1.f, "When let go of"}})
            .help("Trigger: press it while holding the grenade to pull the pin (it fizzes; the fuse runs); let go of unarmed, "
                  "it is a dud you can pick up again. When let go of: the lever flies off as it leaves your hand. Either "
                  "way, let go of unarmed at the pouch, it goes back in."),
        header("Grenade Pouch"),
        open("Grenade Pouch: Hip Holsters", pageIndex(pageHipHolsters)).help("Where the grenade pouch is and how a grenade sits in the hand: Weapons > Hip Holsters."),
        open("Pouch Turn (Hip Holsters)", pageIndex(pageHipHolsters))
            .help("The same place, and how the pouch is turned, with the hip holsters."),
    };
}
