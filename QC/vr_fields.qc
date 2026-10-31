//
//
//
// ----------------------------------------------------------------------------
// Think closures
// ----------------------------------------------------------------------------

.void(float) thinkArgFn;
.void(float) think2ArgFn;
.float thinkArg;
.float vr_flashlight_state; // packed client lamp state, saved and carried by parm50
.float vr_saw_cord_pulled; // the player has pulled a chainsaw's cord (set by the engine: vr_chainsaw.cpp, the cord's hint
                           // stops: vr_chainsaw_cord_hint 1), saved and carried by parm50's bit 19
.float vr_bullettime_spent; // bullet time's meter used up (0: full, 1: empty; kept by the engine: vr_bullettime.cpp), saved and carried by parm74
.float vr_bullettime_earned; // the damage the player dealt living monsters since the engine last took it (VR_BulletTime_Earn, combat.qc)
float VR_PARM50_SAW_CORD_PULLED = 524288; // that bit (2^19: the lamp's state takes bits 0..18)
.float vr_gib; // a gib or head that can be carried (VR_MakeGibGrabbable)
.entity vr_wpn_from; // a weapon lying about or flying: the player whose hand it left (DropWeaponInHandScaled), else world
.float vr_grab_by_handle; // a weapon lying about: a hand closing anywhere on it takes it by its handle, never carries it by a hotspot (func_weapon_grabbable BY_HANDLE)
.float vr_born; // when it was made (set by the engine, ED_Alloc: vr_cheats.cpp; VR_Scene_IsRuntimeProp)
.float vr_hpscale; // a monster: the multiplier Monster Health put on its health as it first appeared (set by the engine, vr_mgroups.cpp VR_MonsterHealth_Frame; 0 not yet)
.float vr_gate_crossed; // when a missile was last carried through a teleporter (set by the engine: VR_PortalToss); then it meets its owner (VR_OwnerSpared)
.float vr_sgib; // a small gib (vr_smallgibs.qc): when it was made
.float vr_sgib_id; // its serial in the small gibs' ring (0: gone)
.entity vr_hit_extra; // precise hits (vr_hitmodel.cpp): a part drawn as an entity of its own (not solid) its shots meet
                      // as this monster (the Armagon's torso: hip_mon_arma.qc)
.float vr_flies; // a severed head's flies buzz round it (HeadThink, player.qc; VR_Head_StopFlies)
.float think2Arg;

//
//
//
// ----------------------------------------------------------------------------
// Hand-grabbing fields
// ----------------------------------------------------------------------------

.float offhand_lastgrabtime;
.float mainhand_lastgrabtime;
.float offhand_lastreleasetime;
.float mainhand_lastreleasetime;
.float vr_weapon_load_until; // after loading a save: the original held weapons stay for this long
.entity vr_weapon_load_main;
.entity vr_weapon_load_off;
.entity vr_weapon_given_main; // a test scene's weapon given into the hand: held until that hand's grip is pressed (VRWeaponGivenHeld)
.entity vr_weapon_given_off;


//
//
//
// ----------------------------------------------------------------------------
// Carrying (vr_carry.qc): the hand slot, the thing each hand holds besides its weapon (.weapon/.weapon2): the thing
// (.mainhand_held/.offhand_held, the engine reads them) and its kind (VR_HOLD_*). Written only by VR_Hand_Take and
// VR_Hand_Release (vr_carry.qc); here so that vr_util.qc's VRIsHandEmpty sees them.
// ----------------------------------------------------------------------------

.entity mainhand_held;
.entity offhand_held;
.float vr_hold_kind_main;
.float vr_hold_kind_off;
float VR_HOLD_NONE = 0;
float VR_HOLD_PROP = 1;    // a box, a gib, a head, an item to put away, a lying weapon's prop (anything carried not below)
float VR_HOLD_RAGDOLL = 2; // a body or a limb of one (a foe grab's throw, a ragdoll grab)
float VR_HOLD_TORCH = 3;   // a wall torch taken off its wall
float VR_HOLD_ROUND = 4;   // a round, a magazine, a front-loaded round (vr_reload.qc)
float VR_HOLD_GRENADE = 5; // a hand grenade (vr_grenade.qc)
// The busy kinds (VR_Hand_Busy/VR_Hand_Unbusy): the hand carries nothing (.mainhand_held world: VRIsHandEmpty still
// true, it can make a fist or steady a gun) but is busy, so it takes nothing (VR_Hand_Free).
float VR_HOLD_FOE = 6;        // an enemy held (the engine's foe grab: .vr_foegrab_hands, read each PlayerPreThink)
float VR_HOLD_FLASHLIGHT = 7; // the flashlight in the hand (the client's: .vrbits0's *_BUSY, read each PlayerPreThink)
float VR_HOLD_SSGGRIP = 8;    // on the other hand's super shotgun, the two-handed hold (vr_reload.qc VR_Reload_SsgFrame)
float VR_HOLD_MAGREHOLD = 9;  // still gripping the magazine it just seated (vr_reload.qc: to the next VR_Reload_MagFrame)
float VR_HOLD_TUG = 10;       // a force grab's tug on a stuck axe (vr_axestick.qc; the axe in .vr_hold_ent_*)
.entity vr_hold_ent_main;     // a busy kind's thing (the tugged axe; else world)
.entity vr_hold_ent_off;
// A grab site's exceptions to "free to grab" (VR_Hand_Free's `xAllow`, vr_wpnforcegrab.qc; 0: none, the usual):
float VR_HANDFREE_BUSY = 1;   // any busy kind takes it too: the water climb's grip on the bank (not a grab of a thing). A
                              // weapon's grab (lying, thrown, a grunt's, a holster's draw) is refused by a busy hand like
                              // any grab (his decision, 2026-10-31: the flashlight hand takes no weapon)
float VR_HANDFREE_FOE = 2;    // busy with an enemy alone takes it: the foe grab's hand-over (the enemy becomes a ragdoll)
float VR_HANDFREE_LOCKED = 4; // the force grab's lock does not refuse: its own frame and catch, a weapon's grab
float VR_HANDFREE_GUNGRIP = 8; // busy on your own gun alone takes it (its fore-end, VR_HOLD_SSGGRIP; the magazine it just
                               // seated, VR_HOLD_MAGREHOLD): the hand-off and the grapple's pass (two hands on that gun)
.float vr_carry_items;   // the hands carrying a thing to put away (vr_carry.qc VR_Carry_ItemsFrame: bit 0 the off hand, 1 the main; STAT_QVR_CARRYITEM)
.float vr_carry_fullcue; // on a player: no full cue again before then (VR_Carry_FullCue)
.float carry_autotried;  // on a box: vr_carry_autocollect tried it this carry (VR_Carry_AutoCollect)

// ----------------------------------------------------------------------------
// Force-grabbing fields
// ----------------------------------------------------------------------------

// Per hand, on the player (vr_wpnforcegrab.qc): the object pointed at or locked on to.
.entity offhand_fgtarget;
.entity mainhand_fgtarget;
.float offhand_fglocked;
.float mainhand_fglocked;
.float thrownweapon_timeout; // a thrown weapon's removal time in deathmatch
.entity offhand_fgpulled; // the object last pulled, while it flies to that hand
.entity mainhand_fgpulled;
.float offhand_fgrearm;   // 1 after a pull, until the trigger is let go
.float mainhand_fgrearm;
.float offhand_fgheldfoe; // the last time the hand held an enemy (vr_foegrab.qc): no force grab for a moment after
.float mainhand_fgheldfoe;
float cVR_Forcegrab_AfterFoe = 0.5; // s: that moment (a judo throw's follow-through)

// On the object pulled.
.entity fg_player;
.float fg_hand;
.float fg_state;    // 0 not pulled, 1 flying to the hand, 2 missed: falling, not yet touchable
.float fg_portal;   // one-based teleporter used by this gravity-glove flight, 0 same room
.float fg_arrive;   // when it reaches the hand
.float fg_gravity;  // of its arc
.float fg_movetype; // as it was before
.vector fg_origin;  // where it was before it flew (it fitted there)
.float fg_solid;
.void() fg_think;

//
//
//
// ----------------------------------------------------------------------------
// Thrown weapon fields
// ----------------------------------------------------------------------------

.float throwhit;
.float stickyarmed;   // vr_weapon_grip_mode 1: hands (bit 1 off, 2 main) whose grip was let go since the weapon came
.float stickypending; // ... hands whose grip was let go again: throw or holster when possible
.float stickyweaponoff, stickyweaponmain; // the weapon each hand had last frame
.float throwdmgspeed; // speed (units/s) at which a thrown weapon deals its base damage
.float throwstabilize;
.float throwstabilizedim;

//
//
//
// ----------------------------------------------------------------------------
// Off-hand attack fields
// ----------------------------------------------------------------------------

.float offhand_attack_finished;

//
//
//
// ----------------------------------------------------------------------------
// Main hand melee fields
// ----------------------------------------------------------------------------

.float mainhand_melee_attack_finished;
.float in_melee;
.float melee_hit_sound_played;

//
//
//
// ----------------------------------------------------------------------------
// Off-hand melee fields
// ----------------------------------------------------------------------------

.float offhand_melee_attack_finished;
.float offhand_in_melee;
.float offhand_melee_hit_sound_played;

//
//
//
// ----------------------------------------------------------------------------
// 2H-related fields
// ----------------------------------------------------------------------------

.float last_2h_time;

//
//
//
// ----------------------------------------------------------------------------
// Headbutt-related fields
// ----------------------------------------------------------------------------

.float headbutt_start_time;
.float in_headbutt;
.vector headbutt_max_vec;

//
//
//
// ----------------------------------------------------------------------------
// Lava weapon fields
// ----------------------------------------------------------------------------

.float mainHandLavaGunFired;
.float offHandLavaGunFired;

//
//
//
// ----------------------------------------------------------------------------
// Holster fields
// ----------------------------------------------------------------------------

// Used to trigger haptics when hovering holsters.
.float holsterhover[6];
.float holsterprevhover[6];

//
//
//
// ----------------------------------------------------------------------------
// World text fields
// ----------------------------------------------------------------------------

.string worldtext;
.float worldtext_halign;
.float worldtext_scale;
.float vr_button_label_top; // a func_button's label board's top over the button's top (vr_setup.cpp puts its screens above it)
.float angle;

//
//
//
// ----------------------------------------------------------------------------
// A monster oblivious until hurt (the map's "vr_oblivious" "1": vrtrailer's grunt; vr_trailer.qc): nothing wakes it --
// no sight, sound, footsteps, shove or stealth AI (FindTarget, VR_EnemyShove_Check, VR_Stealth_Applies) -- until it
// takes damage (T_Damage clears it); a beheading cut on it always kills (VR_Trailer_Take).
// ----------------------------------------------------------------------------

.float vr_oblivious;

// ----------------------------------------------------------------------------
// Map tips (func_vr_tip: QC/vr_tips.qc)
// ----------------------------------------------------------------------------

.string tipname;   // the tip's name (its key in tips_seen.txt)
.string tip_targetname; // its targetname as the map set it (func_vr_tip clears .targetname: only its name)
.float tiphandle;  // the engine's tip record this one made (vr_tip_make)
.float tip_size;   // its screen's text size (0: vr_tips_size)
.string tip_delay; // seconds near before it shows: 0 at once, -1 (or no key) vr_tips_delay (a string: no key is not 0)

//
//
//
// ----------------------------------------------------------------------------
// Button fields
// ----------------------------------------------------------------------------

.float buttonEffect;

//
//
//
// ----------------------------------------------------------------------------
// Wooden crates (vr_crates.qc)
// ----------------------------------------------------------------------------

.float vr_cratekind;     // 1 a small crate, 2 a large one, 3 a piece of one
.float vr_crate_hittime; // a crate: when it last knocked (VR_Crate_Pain)
.float vr_crate_born;    // a piece: when it was made (the oldest go first; they fade after vr_crate_piece_time)
.float alpha;            // how opaque it is drawn (0: wholly; a piece fading away). (Here, not in honey_defs.qc: the
                         // crates' pieces, compiled before it, use it.)

// Explosion chunks (vr_explosiondebris.cpp): the engine makes them (an explosion's temp entity), Box3D moves them, a
// fade and SUB_Remove end them; nothing here touches them.
.float vr_xdebris;      // a chunk: when it was made (0: not one)

//
//
//
// ----------------------------------------------------------------------------
// Grenades shots set off (vr_grenade.qc, "Shooting grenades")
// ----------------------------------------------------------------------------

.float vr_shootnade;   // a grenade shots, blows and blasts set off (VR_GrenShot_Make): T_Damage is its (VR_GrenShot_Damage)
.float vr_shot_half;   // its model's half size (the shot box: vr_shot_radius, with vr_grenade_shoot_pad)
.float vr_shot_off;    // set off (or gone off): no more shots meet it
.void() vr_shot_boom;  // a Quake grenade set off: its own think (its blast), run by VR_GrenShot_Boom
.void() vr_ammo_boom;  // a launcher's round lying about (vr_reload.qc): its blast when set off (its think is its fading)
// The rounds of immersive reloading (vr_reload.qc), here for the pouches' grenades (vr_grenade.qc, "One grenade from
// either pouch"), which are rounds too while unarmed:
.float vr_ammo_aid;      // a round: the ammo it is (AID_*)
.float vr_ammo_count;    // and how many (a taped pair: 2)
.float vr_ammo_front;    // a launcher's round: the launcher it fits (WID_GRENADE_LAUNCHER, _PROXIMITY_GUN, _ROCKET_LAUNCHER)
.float vr_ammo_born;     // when it was last in a hand or flying (its time lying about runs from here)

// ----------------------------------------------------------------------------
// The parry's cooldown (combat.qc VR_Parry) and your own throws hurting you (vr_carry.qc, "Your own throws")
// ----------------------------------------------------------------------------

.float vr_parry_stagger_until; // successful parry: no remaining melee hits or AI until this time (combat.qc)
.float vr_parry_stagger_start; // ... when it began (its sway's phase: VR_Parry_Pose)
.float vr_parry_frame0;  // ... the first frame of the pain animation it steps through (VR_Parry_Pose)
.float vr_parry_frames;  // ... how many frames of it (0: none, its pose held)
.float vr_parry_step;    // ... its steps so far
.float vr_parry_sway;    // ... its sway, degrees (0: none, its angles as they were)
.float vr_parry_last;    // (a monster) when a blow of its was last parried: within vr_parry_cooldown, no new parry (VR_Parry)
.float vr_melee_begun;   // (a monster) when its melee attack running began (its th_melee called; 0: none, ai_run clears it):
                         // a blow of the attack its last parried blow was in is no new parry (VR_Parry, vr_parry_cooldown_attack)
.float vr_throw_self;    // a thing a player threw hard (his: .owner): 1 sparing him until vr_throw_self_at; 2 past it,
                         // .owner given up (it meets him) and the throw still his (VR_Throw_Thrower: .carry_letgo)
.float vr_throw_self_at; // when it can hurt its thrower (vr_throw_self_grace after the throw)
.float vr_letgo_fall;    // a prop let go of gently (not thrown hard), in its first fall: it never hurts who let go
                         // of it (VR_Prop_Flung) until it lands, is taken or flies up (VR_Throw_SelfFrame)
.float vr_was_monster;   // a monster's head (VR_MakeGibGrabbable took its FL_MONSTER): what it drops after (a gibbed
                         // grunt's backpack: SoldierDrop after ThrowHead) is still a monster's drop (.vr_monster_drop)

.float vr_drop_born;     // a thrown_weapon: when it was made (dropped, thrown); the oldest single-use enemy weapon lying
                         // about goes first past vr_enemy_weapon_drop_max (vr_enemyguns.qc VR_EnemyWeapons_MakeRoom)
.float vr_drop_fade;     // such a weapon fading away (past the cap): when it began; it counts no more, can't be taken
.float vr_enemy_drop;    // a weapon's record (.weaponinst): a monster dropped it (a grunt's, an enforcer's, an ogre's):
                         // capped while it lies about, whoever let go of it since (not a map's or a dispenser's)

.float carry_portal; // room mapping of a held object, one-based gate

// The tutorial's scripting (vr_tutorial.qc; vr_crates.qc's contents)
.string contents;      // vr_crate: what it holds (a classname; "none": nothing); func_vr_restock: what it puts
.float contentsflags;  // func_vr_restock: what it puts's spawnflags
.entity vr_spawner;    // a monster a func_vr_spawner made: that spawner
.float vr_cp_taken;    // info_vr_checkpoint: when it was taken (0: not yet)
.float vr_spawn_left;  // func_vr_spawner: monsters still to make for its latest use
.float vr_spawn_armed; // func_vr_spawner: its latest use's monsters not all dead yet (its target not fired)
.entity vr_spawn_by;   // func_vr_spawner: who used it last (its target's activator)
.float vr_spawn_seen;  // a monster a func_vr_spawner made: when it was last up or in the player's sight (VR_Spawner_Unstrand)
.float tip_on;         // func_vr_tip (TRIGGERED): used: shown from now on
