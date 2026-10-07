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
.float vr_gib; // a gib or head that can be carried (VR_MakeGibGrabbable)
.float vr_born; // when it was made (set by the engine, ED_Alloc: vr_cheats.cpp; VR_Scene_IsRuntimeProp)
.float vr_sgib; // a small gib (vr_smallgibs.qc): when it was made
.float vr_sgib_id; // its serial in the small gibs' ring (0: gone)
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


//
//
//
// ----------------------------------------------------------------------------
// Carrying (vr_carry.qc): the object each hand carries, here so that vr_util.qc's VRIsHandEmpty sees them
// ----------------------------------------------------------------------------

.entity mainhand_held;
.entity offhand_held;

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

// On the object pulled.
.entity fg_player;
.float fg_hand;
.float fg_state;    // 0 not pulled, 1 flying to the hand, 2 missed: falling, not yet touchable
.float fg_portal;   // one-based slipgate used by this gravity-glove flight, 0 same room
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
.float angle;

//
//
//
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

// ----------------------------------------------------------------------------
// The parry's cooldown (combat.qc VR_Parry) and your own throws hurting you (vr_carry.qc, "Your own throws")
// ----------------------------------------------------------------------------

.float vr_parry_stagger_until; // successful parry: no remaining melee hits or AI until this time (combat.qc)
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

.float carry_portal; // room mapping of a held object, one-based gate
