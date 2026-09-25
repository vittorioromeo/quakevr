//
//
//
// ----------------------------------------------------------------------------
// Think closures
// ----------------------------------------------------------------------------

.void(float) thinkArgFn;
.void(float) think2ArgFn;
.float thinkArg;
.float vr_gib; // a gib or head that can be carried (VR_MakeGibGrabbable)
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
// Button fields
// ----------------------------------------------------------------------------

.float buttonEffect;
