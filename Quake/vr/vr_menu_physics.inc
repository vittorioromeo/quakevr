// vr_menu_physics.inc -- throwing, carrying, burning (wall torches, AD), rocks and bricks, crates, gibs, corpse damage,
// the ragdoll pages. Part of vr_menu.cpp (included where it was: the same translation unit, so its anonymous
// namespace).

[[nodiscard]] za::Vector<Item> pageThrowing()
{
    return {
        slider("Two-Hand Throw Speed", vr_2h_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend(),
        slider("Two-Hand Throw Damage", vr_throw_2h_damage, 0.5f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A weapon or prop thrown with both hands: its hits' damage times this (one hand: 1x). 1.25 as shipped."),
        toggle("Analog Release", vr_throw_release)
            .help("A throw lets go as the grip starts to open, not only once it is released."),
        slider("Slow Motion: Full Stroke Tempo", vr_throw_slowmo_stroke_time, 0.04f, 0.2f, 0.005f, "%.3f s").extend()
            .help("In slow motion (bullet time), a quick toss, flick or nudge goes as far as you made it, and a full "
                  "throw made slowly with the slowed world goes as far as at full speed: the slower your stroke against "
                  "a real-speed one of its length, the more it is raised. This is a full arm throw's stroke at real "
                  "speed (its path over its peak speed). Lower: slowed throws reach full strength sooner, real-speed "
                  "throws in bullet time are raised a little more."),
        toggle("Slow Motion: Throws As In Real Time", vr_throw_slowmo_real_strength)
            .help("In slow motion, every throw is as strong as the same motion at full speed: never raised. A throw "
                  "you make slowly with the slowed world then goes as slowly as you made it. Off: raised by the "
                  "stroke's tempo (Full Stroke Tempo)."),
        toggle("Spin From Controller Turn", vr_throw_spin_from_pose)
            .help("A throw's spin from how the controller turned, not the runtime's angular velocity (Virtual Desktop "
                  "reports it in the controller's frame: a flick facing away from the play space's front spun throws "
                  "sideways)."),
        cycle("Controller Spin Frame", vr_angvel_frame,
            {{-1.f, "Auto"}, {0.f, "Tracking Space"}, {1.f, "Controller's"}, {2.f, "From Turn"}})
            .help("Which frame the runtime's controller angular velocity is in (melee, flick reload, throws). Auto: "
                  "Virtual Desktop's controller frame, else the tracking space, checked against how the controllers "
                  "turn. Tracking Space: as before. From Turn: ignore it, use the controller's turn."),
        header("Throws by Weight"),
        slider("Wrist to Controller", vr_throw_wrist_dist, 0.f, 0.15f, 0.005f, "%.3f m")
            .help("From the wrist to the controller's point: the turn of the hand about the wrist gives the controller "
                  "this lever's speed, the part of a throw a heavy thing keeps less of."),
        header("Physics"),
        slider("Spin Drag", vr_throw_spin_drag, 0.f, 2.f, 0.05f, "%.2f").extend(),
        header("Thrown Axes"),
        toggle("Axes Stick", vr_axestick)
            .help("A thrown axe that strikes blade first sticks in walls, doors, props and monsters (it moves with them). "
                  "Grip it, or force grab it (it tugs, then comes free), to pull it out. Off: it bounces off."),
        header("Flung Props"),
        toggle("Flung Props Hurt", vr_prop_impact_damage)
            .help("A prop you didn't throw (swung on the grapple's rope, batted, knocked flying) hurts the monster it flies "
                  "into fast enough. Off: only what you throw by hand."),
        toggle("Flung Props Hurt Players", vr_prop_impact_players)
            .help("They hurt players too (never you as you let go of it, bat it or while your grapple holds it). Off: "
                  "monsters only."),
        slider("Fresh Gibs Harmless For", vr_gib_spawn_harmless, 0.f, 2.f, 0.1f, "%.1f s")
            .help("A gib, head or small gib just torn out can't hurt you or a monster as a flung prop for this long (gibbing a "
                  "body you stand on). Thrown by you after it, it hurts as ever."),
        slider("Monster Drops Harmless For", vr_prop_drop_grace, 0.f, 2.f, 0.1f, "%.1f s")
            .help("A weapon or a backpack a monster drops as it dies can't hurt players for this long (it may land on "
                  "you); after it, see Monster Drops Hurt Only Falling."),
        toggle("Monster Drops Hurt Only Falling", vr_prop_drop_falls_only)
            .help("After that, a monster's weapon or backpack hurts you only falling on you or once a hand has thrown it: "
                  "never pushed or tossed into you from under you (a monster killed at your feet). Off: as any flung prop."),
        toggle("Monster Drops Pass Through Bodies", vr_prop_drop_pass_inside)
            .help("A monster's drop made inside you or a monster (killed at your feet) passes through it until clear, "
                  "instead of being pushed out hard."),
        slider("Least Speed", vr_prop_impact_min_speed, 2.f, 20.f, 0.5f, "%.1f m/s").extend(0.f, 50.f)
            .help("How fast it must fly into what it hits to hurt it: less for things over 10 kg. The damage grows with "
                  "the speed over it."),
        slider("Damage", vr_prop_impact_mult, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("All their damage: the throw's (Thrown Box Damage, a gib's, a weapon's) at the least speed and the "
                  "reference mass."),
        slider("Most Speed Multiplier", vr_prop_impact_speed_max, 1.f, 6.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("Faster hits hurt more, up to this many times the damage at the least speed."),
        slider("Weight Curve", vr_prop_impact_weight_curve, 0.f, 1.5f, 0.05f, "%.2f").extend(0.f, 3.f)
            .help("How much mass changes the damage: 0 not at all, 1 in proportion (twice the mass, twice the damage)."),
        slider("Reference Mass", vr_prop_impact_weight_ref, 0.5f, 20.f, 0.5f, "%.1f kg").extend(0.1f, 100.f)
            .help("A prop of this mass deals the damage as set; lighter ones less, heavier ones more."),
        slider("Most Weight Multiplier", vr_prop_impact_weight_max, 1.f, 5.f, 0.1f, "%.1fx").extend(1.f, 20.f)
            .help("The most a heavy prop multiplies the damage."),
        slider("Least Mass", vr_prop_impact_min_mass, 0.f, 10.f, 0.1f, "%.1f kg").extend(0.f, 100.f)
            .help("Lighter props never hurt when flung (thrown by hand, they still do)."),
        header("Shots Push Props"),
        slider("Shot Push", vr_shot_push, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("Pellets, nails and the lightning beam push the boxes, gibs and weapons they hit or pass through (monsters' "
                  "shots too): times every push below. 0: off. Explosions push things anyway."),
        header("Wall Buttons"),
        toggle("Thrown Things Press Buttons", vr_button_throw)
            .help("A weapon, rock, box or gib thrown at a wall button presses it. Buttons you must shoot are still shot."),
    };
}

// Carrying ammo and health boxes, explosive boxes, armour and pickups.
[[nodiscard]] za::Vector<Item> pageCarrying()
{
    return {
        header("Carrying Boxes"),
        toggle("Carry Ammo and Health", vr_carry)
            .help("Grip a box or a backpack to carry it, push it with a hand or gun. Off: touching takes it."),
        cycle("Take a Box", vr_carry_take, {{0.f, "At a holster"}, {1.f, "Trigger"}, {2.f, "Either"}})
            .help("At a holster: let go of it at a hip or shoulder holster to put it in your pack."),
        cycle("Auto-Collect", vr_carry_autocollect, {{0.f, "Off"}, {1.f, "Ammo and Health"}, {2.f, "And Backpacks"}})
            .help("An ammo or health box (and a backpack) is taken the moment a hand grips it or a force grab brings it "
                  "in, seen shrinking into the hand. Full up of it: the full cue, and it stays in the hand."),
        toggle("Full Cue", vr_carry_full_cue)
            .help("A box put away (or taken at once) while you are full up of it: a dull knock and a tick in the hand, "
                  "so it is not taken for a miss of the holster. It drops there."),
        toggle("Put-Away Transition", vr_collect_fx)
            .help("What you put away (a box or backpack at a holster, a key, a rune or a power-up, an ammo box or a round "
                  "at the ammo pouch, a grenade at its pouch) is seen shrinking into the holster or pouch, following you, "
                  "instead of vanishing. Only how it looks: it is yours the moment you let go, as before."),
        slider("Put-Away Time", vr_collect_fx_time, 0.05f, 0.6f, 0.05f, "%.2f s").extend(0.f, 2.f)
            .help("How long it takes to go in (slowed in bullet time)."),
        slider("Put-Away End Size", vr_collect_fx_size, 0.05f, 1.f, 0.05f, "%.2fx")
            .help("How small it is as it goes in: its size then, of its own."),
        toggle("Weapons by the Fist", vr_weapon_grab_drawn)
            .help("A weapon lying on the floor is taken where the fist touches it: a crowbar by its bar, a sword by its "
                  "blade. Off: only with the hand at its handle."),
        slider("Weapon Grab Slack", vr_weapon_grab_slack, 0.f, 10.f, 0.5f, "%.1f cm")
            .help("With Weapons by the Fist: a weapon lying on the floor is taken with the fist this far off it too (a gun "
                  "lying flat is thinner than the lowest your fist gets over it). Not one in the air: caught by the fist on it."),
        slider("Handle Grab Leniency", vr_weapon_grab_handle_leniency, 0.f, 10.f, 0.5f, "%.1f cm")
            .extend(0.f, 20.f)
            .help("A gun lying on a table or the floor is taken by its handle with your fist this much farther off it than "
                  "the slack above (the hand at its handle; your hand nudging it doesn't stop it). Only a direct grab: a "
                  "weapon caught in the air has Mid-Air Catch Leniency, and a force grab is unchanged. 0: as anywhere on it."),
        slider("Mid-Air Catch Leniency", vr_weapon_catch_handle_leniency, 0.f, 15.f, 0.5f, "%.1f cm")
            .help("A weapon in the air (tossed up to re-grip it, flipped, spinning) is caught by its handle with your fist "
                  "this much off it. Not a force grab's pull. 0: only by your fist on it."),
        toggle("Floating Pickup Grace", vr_pickup_float_grace)
            .help("A weapon, key or armour hanging in the air (spinning) isn't knocked down by your empty hand reaching into "
                  "it slowly for a moment: close your grip and it is taken where it hangs. Off: a touch knocks it loose."),
        toggle("Weapons by Their Hotspots", vr_weapon_grab_hotspots)
            .help("Grip a weapon on the floor nearer one of its other grips than its handle (a chainsaw's front handle, "
                  "a crowbar's bar, a sword's blade, a gun's pump) and you carry it by that grip, as when your other hand "
                  "lets go of a weapon held in both: grip its handle with the other hand to use it. Not a pistol's "
                  "two-handed cup grip. A force grab always takes the handle. Off: always by the handle."),
        toggle("Weapons Anywhere", vr_weapon_grab_anywhere)
            .help("Grip a weapon anywhere away from its handle and its other grips and you hold it there, as a box or a gib: "
                  "off the floor as it lies, with your other hand on one you hold (Other Hand Anywhere), with both hands "
                  "on one you carry. Held off its handle it doesn't fire, but strikes and throws; grip its handle to use "
                  "it. Holstered, it is drawn by its handle. Off: only by the handle and the other grips."),
        slider("Anywhere: Away From Grips", vr_weapon_grab_anywhere_min, 0.f, 30.f, 1.f, "%.0f cm")
            .help("How far from a weapon's handle and its other grips (a foregrip, a pump, a blade) you must grip it to hold "
                  "it anywhere: nearer, the handle or that grip takes it, so they stay easy to take."),
        cycle("Other Hand Anywhere", vr_weapon_anygrip_mode, {{0.f, "As a Foregrip"}, {1.f, "Support Only"}, {2.f, "Rigid"}})
            .help("Your other hand gripping a weapon you hold by its handle, away from its grips. As a Foregrip: both hands "
                  "aim it, from wherever it holds it. Support Only: it bears the weight, the aim stays your weapon hand's. "
                  "Rigid: the weapon follows both hands as a box held in both. Each weapon may override it (Weapon "
                  "Offsets: Other Hand Anywhere)."),
        toggle("Drawn In the Hand", vr_carry_local)
            .help("What you carry is drawn in your hand as it is this frame: no lag or lead as you walk or turn. Off: where the server has it."),
        toggle("Two-Handed Carrying", vr_carry_two_hands)
            .help("Grip what your other hand carries to hold it in both: it moves and turns with both hands, and letting go "
                  "of both together throws it. Let go with one and the other keeps it (to pass it from hand to hand)."),
        toggle("Two-Handed Stops at Walls", vr_carry_two_hands_solid)
            .help("What you hold in both hands stops at walls, floors and doors however hard you push, and slides along "
                  "them; it keeps its turn where a new one would put it in. Off: it goes where your hands put it."),
        toggle("Carried Things Stop at Your Body", "vr_carry_body_collision")
            .help("Things you carry stop at your torso. Your hands follow their grips as you press them against it."),
        slider("Carry Load: Mass", "vr_carry_move_mass", 0.f, 300.f, 10.f, "%.0f kg")
            .help("Carried mass that halves stick walking speed. Both hands are counted; 0 disables the mass penalty."),
        slider("Carry Load: Size", "vr_carry_move_size", 0.f, 3.f, 0.05f, "%.2f m")
            .help("Total carried length allowed without a bulk penalty; twice this halves stick walking speed. 0 disables the size penalty."),
        slider("Carry Load: Minimum Speed", "vr_carry_move_min", 0.1f, 1.f, 0.05f, "%.2fx")
            .help("Minimum fraction of stick walking speed under load; 1 disables the slowdown."),
        toggle("Fit to the Hand", vr_held_surface_fit)
            .help("A box, backpack or gib you grip sits against your curled fingers, by its drawn shape. Off: it stays where you gripped it."),
        toggle("Keep Fingers Out of Props", vr_grip_fit)
            .help("Once, as you grip a prop (or take it with your other hand too): if your drawn hand (palm, back, wrist, "
                  "fingers as they will grip) would sink into it, it is moved out of your hand the least way that leaves the "
                  "hand resting on its surface, and kept there while you hold it. Not for weapons, nor props with a Fixed or "
                  "Along the Handle grip."),
        toggle("Held Things Collide", vr_held_collide)
            .help("Things you hold one in each hand, or a thing in one hand and a weapon in the other, bump into each other "
                  "instead of passing through: each hand is drawn moved back with what it holds, a short buzz as they meet. "
                  "Your real hands are never held back."),
        toggle("Empty Hand Stays Out of Held Props", vr_hand_collide_props_keep_out)
            .help("With Empty Hand Against Held Things: an empty hand pushed into a prop your other hand holds is drawn "
                  "resting on its surface however deep you push, and gripping it there takes it in both hands from the "
                  "outside. Hitting it and throwing it are your real hand's. Off: past the push the hand sinks in, as "
                  "with a weapon."),
        toggle("Fingers Rest on Held Things", vr_hand_collide_fingers)
            .help("With Empty Hand Against Held Things: the empty hand's fingers rest on what the other hand holds or bend "
                  "out of it, instead of the whole hand being pushed by its fingertips."),
        toggle("Weapons Slide Along Walls", vr_gun_wall_slide)
            .help("A weapon you hold into a wall, a floor or a table slides along it: lowered onto a table it rests on the top, "
                  "pushed into a wall it is held off along the wall. Off: the old push-back, the hand moved back along the aim "
                  "until the muzzle stops at the wall."),
        toggle("Held Things Stop at Walls", vr_held_collide_walls)
            .help("A thing you hold in one hand stops at walls, floors and doors: it and your hand are drawn held out of them, "
                  "a short buzz as it touches. Looks only: your real hand is never held back."),
        toggle("Things Rest on Hands", vr_model_collide_rest)
            .help("A thing lying on your open palm, fist or gun (let go of there, or balanced) never pushes the drawn hand away: "
                  "the hand stays under it. Off: it pushes the hand out of it as other things do."),
        toggle("Held Things Stop at Monsters", vr_held_collide_monsters)
            .help("A thing you hold in one hand and swing as a club stops at the monster's body, as weapons do (held out "
                  "up to vr_model_collide_max, 20 cm), instead of passing through it. It still hits, and is never dropped for "
                  "it."),
        toggle("Hands Push and Hold Things", vr_box3d_hand_props)
            .help("Your empty hands are solid to loose things: they push them, and what you let go of on an open palm turned "
                  "up stays there. Grenades pass through (your palm catches them)."),
        toggle("Push Boxes With the Fist", vr_box3d_hand_push_fist)
            .help("Your hands push explosive boxes where the drawn fist meets them. Off: a small ball at the hand's point, "
                  "which met a box before the fist did."),
        toggle("Weapons Push Things", vr_box3d_weapon_push)
            .help("The weapons you hold are solid to loose things as drawn: shove things with a gun, balance them on it, bat a "
                  "grenade away with the axe."),
        cycle("Guns' Shape", vr_box3d_gun_pieces, {{1.f, "One Hull"}, {6.f, "6 Pieces"}, {12.f, "12 Pieces"}, {20.f, "20 Pieces"}})
            .help("The guns' solid shape, held and lying about: in convex pieces that follow the drawn gun (a shell drops "
                  "into the shotgun's port, a magazine into its well, nothing rests on the air between its parts), or one "
                  "hull round each (which fills them). For guns made from then on (picked up, dropped)."),
        slider("Throw Grace", vr_box3d_throw_grace, 0.f, 0.5f, 0.05f, "%.2f s")
            .extend(0.f, 2.f)
            .help("How long a thing you throw passes through your hands (and the weapon you hold) as it leaves them: it keeps "
                  "the direction and spin you threw it with instead of being knocked by the hand still moving. 0: off."),
        toggle("Throw Grace for the Body", vr_box3d_throw_grace_body)
            .help("During Throw Grace the thing also passes through your body (an overhead wrist snap throwing it down "
                  "across you)."),
        toggle("Pressing Props: Hand Gives", vr_box3d_press_back)
            .help("A hand, a weapon or a held thing pressed into a crate that has nowhere to go (on the floor, against a wall) "
                  "stops on its surface, drawn there too, instead of pushing the crate into the floor (where it was squeezed "
                  "out and flew off or broke)."),
        slider("Throws Keep Your Motion", vr_throw_inherit, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of your own velocity a thing or weapon you throw takes with it (1: all: a brick thrown running "
                  "forward flies further, running back shorter). 0: none."),
        slider("Box Punch Damage", vr_carry_melee_mult, 1.f, 3.f, 0.1f, "%.1fx").extend().help("Punching with a box in hand."),
        open("Flung Props (Throwing and Physics)", pageIndex(pageThrowing))
            .help("Whether props batted, knocked flying or swung on the grapple hurt monsters (and players), and how much."),
        open("Held Object Offsets (Held Prop)", pageIndex(pageHeldObjectOffsets))
            .help("The grip, fingers and melee points of what a hand carries."),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights))
            .help("The mass, spring and damage of what a hand carries (Aiming: Weight for how weight feels)."),
        header("Physics Sounds"),
        slider("Physics Sounds", vr_physsound, 0.f, 2.f, 0.1f, "%.1f")
            .help("Volume of what things say as physics moves them: knocks as they land, bounce and hit each other, "
                  "scrapes as they slide, a climbing hand taking a hold (0 off; over 1 louder than recorded, up to 2). By what they are made of: wood (crates, "
                  "torches), metal (weapons, ammo, armour), stone, brick, flesh (gibs, heads), a backpack's thud."),
        open("Climbing Grab Sound: Climbing", pageIndex(pageClimbing)).help("The climbing grab sound is on Movement > Climbing."),
        header("Explosive Boxes"),
        toggle("Physics Explosive Boxes", vr_explobox_physics)
            .help("The explosive boxes can be pushed, tipped over, stacked and carried by hand (heavy; never force "
                  "grabbed). Off: fixed in place, as in id's Quake. Next map."),
        slider("Blow Up On Impact", vr_explobox_impact, 0.f, 30.f, 1.f, "%.0f m/s").extend(0.f, 100.f)
            .help("A box hitting something this hard blows up: dropped from about 85 units or more (tipped over, its "
                  "top lands at 11 m/s). 0: never."),
        header("Armour and Pickups"),
        cycle("Armour", vr_armor_wear, {{0.f, "Touch takes it"}, {1.f, "Wear by hand"}})
            .help("Wear by hand: grip the armour to carry it and let go of it over your chest to put it on (only if it is better "
                  "than yours). Walking over it no longer takes it. Next map."),
        cycle("Weapons and Keys", vr_item_objects, {{0.f, "Touch takes them"}, {1.f, "Objects"}})
            .help("Objects: the map's weapons, keys, runes and suits hang spinning until you grab, knock or force-grab them, "
                  "then they are physics objects. A weapon you grip is yours at once; keys, runes and suits you carry to a "
                  "holster and let go of there. Powerups are as before. Next map."),
        cycle("Weapon Pickups Look", vr_pickup_prop_models, {{0.f, "Classic models"}, {1.f, "As held and dropped"}})
            .help("As held and dropped: the map's weapons are drawn with the same models as the weapons in your hands and "
                  "the ones you drop, at their size, spinning about their middle. Classic models: id's pickup models (g_*.mdl). A weapon "
                  "without a model of its own keeps the classic one. Next map."),
    };
}

[[nodiscard]] za::Vector<Item> pageBurning();

// Split from Carrying and Gibs: the wall torches you can take.
[[nodiscard]] za::Vector<Item> pageWallTorches()
{
    return {
        toggle("Take Torches Off Walls", vr_walltorch)
            .help("Grip a wall torch to take it immediately (or force grab it): it is a burning club, lighting the "
                  "room round you as the wall torch did. Held, it burns for ever; dropped, thrown or used up by blows, "
                  "its fire dies. Grip and fingers: Held Object Offsets, holding it. Off: fixed, as in id's Quake. Next map."),
        slider("Grab Window", vr_walltorch_grab_time, 0.f, 1.5f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("A grip closed on the way to a torch on its wall, still held, takes it as your hand gets there, up to "
                  "this long after you closed it (not a punch: a fast fist goes through). 0: only a grip closed there."),
        toggle("Force Grab Torches", vr_walltorch_forcegrab)
            .help("Torches come off their walls, and from where they lie, to a force grab."),
        slider("Dying Time", vr_walltorch_die_time, 1.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long a dying torch's fire takes to go out: dropped, thrown or out of blows. Picked up again before "
                  "it is out, a dropped torch burns up again; one out of blows goes on dying."),
        open("Burning", pageIndex(pageBurning))
            .help("A lit torch's blow sets what it hits on fire: Combat > Burning (its damage, time and flames)."),
        toggle("Light Again", vr_walltorch_relight)
            .help("A dying or burnt-out torch held in another torch's flame (on a wall or in your other hand), in one of "
                  "the map's flames (braziers, flame balls) or dipped in lava lights again, as new."),
        toggle("Shot Off Its Wall", vr_walltorch_shot)
            .help("A wall torch hit by anything you shoot (a shotgun's pellets, a nail, a rocket, a grenade, a laser) or "
                  "by a thing you throw into it hard comes off its wall: it falls lit, and its light on the wall is gone. "
                  "Your hand and a punch take it as before. Off: shots go through it, as in id's Quake. Next map."),
        toggle("Taken Torch Casts Shadows", vr_walltorch_shadows)
            .help("A taken torch's light casts shadows (your hands and body, what is round you), whatever Graphics' Torch "
                  "Light Shadows says. Its brightness is the wall torch's: Graphics' Torch Light Brightness."),
    };
}

// Combat > Burning (QC vr_burning.qc): what a lit torch, a thrown one or a lava nail sets on fire; crates; nails through
// a torch's flame.
// Combat > Arcane Dimensions (AD support, ARCANE_DIMENSIONS.md decisions 2 and 22): AD's projectile pellets and whose
// projectile rules play (QC vr_ad_bridge.qc AD_Pellets).
[[nodiscard]] za::Vector<Item> pageArcaneDimensions()
{
    return {
        cycle("Projectile Rules", vr_ad_projectiles, {{1.f, "Off"}, {0.f, "AD Maps Only"}, {2.f, "Everywhere"}})
            .help("Whose rules shotguns and monster projectiles play by. AD Maps Only: Arcane Dimensions' in its maps "
                  "(projectile pellets you see fly, from your shotguns and the grunts'; the hell knights', scrags', ogres' "
                  "and zombies' missiles at its speeds for the skill), Quake's elsewhere. Off: Quake's everywhere. "
                  "Everywhere: its rules wherever its data is loaded. Its own monsters always play by its rules."),
        slider("Pellet Spread", vr_ad_pellet_spread, 0.f, 2.f, 0.05f, "x%.2f").extend(0.f, 4.f)
            .help("Arcane Dimensions' pellets: how wide they spread, times its own (the shotgun 0.04, the super shotgun "
                  "0.14 by 0.08: wider than Quake VR's hitscan, aimed with the hand). 0: every pellet straight."),
        slider("Player Pellet Speed", vr_ad_pellet_speed, 0.25f, 3.f, 0.05f, "x%.2f").extend(0.1f, 10.f)
            .help("How fast your pellets fly, times Arcane Dimensions' (the shotgun 2000, the super shotgun 1500 units a "
                  "second): faster is nearer hitscan."),
        slider("Enemy Pellet Speed", vr_ad_enemy_pellet_speed, 0.25f, 3.f, 0.05f, "x%.2f").extend(0.1f, 10.f)
            .help("Enemy shotgun pellets' speed multiplier, independent of yours. Applies to Quake grunts using AD projectile rules and AD enemies in any map."),
        slider("Pellet Damage", vr_ad_pellet_damage, 0.f, 3.f, 0.05f, "x%.2f").extend(0.f, 10.f)
            .help("Your pellets' damage, times Weapon Damage's shotgun and super shotgun pellet (4: Arcane Dimensions')."),
        header("Feature Scope"),
        cycle("Ammo Models", vr_ad_ammo_models, {{0.f, "Off"}, {1.f, "AD Maps Only"}, {2.f, "Everywhere"}})
            .help("AD's custom ammo boxes while its data is loaded. Off: original Quake boxes. Newly spawned ammo only; reload the map to change existing boxes. Ammo counts, held weapons and VR pickup physics stay the same."),
        cycle("Ammo Resistance", vr_ad_resistance, {{0.f, "Off"}, {1.f, "AD Maps Only"}, {2.f, "Everywhere"}})
            .help("AD ammo resistance and vulnerability on entities with resistance keys. Everywhere preserves AD monsters' resistance outside AD maps; ordinary enemies without keys stay unchanged. Takes effect immediately."),
        cycle("Weapon Upgrades", vr_ad_upgrades, {{0.f, "Off"}, {1.f, "With AD Content"}, {2.f, "Everywhere"}})
            .help("Shadow Axe, Widowmaker and Plasma Gun upgrade effects and new pickups. Owned upgrades are preserved while Off. "
                  "With AD Content: in AD maps, and in any map while Additional Content is Everywhere (their pickups placed "
                  "there or spawned with the toolgun). Everywhere: in any map regardless. No automatic replacement of ordinary guns."),
        toggle("Evil Skill (with Nightmare)", vr_ad_evil)
            .help("Arcane Dimensions' fifth skill, in any campaign while its data is installed: chosen with Nightmare (the "
                  "start map's skill hall, or Arcane Dimensions' own Evil choice, which sets this). 50 health at most "
                  "(megahealth 100), the Shadow Axe's blows, Arcane Dimensions' monsters at their hardest. Kept in saves."),
        slider("Screen Tints", vr_ad_tints, 0.f, 1.f, 0.05f, "x%.2f")
            .help("Arcane Dimensions' screen colours, times this: green while poisoned, dark amber while bleeding, "
                  "orange while burning (with Burn Damage off), a book's dark background while you read it. Eased in and out. 0: none. Its "
                  "cinematic cameras' fades to black always play (Graphics' colour shift settings apply to all)."),
        toggle("Hand-Climbed Ladders", vr_climb_ladders)
            .help("Arcane Dimensions' ladders climbed with your hands, as ledges (Hand Climbing on): grip an actual "
                  "rung, pull yourself up hand over hand, mantle at its top; a ladder with no rung within reach (a flat "
                  "ladder wall) is held anywhere on it. While hands climb ladders, keyboard ladder movement and jump "
                  "climbing are off; with this or Hand Climbing off, ladders climb Arcane Dimensions' way (hold jump)."),
        cycle("Additional Content", vr_ad_everywhere, {{0.f, "AD Maps Only"}, {1.f, "Everywhere"}})
            .help("Arcane Dimensions' monsters, new artifacts, custom keys, health vials and gems, custom items, ladders and "
                  "weapons (the Shadow Axe, Widowmaker and Plasma Gun with Weapon Upgrades on; its soldiers' guns) in any "
                  "map that places them (custom maps, the toolgun), while its data is installed. AD Maps Only: only in its own "
                  "maps. Its pickups that would replace Quake VR's (backpacks) are only ever in its maps. Disabling required monsters or keys in AD maps would prevent progress, so they remain available there."),
        command("Map Logic / Inventory Override", "")
            .unavailableBecause("AD map logic, key progression and scripted inventory remain AD-map-only. A global override is not supported: it would break triggers, exits and item placement. VR interactions, hit fixes and physics always remain active."),
        open("Soldiers' Guns", pageIndex(pageAdGuns))
            .help("Arcane Dimensions' armed soldiers' own guns, dropped as they die: their damage, ammo and speed."),
        command("AD Status", "vr_ad_info"),
        command("Scope and Ammo Model Test", "vr_ad_test 65")
            .help("Reports the feature scopes, all eight ammo box models and pickup physics; checks both held weapon models stay unchanged."),
    };
}

[[nodiscard]] za::Vector<Item> pageBurning()
{
    return {
        slider("Burn Damage", vr_burn_damage, 0.f, 20.f, 0.5f, "%.1f / s").extend(0.f, 100.f)
            .help("A fire's damage a second, however many flames it has and however often it is lit again: it never "
                  "stacks. 0: nothing alive is set on fire."),
        slider("Burn Time", vr_burn_time, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("How long a monster burns from the last hit that lit it (a hit while it burns starts it again)."),
        slider("Flames Spread To", vr_burn_flames, 1.f, 10.f, 1.f, "%.0f").extend(1.f, 20.f)
            .help("A fire starts as one flame where it was struck and spreads over the body, a smaller flame at a time, "
                  "up to this many."),
        slider("Most Flames", vr_burn_flames_max, 1.f, 12.f, 1.f, "%.0f").extend(1.f, 30.f)
            .help("More hits add a flame each (a few a second at most), up to this many on one body. The damage stays "
                  "one fire's."),
        slider("Spread Time", vr_burn_spread, 0.1f, 3.f, 0.1f, "%.1f s").extend(0.05f, 10.f)
            .help("Time between flames spreading."),
        slider("Flame Size", vr_burn_flame_size, 0.2f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 4.f)
            .help("The first flame's size (id's small flame: 1)."),
        slider("Spread Flames' Size", vr_burn_flame_small, 0.2f, 1.f, 0.05f, "%.2fx").extend(0.05f, 2.f)
            .help("The other flames' size, times the first's (each a little more or less)."),
        slider("Spread Flames' Lean", vr_burn_flame_tilt, 0.f, 30.f, 1.f, "%.0f deg").extend(0.f, 60.f)
            .help("How far the other flames lean, at most; they stay upright. 0: all straight up."),
        header("Corpses"),
        toggle("Corpses Burn", vr_burn_corpses)
            .help("Corpses catch fire too, and a monster that dies burning burns on as a corpse. Corpses are the ones "
                  "Gibs and Corpses' Gib Corpses keeps."),
        slider("Corpse Burn Time", vr_burn_corpse_time, 0.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f),
        slider("Corpse Burn Damage", vr_burn_corpse_damage, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("A burning corpse's damage, times Burn Damage: enough of it gibs it (Corpse Health). 0: it just burns."),
        toggle("Bodies Burn in Lava", vr_burn_lava_bodies)
            .help("Ragdolls, knocked-down monsters and corpses in lava catch fire there, and burn on in it."),
        slider("Burnt Through in Lava", vr_burn_lava_gib, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("A body this long in lava bursts into gibs (a knocked-down one is killed). 0: never."),
        slider("You Burn After Lava", vr_burn_lava_player, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 30.f)
            .help("Lava sets you on fire: out of it you burn on this long (Burn Damage a second; in it, only the lava's "
                  "own damage). Water puts it out. Not with the Pentagram or the Biosuit. 0: off."),
        slider("Smoke After Flames", vr_smoulder_burn_time, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("A burning monster or corpse smokes while it burns, and this long after its flames go out, thinning out "
                  "(how much: Gore > Lightning Shock > Smouldering Smoke)."),
        header("What Sets Things on Fire"),
        toggle("Torch Touch", vr_burn_touch)
            .help("A lit torch, held or thrown, sets a monster or a corpse on fire just by touching it: no blow needed. "
                  "Mapper flames touching your body burn you in the same way as a dropped lit torch. "
                  "Off: a lit torch's blow, or one thrown into it."),
        toggle("Lava Nails", vr_burn_lava_nails)
            .help("Your lava nails (the nailgun's and super nailgun's lava ammo) set what they hit on fire."),
        toggle("Nails Through a Flame", vr_burn_nail_convert)
            .help("A nail fired through a fire becomes a lava nail: more damage, and it sets what it hits on fire. A lit "
                  "torch's flame (in your other hand, thrown, lying or on its wall), the map's flames, braziers and "
                  "candles, the toolgun's flames, a lava ball."),
        toggle("Nails Through Burning Things", vr_burn_nail_burning)
            .help("A burning monster's, corpse's or crate's flames turn nails into lava nails too (Nails Through a "
                  "Flame on)."),
        slider("Flame's Reach for Nails", vr_burn_nail_reach, 2.f, 30.f, 1.f, "%.0f").extend(0.f, 80.f)
            .help("How near a torch's flame a nail must pass, in units (a hand is about 4)."),
        slider("Nail Sizzle Volume", vr_burn_nail_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the short fizz a nail makes as it catches fire in a torch's flame, and of fire hissing out "
                  "in water (0 off)."),
        toggle("Map Flames Burn Monsters", vr_burn_map_monsters)
            .help("Monsters awake, ragdolls and corpses touching the map's flames (flames, braziers, wall torches, the "
                  "toolgun's) catch fire, as you do."),
        toggle("Water Puts Fire Out", vr_burn_water_douse)
            .help("A flamethrower's flames going into water or slime go out in a puff of steam; a lava nail going in "
                  "becomes a plain nail."),
        slider("Crackle Volume", vr_burn_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("A burning monster, corpse or crate crackles as a wall torch does, heard close by, quieter as its flames "
                  "die (0 off)."),
        slider("Most Crackling", vr_burn_sound_max, 1.f, 8.f, 1.f, "%.0f").extend(1.f, 32.f)
            .help("At most this many burning things crackle at once; the rest burn silent until one goes out."),
        header("Crates"),
        toggle("Crates Burn", vr_burn_crates)
            .help("Wooden crates catch fire: a lit torch's blow or touch, a lava nail, a burning crate touching them."),
        toggle("Wood Burns in Lava", vr_burn_lava)
            .help("Crates, barrels and their pieces dropped in lava catch fire there, and burn on in it."),
        slider("Crate Burn Time", vr_burn_crate_time, 1.f, 60.f, 1.f, "%.0f s").extend(0.5f, 300.f)
            .help("How long a crate burns before it breaks (or its fire goes out)."),
        toggle("Burnt Crates Break", vr_burn_crate_break)
            .help("A crate that has burnt its time breaks into its pieces. Off: its fire goes out."),
        slider("Fire Spreads After", vr_burn_crate_spread, 0.f, 20.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long a crate burns before the crates touching it catch fire. 0: fire never spreads."),
        slider("Touching Within", vr_burn_crate_gap, 0.f, 16.f, 1.f, "%.0f").extend(0.f, 64.f)
            .help("How far apart two crates still count as touching, in units."),
        slider("Crate Flames", vr_burn_crate_flames, 1.f, 12.f, 1.f, "%.0f").extend(1.f, 30.f)
            .help("Flames a crate's fire spreads to (Most Flames caps it)."),
        slider("Crate Flame Size", vr_burn_crate_flame_size, 0.5f, 2.5f, 0.05f, "%.2fx").extend(0.1f, 5.f)
            .help("A crate's flames, times a body's (Flame Size)."),
        toggle("Burnt Crates' Pieces Charred", vr_burn_crate_char)
            .help("A crate that burns through breaks into charred, blackened pieces that don't burn again. Off: plain pieces, "
                  "which burn as any other."),
        toggle("Pieces Burn", vr_burn_pieces)
            .help("The pieces of a crate you break catch fire as crates do: a lit torch's blow or touch, a lava nail, burning "
                  "crates and pieces near them. A crate broken while burning scatters burning pieces."),
        slider("Piece Burn Time", vr_burn_piece_time, 1.f, 30.f, 0.5f, "%.1f s").extend(0.5f, 120.f)
            .help("How long a piece burns before it burns away (charred from half of it)."),
        slider("Pieces Catch Within", vr_burn_piece_gap, 0.f, 32.f, 1.f, "%.0f").extend(0.f, 96.f)
            .help("How near a burning piece and another piece or a crate catch from each other, in units (Fire Spreads "
                  "After: when)."),
        slider("Piece Flames", vr_burn_piece_flames, 1.f, 8.f, 1.f, "%.0f").extend(1.f, 20.f)
            .help("Flames a piece's fire spreads to (Most Flames caps it)."),
        slider("Piece Flame Size", vr_burn_piece_flame_size, 0.3f, 2.f, 0.05f, "%.2fx").extend(0.1f, 5.f)
            .help("A piece's flames, times a body's (Flame Size)."),
        header("Torch Flame"),
        slider("Swing Lean", vr_walltorch_lean, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 5.f)
            .help("How far a swung torch's flame leans and trails behind its motion (1: the default; 0: always straight up)."),
        slider("Flatten When Fast", vr_walltorch_flatten, 0.f, 1.5f, 0.1f, "%.1fx").extend(0.f, 1.5f)
            .help("How much a fast swing flattens the flame and stretches it back (1: the default; 0: never)."),
        slider("Hand Motion Strength", vr_walltorch_hand_motion, 0.f, 4.f, 0.1f, "%.1fx")
            .help("Hand translation and rotation move the flame's attachment point and make it lean/stretch. 0: locomotion only."),
        slider("Motion Smoothing", vr_walltorch_motion_smooth, 0.01f, 0.5f, 0.01f, "%.2f s"),
        slider("Upside Down: Flame Height", vr_walltorch_inv_size, 0.05f, 0.3f, 0.01f, "%.2fx")
            .help("One flame stays attached to the head. Past 90 degrees it smoothly shortens to this height at 180 degrees; fire particles continue to rise."),
        open("Fire Particles", pageIndex(pageFireParticles)),
        slider("Upside Down: Brightness", vr_walltorch_inv_light, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("Held head down, its light's brightness, times the upright one's."),
        slider("Upside Down: Burning Drips", vr_walltorch_drips, 0.f, 20.f, 1.f, "%.0f / s").extend(0.f, 60.f)
            .help("Burning drips falling off the head of a torch held head down, a second (0: none)."),
        slider("Upside Down: More Smoke", vr_walltorch_inv_smoke, 1.f, 5.f, 0.25f, "%.2fx").extend(0.f, 10.f)
            .help("Held head down, a torch smokes this much more."),
        slider("Torch Smoke", vr_walltorch_smoke, 0.f, 3.f, 0.25f, "%.2fx").extend(0.f, 10.f)
            .help("Soot rising off every lit torch, on its wall, in your hand or lying (1: three puffs a second; 0: none)."),
        slider("Torch Smoke Opacity", vr_walltorch_smoke_alpha, 0.f, 1.f, 0.05f, "%.2f")
            .help("How dark the torches' smoke is."),
        header("Your Own Torch"),
        toggle("Its Flame Burns You", vr_burn_self)
            .help("The flame of the torch you hold, kept on your other hand, an arm, your body or your head, sets you on fire "
                  "there after a moment (Catch Fire After), as it does a monster: flames on you, burns on your skin, Burn "
                  "Damage. Till then the hand buzzes, harder as it comes. Mapper flames touching your hands or arms use "
                  "the same warning and delay."),
        slider("Catch Fire After", vr_burn_self_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 10.f)
            .help("How long the flame must stay on you before you catch fire (off it, it eases back twice as fast)."),
        slider("Warning Buzz", vr_burn_self_haptic, 0.f, 2.f, 0.1f, "%.1fx")
            .help("How hard the hand buzzes while a flame is on you, before you catch fire or drop the torch (0: none)."),
        toggle("Upside Down Burns Your Hand", vr_burn_drop)
            .help("Held head down, a torch's flame climbs the stick to your hand: kept there a moment (Drop After), it burns "
                  "your hand and you drop the torch."),
        slider("Drop After", vr_burn_drop_time, 0.f, 5.f, 0.1f, "%.1f s").extend(0.f, 20.f)
            .help("How long a torch held head down can burn your hand before you drop it."),
    };
}

// Split from Carrying and Gibs: rocks and bricks lying about.
[[nodiscard]] za::Vector<Item> pageRocksBricks()
{
    return {
        toggle("Rocks and Bricks", vr_debris)
            .help("Rocks lie on natural ground and at the foot of rock and stone walls, bricks at the foot of brick walls, "
                  "by what the textures are: pick them up, force grab them, punch with them, throw them. The same "
                  "places at every load. Single player. Next map."),
        toggle("Rocks", vr_debris_rocks).help("Rocks on grass, dirt and rock, and by rock and stone walls. Next map."),
        toggle("Bricks", vr_debris_bricks).help("Bricks at the foot of brick walls. Next map."),
        debrisMultiplayerSlider(),
    };
}

// Wooden crates (vr_crates*; vr_crates.cpp, QC vr_crates.qc): where they lie, breaking them, what they hold, hiding.
[[nodiscard]] za::Vector<Item> pageCrates()
{
    return {
        header("Where They Lie"),
        toggle("Crates", vr_crates)
            .help("Wooden crates, small and large, stand by the walls and in corners, some stacked two high: push them, "
                  "carry them, stand on them, break them. The same places at every load. Single player. Next map."),
        header("Breaking"),
        slider("Health", vr_crate_health, 0.f, 100.f, 5.f, "%.0f").extend(0.f, 1000.f)
            .help("A small crate's (a large one's 1.6 times): shots, blows, explosions and hard hits wear it down. A shotgun "
                  "blast does up to 24, the axe about 20. 0: they never break."),
        slider("Break Speed", vr_prop_break_speed, 2.f, 20.f, 0.25f, "%.2f m/s").extend(0.1f, 60.f)
            .help("A hand's or weapon's blow on a small crate, or one held and smashed into a wall, the floor or a monster, "
                  "this fast breaks it at once (no real blow is: two strong ones do); from Blows Wear From of it it is "
                  "damaged (more the faster), slower it is only pushed. Every prop that breaks follows it: explosive boxes "
                  "(they blow up), barrels, AD's and the expansions' breakables; each by its health (the square root of it "
                  "over a small crate's) and its material (below). A weapon's heft helps (the axe 1.4x)."),
        slider("Blows Wear From", vr_prop_blow_wear, 0.1f, 0.9f, 0.05f, "%.2fx").extend(0.05f, 0.95f)
            .help("How much of Break Speed a blow (or a prop held and smashed) needs to damage a prop, a little at first, "
                  "more the faster. 0.25: a strong punch (12 m/s) takes two to break a crate, a medium one (9) three or "
                  "four, a light one (5) barely marks it. Thrown and dropped props are damaged from half their own break "
                  "speeds."),
        slider("Break Speed: Wood", vr_prop_break_wood, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.01f, 10.f)
            .help("Times Break Speed for crates, barrels and wooden breakables."),
        slider("Break Speed: Glass", vr_prop_break_glass, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.01f, 10.f)
            .help("Times Break Speed for glass and pottery (AD's windows and vases)."),
        slider("Break Speed: Stone", vr_prop_break_stone, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.01f, 10.f)
            .help("Times Break Speed for rock and brick breakables."),
        slider("Break Speed: Metal", vr_prop_break_metal, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.01f, 10.f)
            .help("Times Break Speed for metal breakables."),
        slider("Break Speed: Explosive", vr_prop_break_explosive, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.01f, 10.f)
            .help("Times Break Speed for explosive boxes (20 health: 16.7 m/s by default). Broken, they blow up, in your "
                  "hands too."),
        slider("Break Speed: Pieces", vr_prop_break_piece, 1.f, 12.f, 0.25f, "%.2f m/s").extend(0.1f, 40.f)
            .help("A hand's hit on a crate's piece, or one held and smashed into something, this fast bursts it into dust; "
                  "from half as fast it is worn (two such hits burst it)."),
        toggle("Overhead Slam", vr_carry_slam)
            .help("A crate or barrel held in both hands over your head and swung down onto a monster, a prop or a wall breaks "
                  "at once (what it hits takes a heavy blow); thrown down from there, it takes heavy damage (two such "
                  "throws break it)."),
        header("What They Hold"),
        slider("Flare Gun on Crates", vr_crate_flaregun, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a crate placed about the map (the top one of a stack, no crowbar on it) has a loaded "
                  "flare gun lying on it; the same crates each load. Experimental: off (0) by default. Next map."),
        slider("Most Flare Guns", vr_crate_flaregun_max, 0.f, 4.f, 1.f, "%.0f").extend(0.f, 64.f)
            .help("Most flare guns lying on crates in a map. Next map."),
        header("Hiding"),
        toggle("Crates Hide You", vr_crate_sight)
            .help("Monsters can't see you through crates and explosive boxes lying about (crouch behind a small one). "
                  "Carrying one, they see you through it."),
        cycle("Held Crate Shields You", vr_crate_shield,
            {{0.f, "Off"}, {1.f, "Crates"}, {2.f, "Crates and Explosive Boxes"}})
            .help("A crate you hold up takes monsters' bullets meant for you (enough of them break it). Crates and "
                  "Explosive Boxes: an explosive box too, which then blows up in your hands (vr_crate_shield)."),
    };
}

[[nodiscard]] za::Vector<Item> pageRagdolls(); // (below)
[[nodiscard]] za::Vector<Item> pageRagdollGrunt();
[[nodiscard]] za::Vector<Item> pageRagdollKnight();
[[nodiscard]] za::Vector<Item> pageCorpseDamage();
[[nodiscard]] za::Vector<Item> pageRagdollOgre();
[[nodiscard]] za::Vector<Item> pageRagdollEnforcer();
[[nodiscard]] za::Vector<Item> pageRagdollDeathKnight();
[[nodiscard]] za::Vector<Item> pageRagdollRottweiler();
[[nodiscard]] za::Vector<Item> pageRagdollScrag();
[[nodiscard]] za::Vector<Item> pageRagdollZombie();
[[nodiscard]] za::Vector<Item> pageRagdollMummy();
[[nodiscard]] za::Vector<Item> pageRagdollGremlin();
[[nodiscard]] za::Vector<Item> pageRagdollVore();
[[nodiscard]] za::Vector<Item> pageRagdollCentroid();
[[nodiscard]] za::Vector<Item> pageRagdollShambler();
[[nodiscard]] za::Vector<Item> pageRagdollFiend();

// Split from Carrying and Gibs: taking, throwing and bursting gibs, heads and corpses.
[[nodiscard]] za::Vector<Item> pageGibs()
{
    return {
        cycle("Gibs and Heads", vr_grab_gibs, {{0.f, "Left alone"}, {1.f, "Grab by hand"}, {2.f, "Hand and force grab"}})
            .help("Pick up and throw gibs and heads, by reaching for them (or force-grabbing them too)."),
        toggle("Destroy Gibs", vr_gib_destroy)
            .help("Gibs and heads burst in a mist of blood when shot, blown up, struck, or thrown hard at a wall or a monster."),
        toggle("Gib Corpses", vr_corpse_gib)
            .help("Corpses burst into gibs when shot, blown up or struck enough: shotguns, nails, lightning, rockets, fists, melee weapons."),
        cycle("Never Gib Corpses", vr_corpse_nogib, {{0.f, "Off"}, {1.f, "Corpses and Ragdolls"}, {2.f, "And Dying Monsters"}})
            .help("For testing: corpses and ragdolls take hits (blood, pushes, blasts) but never burst into gibs, even blown "
                  "up. And Dying Monsters: a monster killed by a rocket dies whole too, so it becomes a ragdoll "
                  "(vr_corpse_nogib)."),
        open("Corpse Damage and Health", pageIndex(pageCorpseDamage)),
        header("Corpse Collision"),
        cycle("Corpses", vr_corpse_collide,
            {{0.f, "Pass Through"}, {1.f, "Fixed Box"}, {2.f, "Pushable Box"}, {3.f, "Fixed Pose"}, {4.f, "Pushable Pose"}})
            .help("Corpses in the physics: props, thrown things and held things meet them. Fixed: they never move. Pushable: "
                  "heavy, they slide when pushed (hands, held weapons, props, walking into them). Box: a low box round the "
                  "body; Pose: its shape as it lies (vr_corpse_collide)."),
        toggle("Props Meet Corpses", vr_corpse_collide_props)
            .help("Boxes, weapons, gibs and heads lying or falling rest on corpses and stop at them (vr_corpse_collide_props)."),
        toggle("Thrown Things Meet Corpses", vr_corpse_collide_thrown)
            .help("Off: what you throw flies through corpses (its hit still hurts them) (vr_corpse_collide_thrown)."),
        toggle("Held Things Meet Corpses", vr_corpse_collide_held)
            .help("Your hands, held weapons and props push pushable corpses; a prop held in both hands stops at any "
                  "(vr_corpse_collide_held)."),
        cycle("You and Corpses", vr_corpse_collide_player, {{0.f, "Walk Through"}, {1.f, "Step Over"}, {2.f, "Solid"}})
            .help("Step Over: a corpse is a low step you walk onto. Solid: walk round it or jump onto it "
                  "(vr_corpse_collide_player)."),
        toggle("Feet Nudge Ragdolls", vr_box3d_walk_nudge)
            .help("Walking through or over a ragdoll gently nudges the limbs your body overlaps along your pace, without blocking you (vr_box3d_walk_nudge)."),
        toggle("Monsters Step Over Corpses", vr_corpse_collide_monsters)
            .help("Monsters walk over corpses as a low step instead of through them (vr_corpse_collide_monsters)."),
        header("Ragdolls (Experimental)"),
        cycle("Ragdolls", vr_ragdoll, {{0.f, "Off"}, {1.f, "On (Experimental)"}})
            .help("A dying grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler or gremlin goes limp: his body becomes jointed parts that fall, tumble, are pushed, grabbed "
                  "and thrown (vr_ragdoll). Their settings: Ragdoll Settings."),
        open("Ragdoll Settings", pageIndex(pageRagdolls)),
    };
}

// Gibs and Corpses > Corpse Damage and Health (ROUND21.md, "Ragdolls on; corpse health and damage by kind"): how much
// damage it takes to gib a corpse or a ragdoll: each kind of hit's share of its damage (VR_Corpse_DamageKind, QC), each
// monster's corpse health (times Corpse Health, vr_corpse_health_mult).
[[nodiscard]] za::Vector<Item> pageCorpseDamage()
{
    return {
        open("Corpse Health, Never Gib: Gibs and Corpses", pageIndex(pageGibs)).help("Corpse Health and Never Gib Corpses are on Gibs and Corpses."),
    };
}

// Gibs and Corpses > Ragdoll Settings (ROUND21.md, "Ragdolls"): on or off, when they go limp and how many; their physics
// for all (each monster class may have its own: its page); taking them by hand and by force grab; their blood.
[[nodiscard]] za::Vector<Item> pageRagdolls()
{
    return {
        open("Ragdolls On/Off: Gibs and Corpses", pageIndex(pageGibs)).help("Ragdolls on or off is on Gibs and Corpses."),
        slider("Go Limp At", vr_ragdoll_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("When in his death animation: 0 as soon as he stops being solid, 1 once he lies still (vr_ragdoll_start)."),
        slider("Most Ragdolls", vr_ragdoll_max, 0.f, 16.f, 1.f, "%.0f").extend(0.f, 64.f)
            .help("At most this many at once. New deaths replace the oldest dead ragdoll first, then the oldest living knockdown if necessary. 0: ragdolls off."),
        toggle("Ragdolls Meet Each Other", vr_ragdoll_collide_each)
            .help("Ragdolls fall onto and pile on each other; off, they pass through each other (vr_ragdoll_collide_each)."),
        toggle("Limbs Meet Each Other", vr_ragdoll_self_collide)
            .help("A ragdoll's own limbs meet: an arm stays out of his chest, a leg out of the other, a body folded over "
                  "lies on itself. Jointed neighbours (a forearm and its upper arm), parts deep in each other as he went "
                  "limp (until they come apart) and an arm reaching for his wound, a stump or a wall pass through; his gun "
                  "never meets him. Off: his limbs pass through each other, as before. Ragdolls made from now on "
                  "(vr_ragdoll_self_collide)."),
        header("Active Deaths (Experimental)"),
        open("Test Them", pageIndex(pageRagdollTests))
            .help("Debug > Tests > Ragdoll Behaviours: each behaviour below set going on the nearest grunt, enforcer, "
                  "knight, death knight, ogre or zombie."),
        toggle("Active Deaths", vr_ragdoll_active)
            .help("A dying grunt, enforcer, knight, death knight, ogre or zombie (AD's soldiers, knights and zombies too) goes limp as soon as he stops being solid, "
                  "his body playing his death animation as his strength fades (legs first): he stumbles if he dies on the "
                  "move, may hold his wound and may crawl before going limp; a headshot or beheading collapses him. Off: "
                  "limp at Go Limp At (vr_ragdoll_active)."),
        slider("Muscle Strength", vr_ragdoll_active_strength, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 3.f)
            .help("How strongly the joints follow the death animation (vr_ragdoll_active_strength)."),
        slider("Strength Lasts", vr_ragdoll_active_fade, 0.25f, 3.f, 0.25f, "%.2fx").extend(0.1f, 5.f)
            .help("How long the strength lasts, times his death animation's length (vr_ragdoll_active_fade)."),
        slider("Muscle Stiffness", vr_ragdoll_active_hertz, 2.f, 20.f, 1.f, "%.0f Hz")
            .help("How quickly a joint at full strength springs towards its pose: more follows the animation tighter, "
                  "less is looser and lags (vr_ragdoll_active_hertz; at most 20)."),
        slider("Muscle Damping", vr_ragdoll_active_damping, 0.2f, 2.f, 0.1f, "%.1f")
            .help("How the joints' springs settle: 1 without overshoot, less bouncy, more sluggish (vr_ragdoll_active_damping)."),
        slider("Muscle Torque", vr_ragdoll_active_torque, 0.5f, 5.f, 0.25f, "%.2f").extend(0.f, 10.f)
            .help("A joint's most force at full strength, times the weight of what hangs on it times its lever: a blow "
                  "or a hand overpowers less of it (vr_ragdoll_active_torque)."),
        slider("Thrown by His Death", vr_ragdoll_active_pin, 0.f, 2.f, 0.1f, "%.1f").extend(0.f, 5.f)
            .help("Body weights pulling his pelvis along his death animation's at first (its throw back or forward "
                  "kept); 0 none (vr_ragdoll_active_pin)."),
        slider("Thrown For", vr_ragdoll_active_pin_time, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("And that pull fading out over this (vr_ragdoll_active_pin_time)."),
        slider("Stumble From", vr_ragdoll_active_stumble, 0.f, 300.f, 10.f, "%.0f u/s").extend(0.f, 1000.f)
            .help("Dying on the move at least this fast, he stumbles on: carried a moment, a leg stepping ahead. 0: "
                  "never (vr_ragdoll_active_stumble; a run is about 200-300, a walk 50-100)."),
        toggle("React to Lost Limbs", vr_ragdoll_active_dismember)
            .help("A dying monster losing a limb holds the stump with his free hand and writhes a few seconds once down; "
                  "killed standing by a leg cut off, he tries to keep his balance on the other before he falls; with no "
                  "legs he crawls on his arms. Off: he dies as from any blow (vr_ragdoll_active_dismember)."),
        slider("Writhing", vr_ragdoll_active_dismember_time, 0.f, 8.f, 0.5f, "%.1f s").extend(0.f, 20.f)
            .help("How long he writhes holding the stump once down (vr_ragdoll_active_dismember_time)."),
        slider("Writhe Size", vr_ragdoll_active_writhe, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 3.f)
            .help("How far his torso and legs stir as he writhes, times the default; 0 still (vr_ragdoll_active_writhe)."),
        slider("Balance on One Leg", vr_ragdoll_active_balance_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 5.f)
            .help("How long at most he tries to keep his balance on one leg (0: he falls at once) "
                  "(vr_ragdoll_active_balance_time)."),
        slider("Falls Past", vr_ragdoll_active_balance_lean, 5.f, 45.f, 1.f, "%.0f deg").extend(1.f, 90.f)
            .help("And he falls once leaning this far from how he stood (vr_ragdoll_active_balance_lean)."),
        slider("Hold the Wound", vr_ragdoll_active_wound, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance he puts a free hand on the wound that killed him (vr_ragdoll_active_wound)."),
        toggle("Use What Is Round Him", vr_ragdoll_active_env)
            .help("A dying monster senses the walls, floors, ledges and crates round him and uses his hands on them (the "
                  "three below). Off: his death ignores them (vr_ragdoll_active_env)."),
        slider("Crawl Away", vr_ragdoll_active_crawl, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a humanoid lying after his death rolls onto his front and crawls a few seconds before going "
                  "limp (vr_ragdoll_active_crawl)."),
        slider("Crawl Time", vr_ragdoll_active_crawl_time, 0.5f, 8.f, 0.5f, "%.1f s").extend(0.1f, 30.f)
            .help("How long he crawls (vr_ragdoll_active_crawl_time)."),
        toggle("Flail on His Back", vr_ragdoll_active_crawl_back)
            .help("One to crawl who lies on his back: on, he doesn't roll over onto his front; he flails there like a "
                  "dying cockroach, his limbs pawing the air, then goes limp. Off: he rolls over and crawls "
                  "(vr_ragdoll_active_crawl_back)."),
        slider("Crawl Pull", vr_ragdoll_active_crawl_force, 0.2f, 3.f, 0.1f, "%.1f").extend(0.f, 6.f)
            .help("Body weights his arms pull along the floor at most: more drags him further (vr_ragdoll_active_crawl_force)."),
        toggle("Swim and Flail in Liquids", vr_ragdoll_active_liquids)
            .help("Dying in water (drowned, or falling in just after his death) he paddles and kicks, his head held up, "
                  "weakening as he drowns; in lava or slime he flails, short and violent; then limp. Off: his death in a "
                  "liquid plays as on land (vr_ragdoll_active_liquids)."),
        slider("Paddle Time", vr_ragdoll_active_paddle, 1.f, 10.f, 0.5f, "%.1f s").extend(0.2f, 30.f)
            .help("About how long he paddles in water before going limp (vr_ragdoll_active_paddle)."),
        slider("Flail Time", vr_ragdoll_active_flail, 0.5f, 4.f, 0.25f, "%.2f s").extend(0.2f, 30.f)
            .help("About how long he flails in lava (in slime 1.4 times it) before going limp (vr_ragdoll_active_flail)."),
        header("Hit Reactions (Living Monsters)"),
        cycle("Hit Reactions", vr_hit_react, {{0.f, "Off"}, {1.f, "Limbs"}, {2.f, "Whole Body"}})
            .help("A living monster you hit flinches where you hit it, then carries on fighting: its AI, its damage and "
                  "its hitboxes are as before. Limbs: the bones struck turn round their joints (the head snaps back, an "
                  "arm swings, a leg's knee buckles and the torso leans over it; a monster with no rig tilts whole). "
                  "Whole Body: every monster tilts whole. Off: none, and no slow-down (vr_hit_react)."),
        slider("Reaction Strength", vr_hit_react_strength, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 3.f)
            .help("How far it flinches, times the default (by the share of its health the blow took; "
                  "vr_hit_react_strength)."),
        slider("Reaction Length", vr_hit_react_time, 0.5f, 2.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How long a flinch lasts, times the default (its peak at about 0.07 s, gone by about 0.5 s; "
                  "vr_hit_react_time)."),
        slider("Hits Slow Them", vr_hit_react_slow, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 3.f)
            .help("A monster hit a moment ago takes shorter steps: a leg hit about half for half a second, with a hitch "
                  "towards that leg; the body a third, 0.3 s. 0: none (vr_hit_react_slow)."),
        toggle("Active Shoves and Throws", vr_ragdoll_active_stagger)
            .help("With Knockdowns, a shove makes a grunt, enforcer, knight, death knight, ogre or zombie backpedal as a "
                  "ragdoll held up by its muscles, about as far as a shove used to slide it, its arms wheeling: beaten by "
                  "the knockdown's chance it gives way after 3 or 4 steps (knocked down), else it catches itself and "
                  "fights on. Going down, its arms brace for the floor; a two-hand throw's body goes slack where you held "
                  "it and curls as it goes over. Off: the knockdown's chance alone (vr_ragdoll_active_stagger)."),
        slider("Stagger Falls Past", vr_ragdoll_active_stagger_lean, 10.f, 60.f, 5.f, "%.0f deg")
            .help("A staggering monster whose middle leans this far past its feet for a third of a second, or twice "
                  "this far at once, falls whatever its balance (vr_ragdoll_active_stagger_lean)."),
        slider("Backpedal Steps", vr_ragdoll_active_stagger_steps, 1.f, 10.f, 1.f, "%.0f")
            .help("How many steps back a shoved monster takes at most (a steady one up to 3 more to catch itself); one "
                  "the shove beat gives way after 3 or 4 of them (vr_ragdoll_active_stagger_steps)."),
        slider("Backpedal Distance", vr_ragdoll_active_stagger_travel, 0.f, 2.f, 0.1f, "%.1fx")
            .help("How far a shoved monster backpedals, times how far the same shove slid a standing one before "
                  "active shoves (at most 4 m) (vr_ragdoll_active_stagger_travel)."),
        slider("Shove's Push", vr_ragdoll_active_stagger_push, 0.f, 5.f, 0.25f, "%.2f m/s").extend(0.f, 10.f)
            .help("A fresh two-hand shove's push on a staggering monster's top (its pelvis carried by its balance, its "
                  "feet not): more tips it further (vr_ragdoll_active_stagger_push)."),
        slider("Stagger Balance", vr_ragdoll_active_stagger_pin, 0.5f, 5.f, 0.25f, "%.2f").extend(0.f, 10.f)
            .help("Body weights holding its pelvis up while it staggers, at most (by its balance: the knockdown's chance "
                  "against its roll): less, it falls more often (vr_ragdoll_active_stagger_pin)."),
        slider("Stagger Recovery Blend", vr_ragdoll_active_stagger_blend, 0.05f, 1.f, 0.05f, "%.2f s")
            .help("Recovered, its ragdoll blends into the animated monster over this (vr_ragdoll_active_stagger_blend)."),
        slider("Stagger Settles Below", vr_ragdoll_active_stagger_settle, 0.f, 3.f, 0.1f, "%.1f m/s")
            .help("Its backpedal nearly done, not mid-step, steady and slower than this: it has recovered and fights on "
                  "(higher: handed back sooner) (vr_ragdoll_active_stagger_settle)."),
        slider("Stagger Settle at Most", vr_ragdoll_active_stagger_settle_time, 0.f, 1.2f, 0.05f, "%.2f s")
            .help("Still staggering this long after its backpedal: recovered however it moves "
                  "(vr_ragdoll_active_stagger_settle_time)."),
        slider("Stagger Drop Falls", vr_ragdoll_active_stagger_drop, 0.f, 64.f, 4.f, "%.0f units")
            .help("Backpedalling off a ledge or down from the top of stairs: the floor this much lower under it than "
                  "where it stood, it falls instead of stepping on air. 0: never (vr_ragdoll_active_stagger_drop)."),
        toggle("Active Get-Ups", vr_ragdoll_active_getup)
            .help("A knocked-down grunt, enforcer, knight, death knight, ogre or zombie gets up with its body: it rolls "
                  "over if it must, pulls itself into its get-up and rises through it, then fights on. Off: its ragdoll "
                  "blends into the get-up from the floor (vr_ragdoll_active_getup)."),
        slider("Drowning Struggle", vr_ragdoll_active_drown, 0.f, 1.f, 0.05f, "%.2f")
            .help("With Knockdowns, the chance a grunt, enforcer, knight, death knight, ogre or zombie standing with its "
                  "head under water, out of breath, struggles as a ragdoll: it paddles frantically for the surface, its "
                  "head up, towards where it can stand, and gets up there. Needs Swim and Flail in Liquids. 0: it drowns "
                  "standing (vr_ragdoll_active_drown)."),
        slider("Pull Up From Ledges", vr_ragdoll_active_pullup, 0.f, 1.f, 0.05f, "%.2f")
            .help("A knocked-down grunt, enforcer, knight, death knight, ogre or zombie falling past a ledge's lip near a "
                  "hand grabs it (Grab Ledges on) and hangs a moment; then, by this chance, it pulls itself up onto the "
                  "top and lies there until it gets up, else it drops. Shooting a hand makes it let go. 0: it never grabs "
                  "(vr_ragdoll_active_pullup)."),
        slider("Grab Others Falling", vr_ragdoll_active_grab, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a grunt, enforcer, knight, death knight, ogre or zombie going down (knocked down or dying) "
                  "grabs at another monster within reach, standing or lying, to catch itself: a hand on it a moment, "
                  "slowing its fall; a body grabbed is dragged along. 0: never (vr_ragdoll_active_grab)."),
        slider("Guard the Head", vr_ragdoll_active_guard, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a living monster guards its head when your hand swings at it in melee (a sword, an axe, a "
                  "fist, a bash; never a shot): its arms up before its face a moment. Standing it is drawn as a hit "
                  "reaction (Hit Reactions on Limbs); staggering, down or getting up, its body's arms. Looks alone: "
                  "the blow lands where it lands. 0: never (vr_ragdoll_active_guard)."),
        toggle("Placed Corpses Are Bodies", vr_corpse_placed)
            .help("The corpses a map places (Dimension and Dawn of the Machine) are dead monsters' bodies: shot, gibbed and "
                  "burnt, and ragdolls you take and cut once you come near. Off: scenery. The next map load (vr_corpse_placed)."),
        slider("Get-Up Lift", vr_ragdoll_active_getup_pin, 0.5f, 5.f, 0.25f, "%.2f").extend(0.f, 10.f)
            .help("Body weights pulling its pelvis through its get-up, at most: less, it struggles and may slump "
                  "(vr_ragdoll_active_getup_pin)."),
        slider("Get-Up Pace", vr_ragdoll_active_getup_pace, 0.3f, 2.f, 0.1f, "%.1fx")
            .help("Its get-up's speed, times the get-up's own (vr_ragdoll_active_getup_pace)."),
        slider("Get-Up Blend", vr_ragdoll_active_getup_blend, 0.05f, 1.f, 0.05f, "%.2f s")
            .help("Risen, its ragdoll blends into the animated monster over this (vr_ragdoll_active_getup_blend)."),
        toggle("Get Up in Place", vr_ragdoll_active_getup_hold)
            .help("On: it rolls over and gathers itself where it lies, then rises; only the get-up's own motion moves "
                  "it. Off: as before, its roll carried it along the floor and it was dragged back "
                  "(vr_ragdoll_active_getup_hold)."),
        header("Physics (All Monsters)"),
        slider("Mass", vr_ragdoll_mass, 20.f, 200.f, 5.f, "%.0f kg").extend()
            .help("A ragdoll's whole weight, its parts by their size, for a monster whose own Mass is Global: each has its "
                  "own by default (grunt 80, knight 90, enforcer 100, death knight 130, ogre 200, rottweiler and scrag 40, fiend 140, shambler 280, gremlin 20; "
                  "vr_ragdoll_mass)."),
        slider("Friction", vr_ragdoll_friction, 0.1f, 2.f, 0.1f, "%.1f").extend()
            .help("How much it drags and catches on floors and steps (vr_ragdoll_friction)."),
        slider("Joint Friction", vr_ragdoll_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend()
            .help("How hard the joints are to turn: 0 limp as a rag, more like a body freshly dead (vr_ragdoll_joint_friction)."),
        slider("Joint Stiffness", vr_ragdoll_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend()
            .help("A spring in each joint pulling it back towards how he stood: 0 none, 1 to 3 some tone left in the body "
                  "(vr_ragdoll_joint_stiffness)."),
        slider("Joint Limits", vr_ragdoll_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("How far the joints bend and twist, times the body's own limits: less stiff, more loose "
                  "(vr_ragdoll_limits)."),
        slider("Limb Damping", vr_ragdoll_damping, 0.f, 3.f, 0.1f, "%.1f").extend()
            .help("Less flailing of the arms and legs (vr_ragdoll_damping)."),
        slider("Blast Throw", vr_ragdoll_blast, 0.f, 5.f, 0.25f, "%.2fx").extend()
            .help("How far explosions throw a ragdoll, times what they give a prop of its weight (vr_ragdoll_blast)."),
        slider("Death Motion Kept", vr_ragdoll_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("How much of his death animation's motion the parts keep as he goes limp (vr_ragdoll_inherit)."),
        header("Each Monster's Own"),
        open("Grunt", pageIndex(pageRagdollGrunt)),
        open("Knight", pageIndex(pageRagdollKnight)),
        open("Ogre", pageIndex(pageRagdollOgre)),
        open("Enforcer", pageIndex(pageRagdollEnforcer)),
        open("Death Knight", pageIndex(pageRagdollDeathKnight)),
        open("Rottweiler", pageIndex(pageRagdollRottweiler)),
        open("Scrag", pageIndex(pageRagdollScrag)),
        open("Fiend", pageIndex(pageRagdollFiend)),
        open("Shambler", pageIndex(pageRagdollShambler)),
        open("Vore", pageIndex(pageRagdollVore)),
        open("Gremlin", pageIndex(pageRagdollGremlin)),
        open("Centroid", pageIndex(pageRagdollCentroid)).help("A centroid's ragdoll (Scourge of Armagon's scorpion)."),
        open("Zombie", pageIndex(pageRagdollZombie)).help("A zombie's ragdoll: only when its head is cut off (Gore > Decapitation)."),
        open("Mummy", pageIndex(pageRagdollMummy)).help("A mummy's ragdoll (Dissolution of Eternity): only when its head is cut off (Gore > Decapitation)."),
        header("Taking Them"),
        slider("Grip Reach", vr_ragdoll_grab_reach, 1.f, 16.f, 1.f, "%.0f units").extend()
            .help("How near a limb your hand must be to take it (vr_ragdoll_grab_reach)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Grunt: the grunt's own physics (vr_ragdoll_army_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollGrunt()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_army_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_army_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_army_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_army_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_army_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_army_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_army_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_army_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_army_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_army_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_army_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_army_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_army_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_army_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_army_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_army_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_army_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_army_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_army_start -1; vr_ragdoll_army_mass -1; vr_ragdoll_army_friction -1; "
                              "vr_ragdoll_army_joint_friction -1; vr_ragdoll_army_joint_stiffness -1; vr_ragdoll_army_limits -1; "
                              "vr_ragdoll_army_damping -1; vr_ragdoll_army_blast -1; vr_ragdoll_army_inherit -1")
            .help("The grunt's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Knight: the knight's own physics (vr_ragdoll_knight_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollKnight()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_knight_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_knight_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_knight_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_knight_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_knight_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_knight_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_knight_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_knight_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_knight_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_knight_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_knight_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_knight_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_knight_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_knight_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_knight_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_knight_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_knight_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_knight_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_knight_start -1; vr_ragdoll_knight_mass -1; vr_ragdoll_knight_friction -1; "
                              "vr_ragdoll_knight_joint_friction -1; vr_ragdoll_knight_joint_stiffness -1; vr_ragdoll_knight_limits -1; "
                              "vr_ragdoll_knight_damping -1; vr_ragdoll_knight_blast -1; vr_ragdoll_knight_inherit -1")
            .help("The knight's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Ogre: the ogre's own physics (vr_ragdoll_ogre_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollOgre()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_ogre_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_ogre_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_ogre_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_ogre_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_ogre_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_ogre_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_ogre_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_ogre_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_ogre_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_ogre_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_ogre_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_ogre_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_ogre_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_ogre_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_ogre_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_ogre_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_ogre_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_ogre_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_ogre_start -1; vr_ragdoll_ogre_mass -1; vr_ragdoll_ogre_friction -1; "
                              "vr_ragdoll_ogre_joint_friction -1; vr_ragdoll_ogre_joint_stiffness -1; vr_ragdoll_ogre_limits -1; "
                              "vr_ragdoll_ogre_damping -1; vr_ragdoll_ogre_blast -1; vr_ragdoll_ogre_inherit -1")
            .help("The ogre's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Enforcer: the enforcer's own physics (vr_ragdoll_enforcer_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollEnforcer()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_enforcer_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_enforcer_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_enforcer_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_enforcer_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_enforcer_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_enforcer_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_enforcer_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_enforcer_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_enforcer_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_enforcer_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_enforcer_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_enforcer_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_enforcer_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_enforcer_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_enforcer_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_enforcer_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_enforcer_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_enforcer_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_enforcer_start -1; vr_ragdoll_enforcer_mass -1; vr_ragdoll_enforcer_friction -1; "
                              "vr_ragdoll_enforcer_joint_friction -1; vr_ragdoll_enforcer_joint_stiffness -1; vr_ragdoll_enforcer_limits -1; "
                              "vr_ragdoll_enforcer_damping -1; vr_ragdoll_enforcer_blast -1; vr_ragdoll_enforcer_inherit -1")
            .help("The enforcer's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Death Knight: the death knight's own physics (vr_ragdoll_hknight_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollDeathKnight()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_hknight_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_hknight_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_hknight_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_hknight_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_hknight_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_hknight_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_hknight_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_hknight_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_hknight_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_hknight_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_hknight_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_hknight_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_hknight_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_hknight_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_hknight_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_hknight_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_hknight_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_hknight_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_hknight_start -1; vr_ragdoll_hknight_mass -1; vr_ragdoll_hknight_friction -1; "
                              "vr_ragdoll_hknight_joint_friction -1; vr_ragdoll_hknight_joint_stiffness -1; vr_ragdoll_hknight_limits -1; "
                              "vr_ragdoll_hknight_damping -1; vr_ragdoll_hknight_blast -1; vr_ragdoll_hknight_inherit -1")
            .help("The death knight's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Rottweiler: the rottweiler's own physics (vr_ragdoll_dog_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollRottweiler()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_dog_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_dog_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_dog_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_dog_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_dog_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_dog_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_dog_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_dog_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_dog_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_dog_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_dog_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_dog_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_dog_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_dog_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_dog_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_dog_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_dog_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_dog_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_dog_start -1; vr_ragdoll_dog_mass -1; vr_ragdoll_dog_friction -1; "
                              "vr_ragdoll_dog_joint_friction -1; vr_ragdoll_dog_joint_stiffness -1; vr_ragdoll_dog_limits -1; "
                              "vr_ragdoll_dog_damping -1; vr_ragdoll_dog_blast -1; vr_ragdoll_dog_inherit -1")
            .help("The rottweiler's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Scrag: the scrag's own physics (vr_ragdoll_wizard_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollScrag()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_wizard_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_wizard_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_wizard_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_wizard_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_wizard_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_wizard_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_wizard_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_wizard_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_wizard_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_wizard_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_wizard_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_wizard_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_wizard_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_wizard_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_wizard_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_wizard_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_wizard_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_wizard_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_wizard_start -1; vr_ragdoll_wizard_mass -1; vr_ragdoll_wizard_friction -1; "
                              "vr_ragdoll_wizard_joint_friction -1; vr_ragdoll_wizard_joint_stiffness -1; vr_ragdoll_wizard_limits -1; "
                              "vr_ragdoll_wizard_damping -1; vr_ragdoll_wizard_blast -1; vr_ragdoll_wizard_inherit -1")
            .help("The scrag's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Zombie: the zombie's own physics (vr_ragdoll_zombie_*), each one Global (the one for
// all monsters) or its own. Its ragdoll is only made when it is beheaded (QC vr_decap.qc).
[[nodiscard]] za::Vector<Item> pageRagdollZombie()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_zombie_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_zombie_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_zombie_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_zombie_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_zombie_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_zombie_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_zombie_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_zombie_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_zombie_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_zombie_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_zombie_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_zombie_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_zombie_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_zombie_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_zombie_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_zombie_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_zombie_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_zombie_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_zombie_start -1; vr_ragdoll_zombie_mass -1; vr_ragdoll_zombie_friction -1; "
                              "vr_ragdoll_zombie_joint_friction -1; vr_ragdoll_zombie_joint_stiffness -1; vr_ragdoll_zombie_limits -1; "
                              "vr_ragdoll_zombie_damping -1; vr_ragdoll_zombie_blast -1; vr_ragdoll_zombie_inherit -1")
            .help("The zombie's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Mummy: the mummy's own physics (vr_ragdoll_mummy_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollMummy()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_mummy_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_mummy_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_mummy_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_mummy_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_mummy_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_mummy_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_mummy_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_mummy_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_mummy_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_mummy_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_mummy_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_mummy_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_mummy_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_mummy_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_mummy_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_mummy_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_mummy_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_mummy_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_mummy_start -1; vr_ragdoll_mummy_mass -1; vr_ragdoll_mummy_friction -1; "
                              "vr_ragdoll_mummy_joint_friction -1; vr_ragdoll_mummy_joint_stiffness -1; vr_ragdoll_mummy_limits -1; "
                              "vr_ragdoll_mummy_damping -1; vr_ragdoll_mummy_blast -1; vr_ragdoll_mummy_inherit -1")
            .help("The mummy's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Gremlin: the gremlin's own physics (vr_ragdoll_gremlin_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollGremlin()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_gremlin_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_gremlin_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_gremlin_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_gremlin_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_gremlin_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_gremlin_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_gremlin_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_gremlin_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_gremlin_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_gremlin_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_gremlin_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_gremlin_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_gremlin_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_gremlin_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_gremlin_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_gremlin_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_gremlin_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_gremlin_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_gremlin_start -1; vr_ragdoll_gremlin_mass -1; vr_ragdoll_gremlin_friction -1; "
                              "vr_ragdoll_gremlin_joint_friction -1; vr_ragdoll_gremlin_joint_stiffness -1; vr_ragdoll_gremlin_limits -1; "
                              "vr_ragdoll_gremlin_damping -1; vr_ragdoll_gremlin_blast -1; vr_ragdoll_gremlin_inherit -1")
            .help("The gremlin's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Vore: the vore's own physics (vr_ragdoll_vore_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollVore()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_vore_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_vore_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_vore_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_vore_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_vore_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_vore_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_vore_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_vore_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_vore_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_vore_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_vore_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_vore_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_vore_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_vore_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_vore_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_vore_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_vore_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_vore_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_vore_start -1; vr_ragdoll_vore_mass -1; vr_ragdoll_vore_friction -1; "
                              "vr_ragdoll_vore_joint_friction -1; vr_ragdoll_vore_joint_stiffness -1; vr_ragdoll_vore_limits -1; "
                              "vr_ragdoll_vore_damping -1; vr_ragdoll_vore_blast -1; vr_ragdoll_vore_inherit -1")
            .help("The vore's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Centroid: the centroid's own physics (vr_ragdoll_centroid_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollCentroid()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_centroid_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_centroid_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_centroid_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_centroid_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_centroid_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_centroid_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_centroid_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_centroid_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_centroid_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_centroid_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_centroid_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_centroid_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_centroid_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_centroid_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_centroid_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_centroid_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_centroid_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_centroid_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_centroid_start -1; vr_ragdoll_centroid_mass -1; vr_ragdoll_centroid_friction -1; "
                              "vr_ragdoll_centroid_joint_friction -1; vr_ragdoll_centroid_joint_stiffness -1; vr_ragdoll_centroid_limits -1; "
                              "vr_ragdoll_centroid_damping -1; vr_ragdoll_centroid_blast -1; vr_ragdoll_centroid_inherit -1")
            .help("The centroid's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Shambler: the shambler's own physics (vr_ragdoll_shambler_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollShambler()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_shambler_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_shambler_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_shambler_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_shambler_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_shambler_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_shambler_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_shambler_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_shambler_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_shambler_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_shambler_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_shambler_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_shambler_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_shambler_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_shambler_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_shambler_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_shambler_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_shambler_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_shambler_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_shambler_start -1; vr_ragdoll_shambler_mass -1; vr_ragdoll_shambler_friction -1; "
                              "vr_ragdoll_shambler_joint_friction -1; vr_ragdoll_shambler_joint_stiffness -1; vr_ragdoll_shambler_limits -1; "
                              "vr_ragdoll_shambler_damping -1; vr_ragdoll_shambler_blast -1; vr_ragdoll_shambler_inherit -1")
            .help("The shambler's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Fiend: the fiend's own physics (vr_ragdoll_demon_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollFiend()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_demon_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_demon_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_demon_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_demon_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_demon_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_demon_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_demon_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_demon_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_demon_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_demon_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_demon_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_demon_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_demon_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_demon_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_demon_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_demon_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_demon_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_demon_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_demon_start -1; vr_ragdoll_demon_mass -1; vr_ragdoll_demon_friction -1; "
                              "vr_ragdoll_demon_joint_friction -1; vr_ragdoll_demon_joint_stiffness -1; vr_ragdoll_demon_limits -1; "
                              "vr_ragdoll_demon_damping -1; vr_ragdoll_demon_blast -1; vr_ragdoll_demon_inherit -1")
            .help("The fiend's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}
