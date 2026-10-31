// vr_menu_gameplay.inc -- VR Settings > Gameplay: enemies (shoves, melee, holding, knockdowns, weapons), damage, sound,
// blood feedback, parry, stamina, batting, the Survival Arena's waves. Part of vr_menu.cpp (included where it was: the
// same translation unit, so its anonymous namespace).

[[nodiscard]] za::Vector<Item> pageGameplay()
{
    return {
        header("Monsters"),
        row(opt::vr_enemy_liquid_damage),
        row(opt::vr_monster_lerp_continue),
        row(opt::vr_ogre_aim_height),
        header("Enemy Weapons"),
        open("Enemy Weapons", pageIndex(pageEnemyWeapons))
            .help("What monsters drop as they die, to pick up and use: knights' swords, ogres' chainsaws, grunts' shotguns, "
                  "enforcers' laser rifles. Their damage and ammo."),
        header("Weapon Drops"),
        row(opt::vr_enemy_drops),
        row(opt::vr_ammobox_drops),
        header("Feel"),
        row(opt::vr_explosion_rumble),
        row(opt::vr_heartbeat),
    };
}

// Grunts and enforcers shove you away when you stand too close (QC vr_enemyshove.qc; ROUND21.md, "Grunts and enforcers
// shove you").
[[nodiscard]] za::Vector<Item> pageEnemyShoves()
{
    return {
        row(opt::vr_enemy_shove),
        row(opt::vr_enemy_shove_range),
        row(opt::vr_enemy_shove_delay),
        row(opt::vr_enemy_shove_cooldown),
        row(opt::vr_enemy_shove_damage),
        row(opt::vr_enemy_shove_distance),
        row(opt::vr_agrunt_shove),
        row(opt::vr_enemy_shove_vore),
    };
}

// Enemy melee (QC fight.qc; ROUND22.md, "Melee arc", "Melee space", "Enemy Melee menu"): the monsters' melee blows at
// you: where they land (the arc ahead of the monster, its reach, both growing with its size and kind), how fast it turns
// after you while swinging, how close it comes.
[[nodiscard]] za::Vector<Item> pageEnemyMelee()
{
    return {
        row(opt::vr_enemy_melee_arc),
        row(opt::vr_enemy_melee_arc_show),
        row(opt::vr_enemy_aim_show),
        header("Arc"),
        row(opt::vr_enemy_melee_kind),
        header("Swinging"),
        row(opt::vr_enemy_melee_space),
        header("Ranged Aim"),
        row(opt::vr_enemy_aim_cone),
        row(opt::vr_enemy_aim_narrow),
        row(opt::vr_enemy_aim_wide),
    };
}

// Struggling in the hands (vr_ragstruggle.cpp, vr_box3d_struggle.cpp; docs/vr-port/STRUGGLE.md): Combat > Holding
// Enemies > Struggle.
[[nodiscard]] za::Vector<Item> pageHoldingStruggle()
{
    return {
        row(opt::vr_struggle_strength),
        row(opt::vr_struggle_speed),
        open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Struggle)>))
            .help("Which monster groups struggle (Combat > Monster Groups: each group's page has its Struggle Strength, "
                  "Speed, Arm Drive and Weaker When Hurt)."),
        header("Behaviours"),
        row(opt::vr_struggle_swat),
        row(opt::vr_struggle_flail),
        row(opt::vr_struggle_frantic),
        row(opt::vr_struggle_squirm),
        row(opt::vr_struggle_look),
        row(opt::vr_struggle_look_strength),
        row(opt::vr_struggle_upright),
        row(opt::vr_struggle_arm_reach),
        header("Character"),
        row(opt::vr_struggle_variation),
        row(opt::vr_struggle_bursts),
        row(opt::vr_struggle_fatigue),
        row(opt::vr_struggle_tired),
        row(opt::vr_struggle_blend),
        header("The Hold"),
        row(opt::vr_struggle_held_share),
        row(opt::vr_struggle_debug),
    };
}

// Holding enemies (vr_foegrab.cpp, QC vr_foegrab.qc; ROUND21.md, "Holding enemies"): an empty hand holds on to a living
// monster, slows it and drags it a little.
[[nodiscard]] za::Vector<Item> pageHoldingEnemies()
{
    return {
        row(opt::vr_foegrab),
        row(opt::vr_foegrab_leniency),
        row(opt::vr_foegrab_break),
        header("Hold"),
        row(opt::vr_foegrab_by_mass),
        header("Effect"),
        row(opt::vr_foegrab_slow),
        row(opt::vr_foegrab_follow),
        row(opt::vr_foegrab_drag),
        row(opt::vr_foegrab_drag_onehand),
        header("Their Shoves"),
        row(opt::vr_foegrab_shove),
        header("Getting Close"),
        row(opt::vr_body_monster_inset),
        row(opt::vr_foegrab_step_in),
        row(opt::vr_foegrab_step_in_depth),
        header("Two-Hand Throw"),
        row(opt::vr_foegrab_throw),
        row(opt::vr_foegrab_throw_debug),
        row(opt::vr_foegrab_throw_away),
        row(opt::vr_foegrab_throw_by_mass),
        open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Throw)>))
            .help("Which groups are always thrown, only when hurt, or never (Combat > Monster Groups)."),
        row(opt::vr_foegrab_throw_hold),
        row(opt::vr_foegrab_throw_grace),
        cycle("Throw Grunt Sound", vr_foegrab_throw_grunt_sound, gruntChoices())
            .help("Which grunt the throw makes: the same choices as climbing's Mantle Grunt Sound. The deeper ones are "
                  "Quake's played slower. Hear it with Hear Throw Grunt."),
        command("Hear Throw Grunt", "vr_foegrab_throw_grunt_test").help("Plays the throw's grunt as set, at full volume."),
        open("Struggle", pageIndex(pageHoldingStruggle))
            .help("How a knocked-down humanoid you hold fights your hold: its moves, their strength, its fits and fatigue."),
        header("Lift"),
        row(opt::vr_foegrab_lift),
        row(opt::vr_foegrab_lift_debug),
        row(opt::vr_foegrab_lift_speed),
        row(opt::vr_foegrab_lift_reach),
        row(opt::vr_foegrab_lift_time),
        row(opt::vr_foegrab_free_sound),
        row(opt::vr_foegrab_manhandle_stagger),
        row(opt::vr_foegrab_manhandle_health),
        header("Leg Grabs"),
        row(opt::vr_foegrab_leggrab),
        row(opt::vr_foegrab_leggrab_debug),
        row(opt::vr_foegrab_leggrab_thigh),
        row(opt::vr_foegrab_legpull_onehand),
        header("Neck Snap"),
        row(opt::vr_foegrab_necksnap),
        row(opt::vr_foegrab_necksnap_debug),
        row(opt::vr_foegrab_necksnap_angle),
        row(opt::vr_foegrab_necksnap_window),
        row(opt::vr_foegrab_necksnap_margin),
        row(opt::vr_foegrab_necksnap_neck),
        row(opt::vr_foegrab_necksnap_loose),
    };
}

// Ragdoll Triggers (QC vr_kdtrig.qc; ROUND22.md, "The flinch removed; ragdoll triggers"): Combat > Ragdoll Triggers, a page per
// trigger and Monster Resistance; the shove's, the leg sweep's, the leg pull's and the lift's on their own pages.
[[nodiscard]] za::Vector<Item> pageKdTrigLedge()
{
    return {
        row(opt::vr_kdtrig_ledge_on),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigStairs()
{
    return {
        row(opt::vr_kdtrig_stairs_on),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigSlope()
{
    return {
        row(opt::vr_kdtrig_slope_on),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigShotgun()
{
    return {
        row(opt::vr_kdtrig_shotgun_on),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigWounded()
{
    return {
        row(opt::vr_kdtrig_wounded_on),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigBlow()
{
    return {
        row(opt::vr_kdtrig_blow_on),
    };
}

[[nodiscard]] za::Vector<Item> pageKdTrigBlast()
{
    return {
        row(opt::vr_kdtrig_blast_on),
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
        row(opt::vr_kdtrig_thrown_on),
    };
    kdTrigWeightRows(items);
    items.pushBack(open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Thrown)>))
                       .help("Thrown Bodies' chance times each group's share."));
    return items;
}

[[nodiscard]] za::Vector<Item> pageKdTrigTackle()
{
    za::Vector<Item> items{
        row(opt::vr_kdtrig_tackle_on),
        row(opt::vr_kdtrig_tackle),
        row(opt::vr_kdtrig_tackle_runup),
        row(opt::vr_kdtrig_tackle_feel),
    };
    kdTrigWeightRows(items);
    items.pushBack(open("Each Monster Group", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Tackle)>))
                       .help("Shoulder Charges' chance times each group's share."));
    return items;
}

[[nodiscard]] za::Vector<Item> pageKdTriggers()
{
    return {
        row(opt::vr_kdtrig),
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
        row(opt::vr_knockdown),
        header("Chance"),
        row(opt::vr_knockdown_chance),
        row(opt::vr_knockdown_onehand),
        row(opt::vr_knockdown_twohand),
        row(opt::vr_knockdown_surprise),
        row(opt::vr_knockdown_ledge),
        header("Hits Unbalance"),
        row(opt::vr_hit_stagger),
        header("Impacts"),
        row(opt::vr_ragimpact),
        row(opt::vr_ragimpact_crush_speed),
        row(opt::vr_ragimpact_crush_push),
        row(opt::vr_ragimpact_crush_standing),
        row(opt::vr_ragimpact_crush_cushion),
        row(opt::vr_ragimpact_stomp),
        header("Leg Sweep"),
        row(opt::vr_legsweep),
        header("Each Monster's Chance"),
        open("Each Monster Group's Chance", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Shove)>))
            .help("A shove's base chance to knock each monster group down (grunts 0.45, shamblers 0.03; Combat > Monster "
                  "Groups)."),
        open("Each Monster Group's Push", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Push)>))
            .help("How far shoves, bashes and unbalancing hits move each monster group, times (Combat > Monster Groups)."),
        open("Each Monster Group's Hits Unbalance", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Stagger)>))
            .help("Each monster group's share of Hits Unbalance's chance (Combat > Monster Groups)."),
        header("Down"),
        row(opt::vr_knockdown_time_min),
        row(opt::vr_knockdown_time_max),
        row(opt::vr_knockdown_hit_time),
        row(opt::vr_knockdown_keep_time),
        row(opt::vr_knockdown_keep_max),
        row(opt::vr_knockdown_push),
        row(opt::vr_knockdown_shove_travel),
        row(opt::vr_knockdown_shove_topple_angle),
        row(opt::vr_knockdown_shove_topple_time),
        row(opt::vr_knockdown_shove_feet_lag),
        row(opt::vr_knockdown_shove_max_spin),
        row(opt::vr_knockdown_shove_ledge_topple),
        row(opt::vr_knockdown_shove_ledge_push),
        row(opt::vr_knockdown_shove_ledge_over),
        row(opt::vr_knockdown_weld),
        header("Getting Up"),
        row(opt::vr_knockdown_wiggle),
        row(opt::vr_knockdown_wiggle_frequency),
        row(opt::vr_knockdown_retry),
        row(opt::vr_knockdown_getup_slope),
        row(opt::vr_knockdown_getup_force),
        row(opt::vr_knockdown_blend),
        row(opt::vr_knockdown_getup_speed),
        header("Debug"),
        row(opt::vr_knockdown_debug),
    };
}

// A running chainsaw's engine (NOTES.md vrfiringrange_2026-10-01_16-37-54): its exhaust's smoke, its shake in the hand
// and lying on the ground (vr_chainsaw.cpp, QC vr_chainsaw.qc VR_Saw_PropEngine).
[[nodiscard]] za::Vector<Item> pageChainsawEngine()
{
    return {
        header("Pulling the Cord"),
        row(opt::vr_chainsaw_model_handle),
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
        row(opt::vr_agrunt_freeze),
        row(opt::vr_agrunt_grip_hand),
        row(opt::vr_agrunt_grip_scope),
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
        row(opt::vr_agarmor_rule),
        row(opt::vr_agarmor_helmet_head),
        row(opt::vr_agarmor_block_cuts),
        row(opt::vr_agarmor_debris_time),
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
        row(opt::vr_worn_backpacks),
        row(opt::vr_worn_backpack_pull),
        row(opt::vr_worn_backpack_lift_force),
        header("Tearing Off"),
        row(opt::vr_worn_backpack_rip_force),
        row(opt::vr_worn_backpack_pool),
        row(opt::vr_worn_backpack_pool_blast),
        row(opt::vr_worn_backpack_pool_fire),
        row(opt::vr_worn_backpack_hit_roll),
        row(opt::vr_worn_backpack_hit_ref),
        header("Tearing It Off a Living One"),
        row(opt::vr_worn_backpack_tug),
        row(opt::vr_worn_backpack_tug_stretch),
        row(opt::vr_worn_backpack_tug_time),
        row(opt::vr_worn_backpack_two_hand_mult),
        row(opt::vr_worn_backpack_topple),
        row(opt::vr_worn_backpack_topple_push),
        row(opt::vr_worn_backpack_impact),
        row(opt::vr_worn_backpack_reserve),
        row(opt::vr_worn_backpack_volume),
        header("Fit"),
        row(opt::vr_worn_backpack_scale),
        row(opt::vr_worn_backpack_x),
        row(opt::vr_worn_backpack_y),
        row(opt::vr_worn_backpack_z),
        row(opt::vr_worn_backpack_pitch),
        row(opt::vr_worn_backpack_yaw),
        row(opt::vr_worn_backpack_roll),
        row(opt::vr_worn_backpack_mass),
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
        row(opt::vr_agrunt_repl),
        row(opt::vr_agknight),
        row(opt::vr_agdknight),
        row(opt::vr_agknight_ranged_pickup),
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
        row(opt::vr_agrunt_repl_rare_map),
        chance("Dual Wielding", vr_agrunt_repl_dual,
            "The chance of a second weapon that pairs with the first: two guns not too big, two one-handed blades "
            "(vr_agrunt_repl_dual)."),
        chance("Gun and Axe", vr_agrunt_repl_gunaxe,
            "The chance of a gun in the right hand and an axe in the left: it fires the gun, takes to the axe when "
            "you get close or its gun runs dry, back to the gun when you back off (vr_agrunt_repl_gunaxe)."),
        header("Difficulty"),
        row(opt::vr_agrunt_repl_seed),
        header("Where"),
        row(opt::vr_agrunt_repl_id),
        row(opt::vr_agrunt_repl_mp),
        row(opt::vr_agrunt_repl_ad),
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
        row(opt::vr_agrunt_ai_skill),
        header("Fighting"),
        knob("Aggressiveness", vr_agrunt_ai_aggression,
            "How readily they attack and press you: their chance to attack whenever they can, times this, and their "
            "best and furthest fighting distances nearer as it rises (times 2 / (1 + this): at 2 two-thirds as far, "
            "at 0.5 a third further). Under 1 they hold back and fire less (vr_agrunt_ai_aggression)."),
        knob("Accuracy", vr_agrunt_ai_accuracy,
            "Their aim error and their guns' spread divided by this: 2 half as much, 0.5 twice. An enforcer's own "
            "laser (no error of its own) strays under 1 (vr_agrunt_ai_accuracy)."),
        row(opt::vr_agrunt_aim_error),
        row(opt::vr_agrunt_ai_reaction),
        knob("Fire Rate", vr_agrunt_ai_firerate,
            "Their pause after an attack (a shot or a burst, a swing) divided by this: 2 half the pause "
            "(vr_agrunt_ai_firerate)."),
        header("Fighting Distance"),
        row(opt::vr_agrunt_range),
        knob("Retreat Readiness", vr_agrunt_ai_retreat,
            "How readily a gun's holder backs off: the distance it backs away from you nearer than (its weapon's "
            "least), times this, and the health share it turns warier below (0.35 times this). 0: it never backs "
            "off (only from its own blast) (vr_agrunt_ai_retreat)."),
        header("Melee Spacing"),
        toggle("Long Blades Keep Their Reach", vr_agrunt_mspace)
            .help("One holding a long melee weapon (a sword, a long sword) stops where its swing's tip still meets your "
                  "body, its arm's reach plus its weapon's length from its grip, and swings from there; you stepping in, "
                  "it holds its ground; you backing off, it comes back. Fists, the axe, the hammers and the chainsaw "
                  "come right up to you as before. Off: every melee weapon closes in (vr_agrunt_mspace)."),
        slider("Reach Share", vr_agrunt_mspace_reach, 0.5f, 1.f, 0.05f, "%.2f").extend(0.f, 1.5f)
            .help("Where in its swing's reach (arm plus weapon, from your body's front) it stands: 1 its tip just "
                  "meeting you, 0.8 a fifth of its reach inside you, less nearer (vr_agrunt_mspace_reach)."),
        slider("Shortest Standoff", vr_agrunt_mspace_min, 32.f, 96.f, 2.f, "%.0f units").extend(0.f, 160.f)
            .help("A weapon whose standoff comes out shorter than this (its middle to yours) closes right in as before: "
                  "the fists, the axe, the hammers (vr_agrunt_mspace_min)."),
        slider("Longest Standoff", vr_agrunt_mspace_max, 48.f, 128.f, 2.f, "%.0f units").extend(0.f, 200.f)
            .help("The furthest it stands from you, however long its blade (vr_agrunt_mspace_max)."),
        slider("Re-Close Speed", vr_agrunt_mspace_speed, 0.25f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("How fast it comes back to its standoff when you back off, times its run's step "
                  "(vr_agrunt_mspace_speed)."),
        header("Aiming"),
        row(opt::vr_agrunt_aim_converge),
        row(opt::vr_agrunt_aim_exposed),
        header("Friends and Blasts"),
        row(opt::vr_agrunt_friendly_care),
        row(opt::vr_agrunt_blast_safe),
        header("Weapons Lying About"),
        row(opt::vr_agrunt_rearm),
        row(opt::vr_agrunt_rearm_range),
        row(opt::vr_agrunt_rearm_punch),
        row(opt::vr_agrunt_rearm_idle),
        row(opt::vr_agrunt_dual_pickup),
        header("Loadout"),
        row(opt::vr_agrunt_dual_chance),
        row(opt::vr_agrunt_flaregun),
        header("Parrying Your Blows (Experimental)"),
        row(opt::vr_agrunt_parry),
        row(opt::vr_agrunt_parry_strikes_only),
        row(opt::vr_agrunt_parry_react),
        row(opt::vr_agrunt_parry_chance),
        row(opt::vr_agrunt_react_parry),
        row(opt::vr_agrunt_preparry),
        row(opt::vr_agrunt_windup_parry),
        header("Manner"),
        row(opt::vr_agrunt_taunt),
        row(opt::vr_agrunt_strafe_lean),
        row(opt::vr_agrunt_strafe_turn),
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
        row(opt::vr_arena_skill_impact),
        row(opt::skill),
        header("The Waves"),
        row(opt::vr_arena),
        row(opt::vr_arena_wave_rest),
        row(opt::vr_arena_countdown),
        row(opt::vr_arena_countdown_sound),
        row(opt::vr_arena_wave_size),
        row(opt::vr_arena_alive_max),
        row(opt::vr_arena_alive_grow),
        row(opt::vr_arena_burst),
        row(opt::vr_arena_spawn_gap),
        row(opt::vr_arena_max_alive),
        header("Boss Waves"),
        row(opt::vr_arena_boss_every),
        row(opt::vr_arena_boss_health),
        header("Armour"),
        row(opt::vr_arena_foe_armor),
        row(opt::vr_arena_foe_armor_scale),
        row(opt::vr_arena_foe_armor_from),
        header("Wave Clears"),
        row(opt::vr_arena_medkits_after),
        row(opt::vr_arena_armor_every),
        row(opt::vr_arena_shards),
        row(opt::vr_arena_scatter),
        row(opt::vr_arena_score_kill),
        header("Perks"),
        row(opt::vr_arena_perks),
        row(opt::vr_arena_perk_count),
        header("Cleanup"),
        row(opt::vr_arena_corpse_fade),
        row(opt::vr_arena_corpse_max),
        row(opt::vr_arena_item_waves),
        row(opt::vr_arena_gun_max),
        header("This Run"),
        row(opt::vr_arena_seed),
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
        row(opt::vr_agrunt_damage_scale),
        row(opt::vr_agrunt_reserve),
        row(opt::vr_agrunt_dual_penalty),
        header("Their Strikes"),
        row(opt::vr_agrunt_strike_torso),
        row(opt::vr_agrunt_strike_shoulder),
        header("Disarming"),
        row(opt::vr_agrunt_disarm),
        row(opt::vr_agrunt_disarm_yank),
        row(opt::vr_agrunt_disarm_deliberate),
        row(opt::vr_agrunt_strike_first),
        row(opt::vr_agrunt_tug),
        row(opt::vr_agrunt_tug_ragdoll),
        row(opt::vr_agrunt_tug_break_force),
        row(opt::vr_agrunt_tug_break_impulse),
        row(opt::vr_agrunt_tug_let_go),
        row(opt::vr_agrunt_tug_downed_scale),
        row(opt::vr_agrunt_tug_second_grab),
        row(opt::vr_agrunt_tug_strike),
        row(opt::vr_agrunt_grab_speed),
        row(opt::vr_agrunt_tug_strength),
        row(opt::vr_agrunt_tug_time),
        row(opt::vr_agrunt_tug_fight),
        row(opt::vr_agrunt_parry_disarm),
        row(opt::vr_agrunt_fall_drop),
        row(opt::vr_agrunt_fall_keep),
        row(opt::vr_agrunt_fall_keep_impact),
        row(opt::vr_agrunt_fall_keep_window),
        row(opt::vr_agrunt_saw_drop_running),
        header("Disarming: Shooting It Out"),
        row(opt::vr_agrunt_shotdisarm),
        row(opt::vr_agrunt_shotdisarm_parts),
        row(opt::vr_agrunt_shotdisarm_size),
        row(opt::vr_agrunt_shotdisarm_chance),
        row(opt::vr_agrunt_shotdisarm_chance_scale),
        row(opt::vr_agrunt_shotdisarm_block),
        row(opt::vr_agrunt_shotdisarm_proj),
        row(opt::vr_agrunt_shotdisarm_proj_chance),
        row(opt::vr_agrunt_shotdisarm_proj_ref),
        row(opt::vr_agrunt_shotdisarm_proj_near),
        row(opt::vr_agrunt_shotdisarm_proj_max),
        header("Their Hands"),
        row(opt::vr_agrunt_ik),
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
        row(opt::vr_enemy_weapon_depleted_loot),
        header("Ogres' Chainsaws"),
        open("Chainsaw Engine", pageIndex(pageChainsawEngine))
            .help("The running engine's exhaust smoke and its shake: in one hand, in two, lying on the ground."),
        header("Grunts' Burst Rifles"),
        row(opt::vr_grunt_burst),
        header("Spent Rifles"),
        row(opt::vr_enemygun_spent_smoke),
        row(opt::vr_enemygun_spent_crackle),
        row(opt::vr_enemygun_spent_shake),
        row(opt::vr_enemygun_spent_haptics),
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
        row(opt::vr_dmg_shotgun),
        row(opt::vr_dmg_nail),
        row(opt::vr_dmg_super_nail),
        row(opt::vr_nail_speed),
        row(opt::vr_nailpistol_speed),
        row(opt::vr_nailpistol_cooldown),
        row(opt::vr_nailpistol_spread),
        row(opt::vr_nailpistol_slide_travel),
        row(opt::vr_nailpistol_screen_ride),
        row(opt::vr_flare_burn),
        row(opt::vr_flare_stick_time),
        row(opt::vr_flare_air_time),
        row(opt::vr_flare_air_fall),
        row(opt::vr_revolver_heavy_mult),
        row(opt::vr_revolver_cooldown),
        row(opt::vr_dmg_grenade),
        row(opt::vr_dmg_rocket),
        header("Scourge of Armagon"),
        row(opt::vr_dmg_laser),
        header("Dissolution of Eternity"),
        row(opt::vr_dmg_lava_nail),
        header("Melee"),
        row(opt::vr_crowbar_damage),
        header("Enemy Weapons"),
        row(opt::vr_sword_damage_mult),
        row(opt::vr_gruntgun_damage),
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
        row(opt::vr_snd_spatial),
        header("Direction"),
        row(opt::vr_snd_hrtf),
        row(opt::vr_snd_hrtf_interp),
        row(opt::vr_snd_fullband),
        row(opt::vr_snd_hrtf_gain),
        row(opt::vr_snd_voices),
        header("Walls and Rooms"),
        row(opt::vr_snd_occlusion),
        row(opt::vr_snd_occlusion_samples),
        row(opt::vr_snd_occlusion_radius),
        row(opt::vr_snd_air),
        row(opt::vr_snd_falloff),
        row(opt::vr_snd_reverb),
        row(opt::vr_snd_reverb_quality),
        row(opt::vr_snd_reverb_interval),
        row(opt::snd_waterfx),
        header("Movement and Nearness"),
        row(opt::vr_snd_hands),
        row(opt::vr_snd_follow),
        row(opt::vr_snd_doppler),
        row(opt::vr_snd_pitch_jitter),
        row(opt::vr_snd_hit_layer),
        row(opt::vr_snd_nearfield),
        header("Gunfire"),
        row(opt::vr_gunsfx),
        row(opt::vr_gunsfx_layers),
        row(opt::vr_gunsfx_tail),
        row(opt::vr_gunsfx_vary),
        row(opt::vr_gunsfx_casings),
        row(opt::vr_gunsfx_impacts),
        row(opt::vr_gunsfx_dryfire),
        row(opt::vr_gunsfx_action),
        row(opt::vr_gunsfx_punch),
        header("Advanced"),
        row(opt::vr_snd_limiter),
        row(opt::vr_snd_limiter_ceiling),
        row(opt::vr_snd_frame),
        command("Spatial Audio Info", "vr_snd_info")
            .help("vr_snd_info: in the console, Steam Audio loaded or not, the voices, the scene and the simulations' times."),
    };
}

// Split from Gameplay: damage multipliers, positional damage and knockback.
[[nodiscard]] za::Vector<Item> pageBloodFeedback()
{
    return {
        header("When Hit"),
        row(opt::vr_blood_hit),
        row(opt::vr_blood_hit_duration),
        row(opt::vr_blood_hit_intensity),
        row(opt::vr_blood_hit_size),
        row(opt::vr_blood_hit_bias),
        header("Low Health"),
        row(opt::vr_blood_health),
        row(opt::vr_blood_health_threshold),
        row(opt::vr_blood_health_intensity),
        row(opt::vr_blood_health_size),
        row(opt::vr_blood_health_duration),
        row(opt::vr_blood_health_rate),
        row(opt::vr_blood_health_rate_scale),
        row(opt::vr_blood_health_curve),
    };
}

[[nodiscard]] za::Vector<Item> pageDamage()
{
    return {
        header("Hit Detection"),
        row(opt::vr_hit_precise),
        row(opt::vr_hit_drawn_pose),
        row(opt::vr_hit_tolerance_melee),
        row(opt::vr_hit_head_priority),
        header("Damage"),
        row(opt::vr_damage_to_enemies),
        row(opt::vr_damage_to_player),
        row(opt::vr_damage_self),
        header("Positional Damage"),
        row(opt::vr_positional_damage),
        row(opt::vr_melee_positional),
        row(opt::vr_headshot_mult),
        row(opt::vr_limbshot_mult),
        row(opt::vr_legshot_mult),
        row(opt::vr_headshot_sound),
        header("Knockback"),
        row(opt::vr_push),
        row(opt::vr_melee_push),
        row(opt::vr_melee_ragdoll_bounce),
        row(opt::vr_melee_ragdoll_most),
        row(opt::vr_hit_push),
        row(opt::vr_kill_push),
        row(opt::vr_parry_push_enemy),
        row(opt::vr_parry_push_player),
        row(opt::vr_melee_push_player),
        header("When You're Hit"),
        row(opt::vr_pain_knock),
        row(opt::vr_pain_knock_strength),
        row(opt::vr_pain_knock_max),
        row(opt::vr_pain_knock_tip),
        row(opt::vr_pain_knock_seen),
        row(opt::vr_pain_knock_time),
    };
}

// Blocking and shoving with a guard, and counter-attacks after a parry (the parry settings of Melee joined them).
[[nodiscard]] za::Vector<Item> pageParryBash()
{
    return {
        header("Parry"),
        row(opt::vr_parry),
        row(opt::vr_parry_stagger),
        row(opt::vr_parry_ragdoll).extend()
            .help("A parried humanoid (its monster group's Ragdoll Triggers on: Combat > Monster Groups) staggers back as an active ragdoll, as a shove's stagger, for Parry Stagger; on level ground it never falls, with stairs, a ledge or a slope behind it it may go down. Off: its pain animation's stagger (Stagger Frames, Stagger Sway)."),
        slider("Parry Pushback", vr_parry_push_enemy, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("How far a parry pushes the attacker back (times Knockback; the distance goes about as its square: "
                  "0.55 about half as far as 0.8). More makes room; less keeps it within reach of a counter-attack. "
                  "Separate from shoves (Shove and Bash Push below)."),
        row(opt::vr_parry_reduction),
        row(opt::vr_parry_drop_chance),
        row(opt::vr_parry_wobble),
        row(opt::vr_parry_unarmed),
        row(opt::vr_dirparry),
        row(opt::vr_parry_cooldown_attack).extend()
            .help("Every blow of the attack whose blow you last parried is a cooldown blow, however long between them "
                  "(a fiend's two claws, the Overlord's double smash, a shambler's chained swings): one parry and one "
                  "stamina cost per attack. Off: only the Parry Cooldown's time."),
        header("Parry Balance"),
        row(opt::vr_parry_balance),
        row(opt::vr_parry_stagger_min),
        row(opt::vr_parry_stagger_max),
        row(opt::vr_parry_stagger_heavy),
        row(opt::vr_poise_chances),
        header("Bash and Shove"),
        row(opt::vr_bash),
        row(opt::vr_bash_speed),
        row(opt::vr_shove_push_onehand),
        row(opt::vr_shove_push_twohand),
        row(opt::vr_bash_sound),
        header("Counter-Attacks"),
        row(opt::vr_counter),
        row(opt::vr_counter_sound),
        row(opt::vr_counter_glow),
    };
}

// Split from Parry, Bash and Headbutt: the stamina parries, shoves and blows cost.
[[nodiscard]] za::Vector<Item> pageStamina()
{
    return {
        header("Parry Stamina"),
        row(opt::vr_parry_stamina),
        open("Stamina on the Gadget: Screens", pageIndex(pageScreens)).help("What the gadget shows (stamina and counters) is on HUD and Menus > Screens."),
        header("Shove and Strike Stamina"),
        row(opt::vr_shove_stamina),
        row(opt::vr_strike_stamina),
                header("Climbing Stamina"),
        row(opt::vr_climb_stamina),
        header("Tired Arms"),
        row(opt::vr_fatigue_shake),
        row(opt::vr_fatigue_shake_angle),
        row(opt::vr_fatigue_shake_always),
        header("Tired Legs"),
        row(opt::vr_stamina_speed),
        open("Heavy When Tired (Aiming)", pageIndex(pageAimingSettings)).help("Tired hands get heavy too (what they hold, and an empty hand): Tired Arms, on the Aiming page."),
    };
}

// Batting projectiles back (Parry, Bash and Headbutt) and catching grenades (Carrying and Gibs).
[[nodiscard]] za::Vector<Item> pageBatting()
{
    return {
        header("Batting Projectiles"),
        row(opt::vr_deflect),
        row(opt::vr_deflect_speed),
        row(opt::vr_deflect_window),
        row(opt::vr_deflect_swing).extend().help("Only a real swing bats: the hand itself moving fast (Batting Hand Speed), from some way off, not a weapon waved back and forth. Off: any weapon moving fast enough at its end bats, a turn of the wrist or a gun brought round to aim too."),
        row(opt::vr_bash_deflect_window),
        header("Grenades"),
        row(opt::vr_grenade_catch),
        row(opt::vr_grenade_fuse_regrab),
        row(opt::vr_grenade_return_full),
        row(opt::vr_grenade_shoot),
        row(opt::vr_grenade_shoot_melee),
        header("Hand Grenades"),
        row(opt::vr_handgrenade),
        row(opt::vr_handgrenade_arm),
        header("Grenade Pouch"),
        open("Grenade Pouch: Hip Holsters", pageIndex(pageHipHolsters)).help("Where the grenade pouch is and how a grenade sits in the hand: Weapons > Hip Holsters."),
        open("Pouch Turn (Hip Holsters)", pageIndex(pageHipHolsters))
            .help("The same place, and how the pouch is turned, with the hip holsters."),
    };
}
