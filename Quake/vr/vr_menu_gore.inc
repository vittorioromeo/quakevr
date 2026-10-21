// vr_menu_gore.inc -- gore: Small Gibs, Decapitation, Limb Gore, Gore. Part of vr_menu.cpp (included where it was: the
// same translation unit, so its anonymous namespace).

// Small gibs (QC vr_smallgibs.qc; ROUND21.md, "Small gibs"): chunks torn out by hits, carried and thrown as the gibs.
[[nodiscard]] za::Vector<Item> pageSmallGibsEnemies(); // (below: each monster's counts)
[[nodiscard]] za::Vector<Item> pageSmallGibs()
{
    return {
        toggle("Small Gibs", vr_smallgibs)
            .help("Hits tear small chunks of meat out of monsters and corpses: the gibs' models, a rock's size or half of it. They bleed, can be picked up and thrown, and fade away after a while."),
        toggle("From You Too", vr_smallgibs_player).help("Hits on you tear them out of you as well."),
        header("When"),
        slider("Least Damage", vr_smallgibs_min_damage, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("A hit with less damage tears none out (a shotgun blast's pellets count as one hit). Above it the chance rises."),
        slider("Damage for a Sure One", vr_smallgibs_full_damage, 1.f, 200.f, 5.f, "%.0f").extend(1.f, 1000.f)
            .help("From this damage a hit always tears some out (times the weapon's chance below). Quad Damage, the chainsaw, gibbing and bursting a large gib always do."),
        slider("Chance Curve", vr_smallgibs_curve, 0.5f, 3.f, 0.1f, "%.1f")
            .help("How the chance rises between the two: 1 straight, more makes light hits rarer (a plain shotgun blast) while hard ones stay sure."),
        slider("One More Each", vr_smallgibs_damage_per_gib, 5.f, 200.f, 5.f, "%.0f damage").extend(1.f, 1000.f)
            .help("Harder hits tear more out: one more for about each this much damage."),
        slider("Most From a Hit", vr_smallgibs_per_hit, 1.f, 10.f, 1.f, "%.0f"),
        header("Chance by Weapon"),
        slider("Shotguns", vr_smallgibs_shots, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The shotgun's blast rarely tears one out, the double-barrelled one's often (their pellets' damage summed). A grunt's gun too."),
        slider("Nails", vr_smallgibs_nails, 0.f, 3.f, 0.1f, "%.1fx").help("Nails and lasers (0: never, but under Quad Damage)."),
        slider("Blades", vr_smallgibs_blades, 0.f, 3.f, 0.1f, "%.1fx").help("The axe's and a sword's edge or point, swung or thrown fast."),
        slider("Blunt Blows", vr_smallgibs_blunt, 0.f, 3.f, 0.1f, "%.1fx")
            .help("Punches, a pommel or hilt, a gun or the crowbar swung, Mjolnir, bashes and headbutts: only strong ones, rarely."),
        slider("Props", vr_smallgibs_props, 0.f, 3.f, 0.1f, "%.1fx").help("A rock, a brick or another thing thrown hard or swung in the fist."),
        slider("Explosions", vr_smallgibs_explosions, 0.f, 3.f, 0.1f, "%.1fx"),
        slider("Everything Else", vr_smallgibs_other, 0.f, 3.f, 0.1f, "%.1fx").help("Lightning, monsters' blows and the rest."),
        slider("Chainsaw: One Each", vr_smallgibs_saw_interval, 0.05f, 1.f, 0.05f, "%.2f s").help("The running chain tears one out of a monster or a corpse this often."),
        slider("With a Gibbing", vr_smallgibs_gibbing, 0.f, 20.f, 1.f, "%.0f").help("How many fly with a body's gibs when it is gibbed."),
        slider("A Large Gib Bursts Into", vr_smallgibs_burst, 0.f, 10.f, 1.f, "%.0f").help("How many a gib breaks into when it is destroyed (a head half as many again)."),
        header("Brain Chunks"),
        toggle("Brain Chunks", vr_smallgibs_brains)
            .help("A head that bursts (a head gib destroyed, or a head popped by a headshot) throws torn lumps of brain too: small gibs in all else (picked up, thrown, bleeding, fading), lighter."),
        slider("From a Head Gib", vr_smallgibs_brains_burst, 0.f, 20.f, 1.f, "%.0f").help("How many when a head gib is destroyed (with its small gibs of meat)."),
        slider("From a Head Pop", vr_smallgibs_brains_pop, 0.f, 20.f, 1.f, "%.0f").help("How many when a headshot pops a head (Decapitation > Head Shots)."),
        slider("Brain Chunk Size", vr_smallgibs_brains_size, 0.2f, 2.f, 0.05f, "%.2fx").help("Times the small gibs' size (Smallest, Largest)."),
        slider("Brain Chunk Mass", vr_smallgibs_brains_mass, 0.05f, 2.f, 0.05f, "%.2f kg"),
        slider("Brain Chunk Gloss", vr_smallgibs_brains_gloss, 0.f, 2.f, 0.1f, "%.1fx")
            .help("Their wet sheen, always on: the highlights on the folds and a warm, reddish rim; 0 dry and matt "
                  "(vr_smallgibs_brains_gloss)."),
        slider("Brain Chunk Jiggle", vr_smallgibs_brains_jiggle, 0.f, 4.f, 0.25f, "%.2fx")
            .help("They wobble when knocked (landing, bouncing, shaken in a hand): squashed and bulging, settling in "
                  "half a second. Drawn only: their physics stay rigid. 0 off (vr_smallgibs_brains_jiggle)."),
        slider("Brain Chunk Speed", vr_smallgibs_brains_speed, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f)
            .help("How fast brain chunks fly out of a head burst: their own, not Speed below (times A Large Gib Bursts: "
                  "Speed). The default is Speed's, which is what they used then (vr_smallgibs_brains_speed)."),
        slider("Brain Chunk Up", vr_smallgibs_brains_up, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f)
            .help("And upwards: their own, not Up below (times A Large Gib Bursts: Up) (vr_smallgibs_brains_up)."),
        header("Flight and Size"),
        slider("Speed", vr_smallgibs_speed, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f).help("How fast they fly out from where the hit landed (times Flight by Situation below)."),
        slider("Up", vr_smallgibs_up, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f).help("And upwards (times Flight by Situation below)."),
        slider("Pass Through the Body", vr_smallgibs_grace, 0.f, 0.5f, 0.05f, "%.2f s").help("For this long they don't collide with the body they came from."),
        toggle("Not Pushed Out of Bodies", vr_smallgibs_pass_inside)
            .help("Still inside a monster, you, a hand or a blade after that (melee, the chainsaw), they pass through it until clear, not pushed out at 2 to 3 m/s."),
        slider("Not Batted Away For", vr_smallgibs_blow_grace, 0.f, 1.f, 0.05f, "%.2f s")
            .help("For this long after it is torn out, your hands, weapons and what they hold pass through a small gib: the blade that tore it out doesn't bat it away. After it you can bat it."),
        toggle("Burst by Blows Meanwhile", vr_smallgibs_blow_burst)
            .help("In that time a blow still bursts one (the blade that tore it out too, going on through it), with Can Be Destroyed on; it is never batted or pushed. Off: blows pass through it."),
        slider("Smallest", vr_smallgibs_size_min, 0.1f, 2.f, 0.05f, "%.2fx rock"),
        slider("Largest", vr_smallgibs_size_max, 0.1f, 2.f, 0.05f, "%.2fx rock"),
        slider("Mass", vr_smallgibs_mass, 0.05f, 5.f, 0.05f, "%.2f kg").help("Light: thrown, they hurt little."),
        header("Flight by Situation"),
        slider("Melee: Speed", vr_smallgibs_speed_melee, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A hand's blow: a blade, a punch, a pommel or hilt, a rock or brick in the fist; bashes and headbutts."),
        slider("Melee: Up", vr_smallgibs_up_melee, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Chainsaw: Speed", vr_smallgibs_speed_saw, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. The chainsaw's running chain or a swing of it."),
        slider("Chainsaw: Up", vr_smallgibs_up_saw, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Guns: Speed", vr_smallgibs_speed_guns, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. Shotguns, nails, lasers, lightning, a grunt's gun."),
        slider("Guns: Up", vr_smallgibs_up_guns, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Explosions: Speed", vr_smallgibs_speed_explosions, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. Rockets, grenades, every blast."),
        slider("Explosions: Up", vr_smallgibs_up_explosions, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Thrown: Speed", vr_smallgibs_speed_thrown, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A thrown axe, sword, rock, brick or gib; a projectile batted back."),
        slider("Thrown: Up", vr_smallgibs_up_thrown, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Everything Else: Speed", vr_smallgibs_speed_other, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. Monsters' blows and the rest."),
        slider("Everything Else: Up", vr_smallgibs_up_other, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Gibbing a Live One: Speed", vr_smallgibs_speed_gibbing, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A living monster gibbed: they fly out of its whole body, with its gibs' push."),
        slider("Gibbing a Live One: Up", vr_smallgibs_up_gibbing, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Gibbing a Corpse: Speed", vr_smallgibs_speed_corpse, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A corpse gibbed where it lies (struck, shot, blown up)."),
        slider("Gibbing a Corpse: Up", vr_smallgibs_up_corpse, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("A Large Gib Bursts: Speed", vr_smallgibs_speed_burst, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A gib or head destroyed breaks into them."),
        slider("A Large Gib Bursts: Up", vr_smallgibs_up_burst, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        header("How Many and How Long"),
        slider("Most Lying About", vr_smallgibs_max, 1.f, 128.f, 1.f, "%.0f").extend(1.f, 256.f).help("Past it, the oldest go first (not one in your hand)."),
        slider("Last", vr_smallgibs_time, 0.f, 120.f, 5.f, "%.0f s").help("Then they fade away (0: never). Held, and in the air after, they wait."),
        toggle("Can Be Destroyed", vr_smallgibs_destroy).help("Shots and blows burst them into a puff of blood. Off: they pass through them."),
        open("Per Enemy", pageIndex(pageSmallGibsEnemies))
            .help("Each monster's own counts: a hit's, a gibbing's and a head or gib burst's, times these."),
    };
}

// Small Gibs > Per Enemy (QC vr_smallgibs.qc's VR_SmallGib_Mult; the same monsters as Corpse Damage and Health's
// Corpse Health, by Monster): each monster's small gibs and brain chunks, times these.
[[nodiscard]] za::Vector<Item> pageSmallGibsEnemies()
{
    return {
        open("Small Gibs, Per Enemy: Small Gibs", pageIndex(pageSmallGibs)).help("The counts themselves, the flight and Brain Chunks are on Small Gibs."),
        header("Small Gibs, by Monster"),
        slider("Grunt", vr_smallgibs_mult_grunt, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("Its small gibs, times this: what a hit tears out, what flies with its gibs, what its gibs and head burst into (vr_smallgibs_mult_grunt)."),
        slider("Enforcer", vr_smallgibs_mult_enforcer, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An enforcer's (vr_smallgibs_mult_enforcer)."),
        slider("Rottweiler", vr_smallgibs_mult_dog, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rottweiler's (vr_smallgibs_mult_dog)."),
        slider("Fiend", vr_smallgibs_mult_fiend, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A fiend's (vr_smallgibs_mult_fiend)."),
        slider("Ogre", vr_smallgibs_mult_ogre, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An ogre's (vr_smallgibs_mult_ogre)."),
        slider("Knight", vr_smallgibs_mult_knight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A knight's (vr_smallgibs_mult_knight)."),
        slider("Hell Knight", vr_smallgibs_mult_hellknight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A hell knight's (vr_smallgibs_mult_hellknight)."),
        slider("Vore", vr_smallgibs_mult_vore, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A vore's (vr_smallgibs_mult_vore)."),
        slider("Shambler", vr_smallgibs_mult_shambler, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A shambler's (vr_smallgibs_mult_shambler)."),
        slider("Scrag", vr_smallgibs_mult_scrag, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A scrag's (vr_smallgibs_mult_scrag)."),
        slider("Rotfish", vr_smallgibs_mult_fish, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rotfish's (vr_smallgibs_mult_fish)."),
        slider("Gremlin (Hipnotic)", vr_smallgibs_mult_gremlin, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A gremlin's (vr_smallgibs_mult_gremlin)."),
        slider("Centroid (Rogue)", vr_smallgibs_mult_scourge, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A centroid's (vr_smallgibs_mult_scourge)."),
        slider("Eel (Rogue)", vr_smallgibs_mult_eel, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An eel's (vr_smallgibs_mult_eel)."),
        slider("Zombie (Beheaded)", vr_smallgibs_mult_zombie, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A zombie's, beheaded or not (vr_smallgibs_mult_zombie)."),
        slider("Mummy (Rogue, Beheaded)", vr_smallgibs_mult_mummy, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A mummy's (vr_smallgibs_mult_mummy)."),
        header("Brain Chunks, by Monster"),
        slider("Grunt", vr_smallgibs_brains_mult_grunt, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("Its brain chunks, times this: from its head gib destroyed or its head popped by a headshot. A head it "
                  "lost carries them with it (vr_smallgibs_brains_mult_grunt)."),
        slider("Enforcer", vr_smallgibs_brains_mult_enforcer, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An enforcer's (vr_smallgibs_brains_mult_enforcer)."),
        slider("Rottweiler", vr_smallgibs_brains_mult_dog, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rottweiler's (vr_smallgibs_brains_mult_dog)."),
        slider("Fiend", vr_smallgibs_brains_mult_fiend, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A fiend's (vr_smallgibs_brains_mult_fiend)."),
        slider("Ogre", vr_smallgibs_brains_mult_ogre, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An ogre's (vr_smallgibs_brains_mult_ogre)."),
        slider("Knight", vr_smallgibs_brains_mult_knight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A knight's (vr_smallgibs_brains_mult_knight)."),
        slider("Hell Knight", vr_smallgibs_brains_mult_hellknight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A hell knight's (vr_smallgibs_brains_mult_hellknight)."),
        slider("Vore", vr_smallgibs_brains_mult_vore, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A vore's (vr_smallgibs_brains_mult_vore)."),
        slider("Shambler", vr_smallgibs_brains_mult_shambler, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A shambler's (vr_smallgibs_brains_mult_shambler)."),
        slider("Scrag", vr_smallgibs_brains_mult_scrag, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A scrag's (vr_smallgibs_brains_mult_scrag)."),
        slider("Rotfish", vr_smallgibs_brains_mult_fish, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rotfish's (vr_smallgibs_brains_mult_fish)."),
        slider("Gremlin (Hipnotic)", vr_smallgibs_brains_mult_gremlin, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A gremlin's (vr_smallgibs_brains_mult_gremlin)."),
        slider("Centroid (Rogue)", vr_smallgibs_brains_mult_scourge, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A centroid's (vr_smallgibs_brains_mult_scourge)."),
        slider("Eel (Rogue)", vr_smallgibs_brains_mult_eel, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An eel's (vr_smallgibs_brains_mult_eel)."),
        slider("Zombie (Beheaded)", vr_smallgibs_brains_mult_zombie, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A zombie's (vr_smallgibs_brains_mult_zombie)."),
        slider("Mummy (Rogue, Beheaded)", vr_smallgibs_brains_mult_mummy, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A mummy's (vr_smallgibs_brains_mult_mummy)."),
    };
}

// Gore > Decapitation (ROUND21.md, "Decapitation"; QC vr_decap.qc, vr_box3d.cpp's "Decapitation"): when a blow cuts a
// head off, how the head flies, the stump's fountain.
[[nodiscard]] za::Vector<Item> pageDecapitation()
{
    return {
        toggle("Decapitation", vr_decap)
            .help("A killing slash at the head of a grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler, gremlin or mummy cuts it "
                  "off: a sword's or an axe's blade swung across (not a stab, not the pommel or the hilt). The body falls at "
                  "once as a headless ragdoll, the head flies off, the neck spurts blood. Needs Ragdolls on (vr_decap)."),
        slider("Least Swing Speed", vr_decap_speed, 0.f, 15.f, 0.5f, "%.1f m/s").extend(0.f, 30.f)
            .help("How fast the blade must move where it strikes (and a thrown axe fly) to cut the head off (vr_decap_speed)."),
        slider("Least Slash Angle", vr_decap_slash_angle, 0.f, 90.f, 5.f, "%.0f deg")
            .help("How far off the blade's line the struck point must move to cut (nearer its line it is a stab, which "
                  "doesn't behead); 0: any motion (vr_decap_slash_angle)."),
        slider("Head Zone Size", vr_decap_head_size, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.1f, 4.f)
            .help("Melee only (a slash, the chainsaw, a corpse's head): how big the head zone a blade must strike to behead is, "
                  "times its size for headshots. Debug > Show Hit Zones draws it (vr_decap_head_size)."),
        slider("Neck", vr_decap_neck, 0.f, 20.f, 1.f, "%.0f units").extend(0.f, 40.f)
            .help("Melee only: the head zone reaches this far down below the head's middle, over the neck (a ragdoll's neck "
                  "at least this far round it) (vr_decap_neck)."),
        slider("Head Speed", vr_decap_head_speed, 0.f, 1.5f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("How much of the blade's speed the head flies off with; it spins as the swing turns it "
                  "(vr_decap_head_speed)."),
        slider("Head Thrown Up", vr_decap_head_lift, 0.f, 400.f, 10.f, "%.0f u/s").extend(0.f, 1000.f)
            .help("And how fast it is thrown upwards besides (vr_decap_head_lift)."),
        slider("Fountain", vr_decap_fountain, 0.f, 8.f, 0.25f, "%.2f s").extend(0.f, 30.f)
            .help("How long the neck spurts blood, in beats, dying away (0: none) (vr_decap_fountain)."),
        slider("Flying Head's Fountain", vr_limbs_end_fountain, 0.f, 4.f, 0.25f, "%.2f s").extend(0.f, 20.f)
            .help("How long the neck of a head cut off spurts a smaller fountain as it flies (and the cut end of a limb: "
                  "Limb Gore's Cut End Fountain, the same setting) (0: none) (vr_limbs_end_fountain)."),
        toggle("Keeps Its Own Motion", vr_decap_own_motion)
            .help("A beheaded (or popped) monster's headless body goes on as it was moving (running at you: it stumbles on "
                  "towards you) in full; a pop slows only the shot's knock (Body's Speed After a Pop). Off: as before: its "
                  "run not carried, a pop slowing all of it (vr_decap_own_motion)."),
        header("What Beheads"),
        toggle("Corpses", vr_decap_corpses)
            .help("A slash at a ragdoll's (or a dying body's) head cuts it off instead of hurting the corpse: it never "
                  "bursts into gibs from it (vr_decap_corpses)."),
        toggle("Zombies", vr_decap_zombies)
            .help("A slash at a zombie's head cuts it off whatever the damage: it falls as a headless ragdoll and never "
                  "gets up again. Besides gibbing it, the only way to kill one (vr_decap_zombies)."),
        toggle("Thrown Axes", vr_decap_thrown)
            .help("An axe thrown into a head edge first (as it sticks) cuts it off when it kills (vr_decap_thrown)."),
        toggle("Chainsaw", vr_decap_chainsaw)
            .help("The chainsaw's running bar at the neck (or a swing of it) cuts the head off when it kills; a corpse's "
                  "at once (vr_decap_chainsaw)."),
        header("Head Shots"),
        toggle("Shotgun", vr_decap_shotgun)
            .help("A shotgun blast that kills with a headshot pops the head: it bursts in blood and small gibs, and the body "
                  "falls at once as a headless ragdoll (never gibbed, nor its death animation) (vr_decap_shotgun)."),
        toggle("Super Shotgun", vr_decap_super_shotgun)
            .help("The same for a super shotgun blast (vr_decap_super_shotgun)."),
        toggle("Lightning Gun", vr_decap_lightning)
            .help("The same for a lightning gun bolt at the head that kills (vr_decap_lightning)."),
        slider("Blast's Head Share", vr_decap_head_share, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of a blast's damage on him must come from pellets at the head (the headshot multiplier in) for "
                  "it to count as a headshot; 0: any pellet at the head (vr_decap_head_share)."),
        slider("Body's Speed After a Pop", vr_decap_pop_body_speed, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 2.f)
            .help("A popped head's body: how much of the shot's knock its headless ragdoll keeps (its own motion, as it ran, "
                  "in full: Keeps Its Own Motion). The shot's force went into the head, so it slumps where it stood; 1: flung "
                  "as before (vr_decap_pop_body_speed)."),
        slider("Head Chance", vr_decap_chance_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Every beheading or head pop's chance times this (a blade's sure cut too: below 1 it may fail); Quad Damage "
                  "always pops. Limbs: Gore > Limb Gore (vr_decap_chance_scale)."),
        header("Head Pop Chance"),
        slider("Always Within", vr_decap_pop_always_range, 0.f, 10.f, 0.5f, "%.1f lengths").extend(0.f, 50.f)
            .help("A shotgun's or super shotgun's headshot kill this near (in player lengths, 56 units: about 1.75 m) "
                  "always pops the head (vr_decap_pop_always_range)."),
        slider("Never Beyond", vr_decap_pop_never_range, 1.f, 40.f, 1.f, "%.0f lengths").extend(0.f, 200.f)
            .help("This far or farther it never does; between the two, the chance falls off (vr_decap_pop_never_range)."),
        slider("Super Shotgun Chance", vr_decap_pop_ssg_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The super shotgun's chance between the ranges, times this (vr_decap_pop_ssg_scale)."),
        slider("Super Shotgun Falloff", vr_decap_pop_ssg_falloff, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("How fast its chance dies away with range: 1 straight down to the far range, higher sooner, 0 not at all "
                  "(vr_decap_pop_ssg_falloff)."),
        slider("Shotgun Chance", vr_decap_pop_sg_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The shotgun's chance between the ranges, times this (vr_decap_pop_sg_scale)."),
        slider("Shotgun Falloff", vr_decap_pop_sg_falloff, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("How fast the shotgun's chance dies away with range (stronger than the super shotgun's) "
                  "(vr_decap_pop_sg_falloff)."),
        slider("Pellets at the Head", vr_decap_pop_pellet_weight, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the share of the blast's pellets that struck the head weighs on the chance: 1 in proportion, "
                  "0 not at all (vr_decap_pop_pellet_weight)."),
        toggle("Lightning Always Pops", vr_decap_pop_lightning_always)
            .help("A lightning bolt's headshot kill always pops the head, at any range; off: the ranges, with the super "
                  "shotgun's falloff (vr_decap_pop_lightning_always)."),
        slider("Enforcer Laser", vr_decap_pop_laser, 0.f, 1.f, 0.05f, "%.2f")
            .help("An enforcer's laser bolt that kills a monster with a head or limb hit (an enemy's bolt into another "
                  "monster, or your own enforcer's rifle) pops it at this chance, at any range (0: never) (vr_decap_pop_laser)."),
        toggle("Thrown Things", vr_decap_pop_thrown)
            .help("A blunt weapon or prop thrown (or flung) into a head that kills pops it, when heavy enough (not a sword, "
                  "an axe or the chainsaw: an axe's edge cuts the head off) (vr_decap_pop_thrown)."),
        slider("Thrown: Always From", vr_decap_pop_thrown_mass, 0.f, 20.f, 0.5f, "%.1f kg").extend(0.f, 200.f)
            .help("This heavy or heavier, a thrown thing's headshot kill always pops the head (vr_decap_pop_thrown_mass)."),
        slider("Thrown: Lighter's Chance", vr_decap_pop_thrown_light_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("A lighter one's chance (0: never) (vr_decap_pop_thrown_light_chance)."),
        slider("Fist", vr_decap_pop_fist_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("A punch that kills with a head hit pops the head at this chance at the hardest hit, times the hit's "
                  "hardness to the Hardness Curve (the rows below): very rarely by default (vr_decap_pop_fist_scale)."),
        slider("Gun Butt", vr_decap_pop_gun_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("The same for a gun swung, its butt or a pistol-whip (vr_decap_pop_gun_scale)."),
        slider("Crowbar", vr_decap_pop_crowbar_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("The same for the crowbar (vr_decap_pop_crowbar_scale)."),
        slider("Pommel", vr_decap_pop_pommel_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("The same for a sword's, axe's, chainsaw's or Mjolnir's pommel or handle end (vr_decap_pop_pommel_scale)."),
        slider("Club", vr_decap_pop_club_scale, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("The same for a carried prop swung as a club, a wall torch (vr_decap_pop_club_scale)."),
        slider("Mjolnir", vr_decap_pop_mjolnir_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The same for Mjolnir's head: above 1 it is sure well before the hardest hit (1.75: a solid blow nearly "
                  "always pops it) (vr_decap_pop_mjolnir_scale)."),
        slider("Soft Hit Speed", vr_decap_pop_melee_soft_speed, 0.f, 10.f, 0.5f, "%.1f m/s").extend(0.f, 30.f)
            .help("The striking part this slow or slower: no hardness from its speed (vr_decap_pop_melee_soft_speed)."),
        slider("Hard Hit Speed", vr_decap_pop_melee_hard_speed, 1.f, 20.f, 0.5f, "%.1f m/s").extend(0.f, 40.f)
            .help("This fast or faster: all of it (vr_decap_pop_melee_hard_speed)."),
        slider("Soft Hit Damage", vr_decap_pop_melee_soft_damage, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("The blow's damage (the headshot multiplier in) this or less: no hardness from it "
                  "(vr_decap_pop_melee_soft_damage)."),
        slider("Hard Hit Damage", vr_decap_pop_melee_hard_damage, 1.f, 200.f, 1.f, "%.0f").extend(0.f, 1000.f)
            .help("This or more: all of it (vr_decap_pop_melee_hard_damage)."),
        slider("Damage's Weight", vr_decap_pop_melee_damage_weight, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the damage weighs in the hit's hardness; the rest is its speed (vr_decap_pop_melee_damage_weight)."),
        slider("Hardness Curve", vr_decap_pop_melee_curve, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("The hardness to this power: higher, soft blows nearer never; 1 straight (vr_decap_pop_melee_curve)."),
        toggle("Quad Damage: Always Pop", vr_decap_pop_quad)
            .help("With Quad Damage, every headshot kill pops the head: any gun (nails, rockets and grenades at the head too), "
                  "any blow (a blade's stab), any throw or prop, at any range, by no chance (a slash still cuts it off) "
                  "(vr_decap_pop_quad)."),
    };
}

// Gore > Limb Gore (ROUND21.md, "Limb gore"; QC vr_limbs.qc, vr_box3d.cpp's "Limb gore", vr_limbmodel.cpp): every
// limb as the head: cut off or popped, by the head's chance rules.
[[nodiscard]] za::Vector<Item> pageLimbGore()
{
    return {
        toggle("Limb Gore", vr_limbs)
            .help("A monster's arms, legs, tails and the rest come off as its head does (the monsters with ragdolls): a blade's "
                  "slash cuts the limb off at the joint nearest the hit (the elbow the forearm, the shoulder the whole arm), "
                  "a shot, a blunt blow or a bolt pops it, by the head's chances (Decapitation's rows). A living one only by "
                  "the blow that kills it: it falls at once as a ragdoll; the limb flies off (pick it up, throw it), the "
                  "stump spurts blood. Needs Ragdolls on (vr_limbs)."),
        slider("Limb Chance", vr_limbs_chance_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Every limb cut or pop's chance times this (a blade's sure cut too: below 1 it may fail); Quad Damage "
                  "always pops (vr_limbs_chance_scale)."),
        slider("Head Chance", vr_decap_chance_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("And every beheading or head pop's chance times this (1: as before) (vr_decap_chance_scale)."),
        toggle("Corpses", vr_limbs_corpses)
            .help("Corpses and ragdolls lose limbs too, each on its own, down to the torso: a slash cuts, a blunt blow or a "
                  "shot pops one (vr_limbs_corpses)."),
        toggle("Zombies and Mummies", vr_limbs_zombies)
            .help("A zombie or a mummy losing a limb dies for good (a zombie: whatever the damage, as beheaded). Off: their "
                  "limbs stay on (their heads: Decapitation > Zombies) (vr_limbs_zombies)."),
        slider("Body's Speed After", vr_limbs_body_speed, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 2.f)
            .help("A living monster's limb cut or popped: how much of the blow's knock its ragdoll keeps (it slumps where it "
                  "stood; 1: flung as a kill) (vr_limbs_body_speed)."),
        slider("Most Limbs Lying About", vr_limbs_max, 1.f, 64.f, 1.f, "%.0f").extend(1.f, 256.f)
            .help("Past it, the oldest go first (not one in your hand) (vr_limbs_max)."),
        cycle("Make Limbs as the Map Loads", "vr_limbs_prebuild", {{0.f, "Off"}, {1.f, "The Map's"}, {2.f, "And the Range's"}})
            .help("The Map's: the limbs of every kind of monster the map has (those waiting to appear too) are made as it "
                  "loads (about 5 ms each: a tenth of a second or so more), not at their first cut (a dropped frame). And the "
                  "Range's: also every kind the firing range's dispensers and dummies can make (half a second more there, "
                  "once a session). The next map load (vr_limbs_prebuild)."),
        slider("Limb Weight", vr_limbs_mass_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A limb (or head) cut off weighs its share of its monster's ragdoll Mass by the kind of limb: a whole arm "
                  "6.3%, a forearm and hand 2.8%, a hand 0.8%, a whole leg 15.5%, a shin and foot 6%, the head 7% (four "
                  "legs share two legs' weight, six arms two arms'; a tail its share of the body's volume), times this; "
                  "held and thrown as that. 0: its model's volume of flesh, as before (vr_limbs_mass_scale)."),
        slider("Cut End Fountain", vr_limbs_end_fountain, 0.f, 4.f, 0.25f, "%.2f s").extend(0.f, 20.f)
            .help("How long the cut end of a limb (or a head's neck) spurts a smaller fountain of its own as it flies, "
                  "dying away (the stump's: Decapitation > Fountain) (0: none) (vr_limbs_end_fountain)."),
        slider("Enforcer Laser Pops", vr_decap_pop_laser, 0.f, 1.f, 0.05f, "%.2f")
            .help("An enforcer's laser bolt that kills with a head or limb hit (an enemy's into another monster, or your "
                  "enforcer's rifle) pops it at this chance (times Head or Limb Chance) (vr_decap_pop_laser)."),
        header("Explosions and Gibbing"),
        toggle("Explosions Pop Limbs", vr_limbs_blast)
            .help("An explosion pops the limbs near it by chance: a monster it kills falls as a ragdoll without them instead "
                  "of bursting into gibs (if none popped: gibbed as before); corpses lose them too. Off: as before "
                  "(vr_limbs_blast)."),
        slider("Explosion Reach", vr_limbs_blast_radius, 16.f, 256.f, 8.f, "%.0f units").extend(1.f, 1000.f)
            .help("Limbs this near the blast may pop (vr_limbs_blast_radius)."),
        slider("Explosion Chance", vr_limbs_blast_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("A limb's chance at the blast itself, falling to none at the reach (times Limb Chance) "
                  "(vr_limbs_blast_chance)."),
        cycle("Gibbed Bodies Throw Limbs", vr_gib_limbs, {{0.f, "No"}, {1.f, "With the Gibs"}, {2.f, "Instead of Gibs"}})
            .help("A body bursting into gibs throws its own arms, legs and tail as well (With the Gibs), or instead of "
                  "Quake's meat chunks (Instead of Gibs) (vr_gib_limbs)."),
    };
}

// Gore (vr_gore.cpp, vr_decals.cpp, vr_bodyblood.cpp; the QC's gibs sticking: vr_carry.qc).
[[nodiscard]] za::Vector<Item> pageGore()
{
    return {
        cycle("Gore", vr_gore, {{0.f, "Quake VR"}, {1.f, "More"}, {2.f, "Over the top"}})
            .help("Over the top: hits spray blood onto the walls, floor and ceiling behind, gibbing paints the room, pools spread under corpses, gibs stick to ceilings and drip."),
        slider("Gore Sounds", vr_gore_sound, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 2.f)
            .help("How loud the gore's own sounds are (0 none): stumps spurting with each pulse, streams splattering on the "
                  "floor, a beheaded or face-smashed enemy's choke instead of its cry, limbs tearing off, a hand squelching "
                  "into a gib or a body, wet steps after a pool of blood, gibs slapping walls. Placeholders for now "
                  "(vr_gore_sound)."),
        header("Hits, Gibs and Corpses"),
        slider("Blood Sprays", vr_gore_spray, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("How many splats a hit or a gibbing throws onto the walls, floor and ceiling round it (0 none)."),
        slider("Splat Size", vr_gore_size, 0.5f, 2.f, 0.1f, "%.1fx").extend().help("How big the gore's splats, pools and runs are."),
        slider("Blood Pools", vr_gore_pools, 0.f, 2.f, 0.1f, "%.1fx").extend().help("Pools of blood spreading under corpses and gibs: their size (0 none)."),
        slider("Dripping", vr_gore_drips, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Blood dripping from splats on the ceiling and from gibs stuck there: how long and how much (0 none)."),
        slider("Gibs Stick", vr_gore_stick, 0.f, 30.f, 1.f, "%.0f s").extend()
            .help("Gibs flung into a ceiling or a wall may stick there about this long, dripping, then fall (0 never)."),
        slider("Thrown Gibs Stick", vr_gore_stick_thrown, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a gib or head (or a small gib) you throw into a wall or a ceiling sticks there (faster than Gib Splat Speed it bursts instead)."),
        slider("Speed to Stick", vr_gore_stick_speed, 50.f, 500.f, 10.f, "%.0f u/s").extend(20.f, 1000.f)
            .help("How fast a gib must strike a wall or a ceiling to stick (flung from a body or thrown)."),
        toggle("Flies on Heads", vr_head_flies)
            .help("Flies buzzing round some severed heads (Scourge of Armagon's: about one head in ten). Off "
                  "by default."),
        slider("Gib Speed: Melee", vr_gib_speed_melee, 0.05f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("How fast the gibs fly when a melee blow gibs a monster or a corpse (a swing, a bash, a shove, a headbutt): "
                  "times Quake's speed."),
        slider("Gib Speed: Light Weapons", vr_gib_speed_light, 0.05f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("And when a thrown thing, the shotguns, nails or an enforcer's laser gib it."),
        slider("Gib Speed: Explosives", vr_gib_speed_heavy, 0.05f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("And everything else: explosions, rockets, grenades, lightning, plasma, monsters' blows (1: Quake's)."),
        header("Blood Streams, Back-splatter and Teeth"),
        toggle("Blood Streams", vr_gore_streams)
            .help("Shots and nails open bullet holes that pour a thin stream of blood, pulsing and weakening over a few seconds, "
                  "following the body as it moves and falls, painting a pool where it lands (vr_gore_streams)."),
        slider("Stream Chance", vr_gore_streams_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance each shot or pellet opens one (vr_gore_streams_chance)."),
        slider("Melee Blade Chance", vr_gore_streams_melee_blade, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance each blow of a blade in your hand (the axe, the sword, the chainsaw) opens one, on a monster "
                  "or a corpse (vr_gore_streams_melee_blade)."),
        slider("Melee Blunt Chance", vr_gore_streams_melee_blunt, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance each blunt blow of yours (the crowbar, a punch, a pommel, a gun, a thing in the fist) opens "
                  "one (vr_gore_streams_melee_blunt)."),
        slider("Thrown Blade Chance", vr_gore_streams_thrown_blade, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance an axe or a sword you threw opens one where it strikes (vr_gore_streams_thrown_blade)."),
        slider("Streams per Body", vr_gore_streams_per_body, 1.f, 8.f, 1.f, "%.0f")
            .help("Most on one body: a new one takes the weakest's place (vr_gore_streams_per_body)."),
        slider("Streams in All", vr_gore_streams_max, 0.f, 64.f, 2.f, "%.0f")
            .help("Most at once everywhere (vr_gore_streams_max)."),
        slider("Stream Duration", vr_gore_streams_time, 1.f, 20.f, 0.5f, "%.1f s").extend(1.f, 30.f)
            .help("About how long one pours, weakening all along (vr_gore_streams_time)."),
        slider("Stream Force", vr_gore_streams_speed, 0.f, 200.f, 5.f, "%.0f u/s").extend(0.f, 400.f)
            .help("How fast it leaves the wound at its strongest: how far it arcs out (vr_gore_streams_speed)."),
        slider("Stream Width", vr_gore_streams_width, 0.1f, 2.f, 0.05f, "%.2f units").extend(0.05f, 4.f)
            .help("How thick, at its strongest (1 unit: 3.8 cm) (vr_gore_streams_width)."),
        slider("Stream Pools", vr_gore_streams_pool, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 4.f)
            .help("The pools a stream spreads where it lands: their size (0: no marks) (vr_gore_streams_pool)."),
        slider("Stream Spread", vr_gore_streams_spread, 0.f, 90.f, 5.f, "%.0f deg")
            .help("Each stream's way out turned at random up to this far from the wound's, so one shotgun blast's "
                  "streams go their own ways (0: all alike) (vr_gore_streams_spread)."),
        slider("Stream Force Variation", vr_gore_streams_speed_var, 0.f, 0.9f, 0.05f, "+-%.2f")
            .help("Each stream's force times a random 1 plus or minus this (vr_gore_streams_speed_var)."),
        slider("Stream Wobble", vr_gore_streams_wobble, 0.f, 45.f, 1.f, "%.0f deg")
            .help("How far each stream's way out wanders as it pours: slow noise (0: steady) (vr_gore_streams_wobble)."),
        slider("Stream Wobble Rate", vr_gore_streams_wobble_rate, 0.f, 3.f, 0.05f, "%.2f Hz").extend(0.f, 10.f)
            .help("How fast it wanders: about this many swings a second (vr_gore_streams_wobble_rate)."),
        slider("Stream Thickness Variation", vr_gore_streams_thickness_var, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much its thickness swells and thins as it pours, and so along it as it falls: an uneven flow, not a "
                  "steady trickle (0: even; 1: from a thread to twice as thick) (vr_gore_streams_thickness_var)."),
        slider("Stream Thickness Rate", vr_gore_streams_thickness_rate, 0.f, 4.f, 0.1f, "%.1f Hz").extend(0.f, 10.f)
            .help("How fast it swells and thins: about this many swells a second, unevenly (vr_gore_streams_thickness_rate)."),
        slider("Stream Smoothing", vr_gore_streams_smooth, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 2.f)
            .help("The body's motion at the wound, and the stream's way out, smoothed over about this long, so the "
                  "monster's animation and turns don't fling it about in ribbons (0: none) (vr_gore_streams_smooth)."),
        slider("Stream Body Motion", vr_gore_streams_inherit, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 1.5f)
            .help("How much of the body's (smoothed) motion the stream and its spurts take: 1 as a real one, less to keep "
                  "it steady as the monster moves (vr_gore_streams_inherit)."),
        slider("Stream Break Stretch", vr_gore_streams_break, 0.f, 10.f, 0.5f, "%.1fx").extend(0.f, 100.f)
            .help("A piece of the stream stretched this many times its length as poured breaks off, fading from half that; "
                  "and it is cut where the wound jumps (0: never) (vr_gore_streams_break)."),
        toggle("Stream Spurts", vr_gore_streams_spurts)
            .help("Small drops spurting from the wound in a cone as it pours, most fading fast, a few flying further "
                  "(vr_gore_streams_spurts)."),
        slider("Spurt Rate", vr_gore_streams_spurt_rate, 0.f, 150.f, 5.f, "%.0f /s").extend(0.f, 1000.f)
            .help("Drops a second at its strongest; fewer as it weakens (vr_gore_streams_spurt_rate)."),
        slider("Spurt Cone", vr_gore_streams_spurt_cone, 0.f, 90.f, 5.f, "%.0f deg")
            .help("How far round its way out they spurt (vr_gore_streams_spurt_cone)."),
        slider("Spurt Speed", vr_gore_streams_spurt_speed, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("Their speed, times the stream's (vr_gore_streams_spurt_speed)."),
        slider("Spurt Lifetime", vr_gore_streams_spurt_life, 0.05f, 1.f, 0.05f, "%.2f s").extend(0.02f, 5.f)
            .help("About how long most live, fading all along (vr_gore_streams_spurt_life)."),
        slider("Spurt Outliers", vr_gore_streams_spurt_outliers, 0.f, 1.f, 0.02f, "%.2f")
            .help("The share of them that fly further and live longer (vr_gore_streams_spurt_outliers)."),
        slider("Spurt Outlier Lifetime", vr_gore_streams_spurt_outlier_life, 0.1f, 3.f, 0.05f, "%.2f s").extend(0.02f, 10.f)
            .help("About how long those live (vr_gore_streams_spurt_outlier_life)."),
        slider("Spurt Size", vr_gore_streams_spurt_size, 0.05f, 1.f, 0.01f, "%.2f units").extend(0.02f, 3.f)
            .help("About how big the drops are (1 unit: 3.8 cm) (vr_gore_streams_spurt_size)."),
        slider("Stream Splash", vr_gore_streams_splash, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("Drops splashing away where it lands: on the floor, a wall, a prop or a monster: how many (0 none) "
                  "(vr_gore_streams_splash)."),
        slider("Splash Size", vr_gore_streams_splash_size, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 4.f)
            .help("Their size, times the spurts' (vr_gore_streams_splash_size)."),
        slider("Back-splatter", vr_gore_backsplatter, 0.f, 3.f, 0.25f, "%.2fx").extend(0.f, 4.f)
            .help("Thin long streaks thrown along the shot's line onto what is behind a monster hit: how many (0 none) "
                  "(vr_gore_backsplatter)."),
        slider("Back-splatter Reach", vr_gore_backsplatter_reach, 0.25f, 3.f, 0.25f, "%.2fx").extend(0.f, 4.f)
            .help("How far they fly, times the hit's own reach: a shotgun's close, big hits across the room "
                  "(vr_gore_backsplatter_reach)."),
        toggle("Knocked-out Teeth", vr_teeth)
            .help("A hard punch to a humanoid's face (grunts, enforcers, knights, zombies, AD's humanoids) knocks teeth out: "
                  "tiny gibs that bounce, can be picked up, and fade (vr_teeth)."),
        slider("Teeth per Punch", vr_teeth_per_hit, 1.f, 10.f, 1.f, "%.0f")
            .help("Most from one blow, the hardest (fewer for softer ones) (vr_teeth_per_hit)."),
        slider("Teeth per Enemy", vr_teeth_per_enemy, 1.f, 32.f, 1.f, "%.0f")
            .help("Most one enemy loses (vr_teeth_per_enemy)."),
        slider("Punch Hardness for Teeth", vr_teeth_hardness, 0.f, 0.95f, 0.05f, "%.2f")
            .help("How hard a blow must be to knock any out (0..1: the hand's speed and the damage, as Decapitation's "
                  "blunt pops) (vr_teeth_hardness)."),
        toggle("Teeth from Blunt Weapons", vr_teeth_blunt)
            .help("A gun's butt, a crowbar, a club or a pommel knocks them out too, not only fists (vr_teeth_blunt)."),
        slider("Tooth Size", vr_teeth_size, 1.f, 8.f, 0.5f, "%.1f cm")
            .help("How long a tooth is (vr_teeth_size)."),
        open("Small Gibs", pageIndex(pageSmallGibs)).help("Chunks of meat torn out by hits: when, how many, how they fly, how long they last."),
        open("Decapitation", pageIndex(pageDecapitation))
            .help("A killing slash at a monster's head cuts it off: the head flies, the body falls headless, the neck spurts blood."),
        open("Limb Gore", pageIndex(pageLimbGore))
            .help("Arms, legs and tails cut off or popped as heads are: the killing blow on the living, any blow on corpses."),
        header("Wounds on Models"),
        toggle("Dynamic Wounds", vr_wounds)
            .help("Blood painted on monsters, corpses and you where the hits land, in the skins' own pixels. Your body and hands show your wounds this way instead of the wound skins, and healing washes them off."),
        toggle("Burns", vr_wounds_burns).help("Explosions, fire, lightning, lava and slime char what they hit; fresh burns glow in their cracks for a moment."),
        toggle("Wet from Liquids", vr_wounds_wet).help("Monsters and you get wet up to where water or slime came, drip, and dry (Wet Duration)."),
        slider("Wet Sheen", vr_wounds_wet_coat, 0.f, 2.f, 0.1f, "%.1fx")
            .help("How strongly wet skin, gloves and weapons look: a little darker and glossy, all over where the liquid came, fading as they dry (0: no wet look, the drips stay)."),
        slider("Wet Duration", vr_wounds_wet_time, 5.f, 120.f, 1.f, "%.0f s").extend(2.f, 600.f)
            .help("How long it takes to dry after leaving the liquid: the wet look stays full for the first quarter of it, then fades out, and the drips stop."),
        cycle("Models Kept", vr_wounds_pool, {{32.f, "32 (8 MB)"}, {64.f, "64 (16 MB)"}, {128.f, "128 (32 MB)"}})
            .help("How many models keep their wounds at once: past it, the ones seen longest ago give theirs up."),
        cycle("Your Wounds' Detail", vr_wounds_own_res,
            {{0.f, "Chunky (as monsters)"}, {512.f, "Fine (5 MB)"}, {1024.f, "Finer (20 MB)"}, {2048.f, "Finest (80 MB)"}})
            .help("Your own body's and hands' wounds, burns and wetness: finer than their skin and smooth-edged, with a relief "
                  "(Chunky: in the skin's own pixels, as the monsters')."),
        cycle("Body Wounds' Detail", vr_wounds_body_res,
            {{0.f, "As Hands"}, {512.f, "Fine"}, {1024.f, "Match Hands"}, {2048.f, "Finest"}})
            .help("A minimum for your body's blood, burns and wetness. The long arm texture needs more rows than the hands: "
                  "1024 gives arms about the same blood detail per metre as Chunky hands. Shares the finer mask array "
                  "(1024: 20 MB, 2048: 80 MB); the hands keep Your Wounds' Detail. As Hands uses that setting alone."),
        slider("Your Burns' Relief", vr_wounds_bump_burns, 0.f, 3.f, 0.25f, "%.2fx")
            .help("How crusted and cracked the char on your arms looks in the light (Finer detail only; 0 flat)."),
        slider("Your Wounds' Depth", vr_wounds_bump_blood, 0.f, 3.f, 0.25f, "%.2fx")
            .help("How deep your bleeding wounds look sunk into the skin (Finer detail only; 0 flat)."),
        slider("Blood Opacity", vr_wounds_blood_alpha, 0.2f, 1.f, 0.05f, "%.2f")
            .help("How opaque the painted blood is over the skins: a little of the skin shows through below 1."),
        toggle("Flesh Dents", vr_gore_dents)
            .help("Bullets, pellets, nails, blades and blasts push monsters' and corpses' surfaces in where they strike: a crater "
                  "of raw flesh with a bloody rim, kept through every animation and on ragdolls (with Dynamic Wounds or Burns on)."),
        slider("Dent Depth", vr_gore_dents_depth, 0.f, 3.f, 0.25f, "%.2fx")
            .help("How deep the dents push in (0: flat, the raw flesh only). Never through a thin part: an arm, a tail."),
        slider("Dent Size", vr_gore_dents_size, 0.25f, 3.f, 0.25f, "%.2fx").help("How wide the dents are (and as much deeper)."),
        slider("Dents per Body", vr_gore_dents_max, 1.f, 16.f, 1.f, "%.0f")
            .help("How many dents a body keeps: past it, the oldest merges into the one nearest it."),
        header("Lightning Shock"),
        toggle("Lightning Shock", vr_shock_death)
            .help("A monster the lightning strikes, alive or dead, keeps Quad Damage's arcs crawling over it a while, on through its "
                  "death; its ragdoll convulses as long as they do and the killing bolt chars it (off: only each hit's own arcs and burn)."),
        toggle("Arcs on the Living", vr_shock_living)
            .help("The lasting arcs on living monsters too, from the first bolt and on through their death. Off: only on the bodies "
                  "the lightning kills or strikes dead; a living monster gets just each hit's own short flicker of arcs."),
        slider("Lightning Shock Duration", vr_shock_death_time, 0.5f, 30.f, 0.5f, "%.1f s").extend(0.1f, 60.f)
            .help("How long the arcs (and a body's convulsions) last after the last bolt."),
        slider("On the Living", vr_shock_living_time, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 2.f)
            .help("A living monster's arcs last this much of the Duration after the last bolt (0.5: half). Its death carries "
                  "them on to the whole Duration from that bolt, as a body's (vr_shock_living_time)."),
        slider("Convulsions", vr_shock_seizure, 0.f, 3.f, 0.1f, "%.1fx")
            .help("How hard a shocked ragdoll's limbs convulse, easing off with the arcs (0 still). Only the dead convulse."),
        slider("Crackle Volume", vr_shock_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("The electric crackle a body makes as its lasting arcs start, and again while a bolt stays on it (0 none)."),
        slider("Arcs on Bodies", vr_shock_arcs, 0.f, 3.f, 0.1f, "%.1fx").help("How many arcs crawl over a shocked monster or body (0 none)."),
        slider("Burn Marks", vr_shock_burns, 0.f, 16.f, 1.f, "%.0f").help("The burn marks a lightning kill leaves over the body (each hit also chars where it strikes: Burns)."),
        slider("Smoke After Lightning", vr_smoulder_time, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("How long a monster or a body the lightning strikes smokes from its burns after the last bolt, thinning out (0 none)."),
        slider("Smouldering Smoke", vr_smoulder, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much smoke rises off bodies the lightning struck or fire burnt (0 none; how long after fire: Combat > Burning > Smoke After Flames)."),
        slider("Smoke Opacity", vr_smoulder_alpha, 0.1f, 1.f, 0.05f, "%.2f").help("How opaque each wisp of that smoke is as it leaves the skin."),
        slider("Arcs on You", vr_shock_self_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 5.f)
            .help("Struck by lightning (a shambler's bolt, another's lightning gun, a trap, the water's shock), Quad's arcs crawl over "
                  "your hands, arms and body this long after a shambler's bolt, longer for harder ones (0 none). None right in front "
                  "of your eyes."),
        slider("Arcs on You: Number", vr_shock_self_arcs, 0.f, 3.f, 0.1f, "%.1fx").help("How many arcs crawl over you then."),
        slider("Arcs on You: Light", vr_shock_self_light, 0.f, 1.f, 0.1f, "%.1f")
            .help("A soft flicker of blue light round you while they crackle (0 none). There is no flash over the view."),
        header("Your Wounds"),
        slider("Arm Drip Rate", vr_body_blood, 0.f, 4.f, 0.25f, "%.2fx").extend()
            .help("How often blood drips from your wounded arms and hands (the body's wounds: Show Armour and Wounds; 0 none)."),
        slider("Drop Size", vr_body_blood_amount, 0.5f, 3.f, 0.25f, "%.2fx").extend().help("How big the drops are and how much they splash."),
        slider("Drips Round Feet", vr_body_blood_floor, 0.f, 4.f, 0.25f, "%.2fx").extend()
            .help("While wounded, blood drips from your body round your feet, faster when badly hurt or just hit (0 none)."),
        slider("Drops Mark Floor", vr_body_blood_marks, 0.f, 1.f, 0.05f, "%.2f").help("The chance a drop leaves a mark on the floor."),
        slider("Floor Mark Size", vr_body_blood_mark_size, 0.5f, 4.f, 0.25f, "%.2fx").extend(),
        header("Bloody Hands and Washing"),
        slider("Gib Blood on Hands", vr_gore_hands, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Taking a gib or a head bloodies the hand holding it, more the longer you hold it (0 none)."),
        toggle("Water Washes Blood", vr_gore_wash).help("Water washes the blood off your body and hands, where they are under it."),
        slider("Wash Time", vr_gore_wash_time, 0.25f, 10.f, 0.25f, "%.2f s").extend(0.05f, 60.f)
            .help("How long under water washes the blood off fully."),
        toggle("Wounds Re-open", vr_gore_reopen)
            .help("Hurt, your wounds re-open after a wash and bleed onto your arms and hands again (healed ones don't, nor a gib's blood)."),
        slider("Re-open Delay", vr_gore_reopen_delay, 0.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long you stay clean out of the water before they re-open."),
        slider("Re-open Spread", vr_gore_reopen_time, 0.f, 10.f, 0.25f, "%.2f s").extend()
            .help("Over how long they re-open, one after another (0: all at once)."),
        slider("Re-open Bleed", vr_gore_reopen_fade, 0.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long each re-opened wound takes to bleed back to its full blood, from a trace; the blood runs onto your arms and hands along the way (0: at once)."),
        header("Blood on You and Your Gear"),
        slider("Blood Spatter", vr_gore_spatter, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Blood thrown from hits near you onto your hands, arms, body and what you hold, as drops: how much (0 none)."),
        slider("From Your Blows", vr_gore_spatter_melee, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("A punch, a blade, a prop or a gun swung into a monster bloodies what struck and the hand a little."),
        slider("From the Chainsaw", vr_gore_spatter_saw, 0.f, 6.f, 0.25f, "%.2fx").extend()
            .help("A chainsaw cutting a monster sprays your hands and arms: a lot."),
        slider("From Close Shots", vr_gore_spatter_shots, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("A shot hitting close to you throws a few drops onto your weapon, hands and arms."),
        slider("Close Shot Range", vr_gore_spatter_range, 16.f, 160.f, 8.f, "%.0f u").extend(0.f, 512.f)
            .help("How close to your hands or eyes a shot must hit for its blood to reach you."),
        slider("Gibs Striking You", vr_gore_spatter_gibs, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Gibs flying into you bloody you where they strike: how much (0 none)."),
        toggle("Blood on Weapons and Props", vr_gore_gear)
            .help("What you hold takes blood and keeps it: dropped, thrown and taken again, holstered and drawn, until water washes it."),
        toggle("Clean Weapon Skins", vr_gore_clean_skins)
            .help("Weapons with blood painted into their skins (the axe, the knights' swords, the chainsaw, the grunt's shotgun) start clean: their blood is all yours, and water washes them clean."),
        toggle("Holstered Weapons Too", vr_gore_gear_holstered)
            .help("Your holstered weapons take blood as your body and hands do, when it lands near enough to them (a holstered weapon keeps its blood either way)."),
        slider("Things Lying Near", vr_gore_gear_nearby, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Weapons, boxes and props lying about take blood from bleeding hits, gibbings and bursting gibs near them: how much (0 none)."),
        slider("Gibbed Monsters' Drops", vr_gore_gear_drops, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("What lies right where a monster is gibbed (its gun, its backpack) comes out soaked: how much (0 only the drops near it)."),
        slider("Blood over Your Arms", vr_gore_spread, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Hurt, holding a gib, or as your wounds re-open, blood runs over your arms too (a little your legs): how much (0 none)."),
        header("Blood Mist"),
        slider("Mist Amount", vr_gore_mist, 0.f, 3.f, 0.25f, "%.2fx").extend(0.f, 10.f)
            .help("Large, faint clouds of blood hanging in the air wherever something bleeds: how many (0 none)."),
        slider("Mist Size", vr_gore_mist_size, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.05f, 6.f).help("How big the clouds are."),
        slider("Mist Opacity", vr_gore_mist_alpha, 0.f, 0.3f, 0.01f, "%.2f").extend(0.f, 1.f).help("How opaque they start (very transparent: 0.06)."),
        slider("Mist Lifetime", vr_gore_mist_life, 0.25f, 8.f, 0.25f, "%.2f s").extend(0.2f, 30.f).help("How long they take to fade."),
        slider("Mist Spreading", vr_gore_mist_grow, 0.f, 3.f, 0.1f, "%.1fx").extend().help("How fast they spread as they fade."),
        slider("Mist Drift", vr_gore_mist_speed, 0.f, 3.f, 0.1f, "%.1fx").extend().help("How fast they drift along the blow and apart."),
        slider("Mist Rise", vr_gore_mist_rise, -20.f, 20.f, 1.f, "%.0f u/s").extend(-100.f, 100.f).help("How fast they rise (negative: sink)."),
        slider("Mist Darkness", vr_gore_mist_dark, 0.f, 1.f, 0.05f, "%.2f").help("How dark their red is (0 bright, 1 nearly black)."),
        slider("Mist Clear of Eyes", vr_gore_mist_near, 0.f, 128.f, 4.f, "%.0f u").extend(0.f, 512.f)
            .help("No mist closer to your eyes than this: your own bleeding is not in your face."),
        header("Dying Bodies"),
        toggle("Hit While Dying", vr_corpse_dying)
            .help("A monster dying takes damage as a corpse while it falls (once it drops what it drops): a chainsaw, a blow or a shot can gib it. Never killed twice. Needs Gib Corpses (Carrying and Throwing > Gibs and Corpses)."),
        header("Training Dummy"),
        cycle("Dummy Enemy", vr_dummy_type, dummyEnemies())
            .help("The enemy the training dummy stands as: its model, size, hit zones, the head it loses, its blood, gore and death are that monster's own. It stays still, takes no harm and reports every hit. The mission packs' with the pack installed."),
        toggle("Dummy Bleeds", vr_dummy_gore)
            .help("The firing range's training dummy bleeds as its enemy: blood sprays and mist, wounds on its model, small gibs, blood on you and what you hold. Off: it stays clean."),
        toggle("Dummy Dents", vr_dummy_dents)
            .help("A clean training dummy (Dummy Bleeds off) is dented where it is hit all the same, with nothing else painted: for trying the dents. One that bleeds is dented as its enemy anyway (vr_dummy_dents)."),
        toggle("Dummy Blood Streams", vr_dummy_streams)
            .help("A clean training dummy pours blood streams from its bullet holes all the same (at Stream Chance): for trying the streams. One that bleeds pours them as its enemy anyway (vr_dummy_streams)."),
        command("Clear the Dummy's Dents and Streams", "vr_dummy_gore_reset")
            .help("vr_dummy_gore_reset: every training dummy's dents, blood streams and painted wounds gone at once."),
        toggle("Dummy Dies", vr_dummy_gib)
            .help("The hit that takes the last of its health (Dummy Health) kills the training dummy as its enemy: beheaded, its head popped, dying, lying as a ragdoll or gibbed exactly as that monster would be. Then it stands again. No loot. Off: it stays at 0 health (\"would kill\"). On as shipped."),
        slider("Dummy Stands Again", vr_dummy_gib_respawn, 0.5f, 10.f, 0.5f, "%.1f s").extend(0.1f, 60.f)
            .help("How long a killed training dummy takes to stand again (its body stays, as its enemy's)."),
        dummyHealthSlider(),
        toggle("Dummy Stays Put", vr_dummy_anchored)
            .help("The training dummy is never shoved away, staggered or knocked down; it still flinches and takes the hit. For recording shoves and bashes (vr_dummy_anchored)."),
        slider("Dummy Health Refills", vr_dummy_regen, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long after its last hit the training dummy's health fills up again (0: never; it stays as hit until it dies and stands again)."),
        toggle("Dummy Health Bar", vr_dummy_healthbar)
            .help("The training dummy's health over its head, as a bar and a number, turned to face you. Off: its sign only."),
        header("Marks"),
        toggle("Decals", vr_decals).help("Blood, scorch marks and bullet chips on walls and floors (the gore needs them)."),
        toggle("Permanent Decals", vr_permdecals)
            .help("Marks stay for good, baked into the walls, floors, doors and lifts they land on: as many as you like at no extra cost a frame. Off: only the newest Max Decals show, each for its Decal Lifetime, as without it; the others are kept and come back when it is on again."),
        toggle("Marks on Moving Brushes", vr_decal_entities)
            .help("Shots, blows and blood mark the brush things with models of their own (the firing range's targets: its spawn buttons and panels), and doors and lifts with Permanent Decals off: moving with them, the newest 128, flat (vr_decal_entities)."),
        slider("Max Decals", vr_decal_max, 64.f, 4096.f, 64.f, "%.0f").extend().help("The oldest go first. The gore makes many; more cost GPU time where they pile up (512 shipped)."),
        slider("Decals Piled in One Spot", vr_decal_bucket_max, 8.f, 255.f, 8.f, "%.0f")
            .help("How many marks one spot of the floor or wall keeps where they pile up (a corpse cut apart in one place). Past it the oldest there go whole, never leaving holes in the others. Each pixel there walks all of them: more cost GPU time when you look at the pile (128 shipped, 255 the most)."),
        slider("Decal Lifetime", vr_decal_life, 10.f, 600.f, 10.f, "%.0f s").extend(),
        toggle("Carved Holes and Craters", vr_decal_carve)
            .help("Bullet, nail and blast marks carved into walls and floors: a pit, hole or crater with a raised rim and a dark middle, its texture seen deeper in and lit by the world's light. Pellets leave small pits, nails narrow deeper holes (the super nailgun's and lava nails' larger), rockets and grenades wide shallow craters, the lightning gun a scorch only. Look only: nothing to bump into."),
        slider("Carved Depth", vr_decal_carve_depth, 0.25f, 4.f, 0.25f, "%.2fx").extend(0.f, 4.f)
            .help("How deep the holes and craters look (1 shipped: shallow)."),
        slider("Carved Size", vr_decal_carve_size, 0.5f, 2.f, 0.1f, "%.1fx")
            .help("How wide the holes and craters made from now on are (1 shipped)."),
        slider("Carved Rim", vr_decal_carve_rim, 0.f, 3.f, 0.25f, "%.2fx").extend(0.f, 4.f)
            .help("How high the broken rim round each hole and crater stands (0 none)."),
        slider("Carving Quality", vr_decal_carve_steps, 4.f, 64.f, 4.f, "%.0f steps")
            .help("The most steps a pixel's look into a hole takes (fewer where it is small on the screen): more are smoother at grazing angles and cost more GPU time where many are in view (16 shipped)."),
        slider("Carving Distance", vr_decal_carve_fade, 128.f, 4096.f, 128.f, "%.0f")
            .help("How far off holes and craters stop looking carved (they flatten from half as far; 1024 shipped)."),
        slider("Carved Marks Kept", vr_decal_carve_max, 32.f, 1024.f, 32.f, "%.0f")
            .help("How many of the newest holes and craters stay carved; with Permanent Decals the older ones stay, flat (256 shipped)."),
        cycle("Carve Style", vr_decal_carve_style, {{-1.f, "Auto"}, {0.f, "Smooth"}, {1.f, "Retro"}, {2.f, "Faceted"}})
            .help("How holes, craters, dents and grooves are carved. Smooth: round and smooth. Retro: pixelated like the walls, block by block on the wall's own texels, their depth in steps. Faceted: low-poly, flat sides (8 round, 3 rings out). Auto, as shipped: Retro on walls with Retro Textures' blocks, else Smooth."),
        toggle("Melee Marks", vr_decal_melee)
            .help("Melee blows on walls and floors carve their own marks (needs Carved Holes and Craters): blunt ones (the crowbar, a gun's butt or barrel, a pommel, Mjolnir, a wall torch, a fist holding a prop, a prop thrown hard) leave shallow round dents, blades (a sword, an axe, the chainsaw's bar) grooves along the swing, deepest in the middle; harder blows bigger and deeper. A bare fist leaves none. Off: the old shallow pits."),
        slider("Blunt Dent Depth", vr_decal_melee_blunt_depth, 0.25f, 4.f, 0.25f, "%.2fx").extend(0.f, 4.f)
            .help("How deep blunt blows' dents look (1 shipped: shallower than a pellet's pit; 0 a scuff only)."),
        slider("Blunt Dent Size", vr_decal_melee_blunt_size, 0.5f, 2.f, 0.1f, "%.1fx")
            .help("How wide the dents made from now on are (1 shipped)."),
        slider("Slash Depth", vr_decal_melee_slash_depth, 0.25f, 4.f, 0.25f, "%.2fx").extend(0.f, 4.f)
            .help("How deep blades' grooves look (1 shipped; 0 a scuff only)."),
        slider("Slash Length", vr_decal_melee_slash_length, 0.5f, 2.f, 0.1f, "%.1fx")
            .help("How long the grooves made from now on are (1 shipped)."),
        slider("Slash Width", vr_decal_melee_slash_width, 0.5f, 2.f, 0.1f, "%.1fx")
            .help("How wide the grooves made from now on are (1 shipped)."),
        toggle("Gib Blood", vr_gib_blood).help("Gibs and heads leave a trail of blood drops and splat where they hit walls and floors. Off: Quake's trail."),
        slider("Gib Blood Trail", vr_gib_blood_trail, 0.f, 3.f, 0.25f, "%.2fx").extend().help("How dense their trail of blood and drops is (0 none)."),
    };
}
