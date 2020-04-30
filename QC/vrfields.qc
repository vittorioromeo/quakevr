//
//
// ----------------------------------------------------------------------------
// VR: Entity (visual) scale fields.
.vector scale;
.vector scale_origin;

//
//
// ----------------------------------------------------------------------------
// VR: Hand and weapon touching callback fields.
.void() handtouch;        // touched by VR hand
.void() vr_wpntouch;      // touched by VR weapon

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand think callback fields.
.void() think2;
.float  nextthink2;

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand weapon fields.
.float  weapon2;            // WID
.string weaponmodel2;
.float  weaponframe2;

//
//
// ----------------------------------------------------------------------------
// VR: Holster weapon ID fields.
.float holsterweapon0; // Hotspot: `QVR_HS_LEFT_SHOULDER_HOLSTER`.
.float holsterweapon1; // Hotspot: `QVR_HS_RIGHT_SHOULDER_HOLSTER`.
.float holsterweapon2; // Hotspot: `QVR_HS_LEFT_HIP_HOLSTER`.
.float holsterweapon3; // Hotspot: `QVR_HS_RIGHT_HIP_HOLSTER`.
.float holsterweapon4; // Hotspot: `QVR_HS_LEFT_UPPER_HOLSTER`.
.float holsterweapon5; // Hotspot: `QVR_HS_RIGHT_UPPER_HOLSTER`.

//
//
// ----------------------------------------------------------------------------
// VR: Holster weapon model fields.
// TODO VR: (P2) these are needed because they are read in C++ as 'STAT_XXX'
// variables to determine the weapon. This should be changed to use WIDs.
.string holsterweaponmodel0;
.string holsterweaponmodel1;
.string holsterweaponmodel2;
.string holsterweaponmodel3;
.string holsterweaponmodel4;
.string holsterweaponmodel5;

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand ammunition fields.
.float currentammo2;  // off-hand
.float ammocounter2;  // off-hand

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand firing button field.
.float button3;        // off-hand fire

//
//
// ----------------------------------------------------------------------------
// VR: Main hand physics fields.
.vector handpos;
.vector handrot;
.vector handvel;
.vector handthrowvel;
.float  handvelmag;
.vector handavel;

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand physics fields.
.vector offhandpos;
.vector offhandrot;
.vector offhandvel;
.vector offhandthrowvel;
.float  offhandvelmag;
.vector offhandavel;

//
//
// ----------------------------------------------------------------------------
// VR: Weapon muzzle fields..
.vector muzzlepos;
.vector offmuzzlepos;

//
//
// ----------------------------------------------------------------------------
// VR: Teleportation fields.
.float  teleporting;
.vector teleport_target;

//
//
// ----------------------------------------------------------------------------
// VR: Roomscale movement fields.
.vector       roomscalemove;

//
//
// ----------------------------------------------------------------------------
// VR: Handtouch-related fields.
.float  touchinghand;
.float  handtouch_hand;
.entity handtouch_ent;

//
//
// ----------------------------------------------------------------------------
// VR: Main hand grabbing fields.
.float mainhand_grabbing;
.float mainhand_prevgrabbing;

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand grabbing fields.
.float offhand_grabbing;
.float offhand_prevgrabbing;

//
//
// ----------------------------------------------------------------------------
// VR: Force-grabbing fields.
.float offhand_forcegrabbing;
.float mainhand_forcegrabbing;

//
//
// ----------------------------------------------------------------------------
// VR: Hotspot fields.
.float offhand_hotspot;
.float mainhand_hotspot;

//
//
// ----------------------------------------------------------------------------
// VR: Thrown weapon fields.
.float throwhit;
.float throwstabilize;
.float throwstabilizedim;

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand attack fields.
.float offhand_attack_finished;

//
//
// ----------------------------------------------------------------------------
// VR: Main hand melee fields.
.float mainhand_melee_attack_finished;
.float in_melee;
.float melee_hit_sound_played;

//
//
// ----------------------------------------------------------------------------
// VR: Off-hand melee fields.
.float offhand_melee_attack_finished;
.float offhand_in_melee;
.float offhand_melee_hit_sound_played;
