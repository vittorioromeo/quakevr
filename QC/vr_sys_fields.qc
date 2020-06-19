//
//
//
// ----------------------------------------------------------------------------
// Entity (visual) scale fields
// ----------------------------------------------------------------------------

.vector scale;
.vector scale_origin;
.vector model_offset;

//
//
//
// ----------------------------------------------------------------------------
// New item management fields
// ----------------------------------------------------------------------------

.float vr_itemId;

//
//
//
// ----------------------------------------------------------------------------
// Hand and weapon touching callback fields
// ----------------------------------------------------------------------------

.void() handtouch;        // touched by VR hand
.void() vr_wpntouch;      // touched by VR weapon

//
//
//
// ----------------------------------------------------------------------------
// Off-hand think callback fields
// ----------------------------------------------------------------------------

.void() think2;
.float  nextthink2;

//
//
//
// ----------------------------------------------------------------------------
// Main hand weapon fields
// ----------------------------------------------------------------------------

.float weaponflags;

//
//
//
// ----------------------------------------------------------------------------
// Off-hand weapon fields
// ----------------------------------------------------------------------------

.float  weapon2;            // WID
.string weaponmodel2;
.float  weaponframe2;
.float  weaponflags2;

//
//
//
// ----------------------------------------------------------------------------
// Holster weapon ID fields
// ----------------------------------------------------------------------------

.float holsterweapon0; // Hotspot: `QVR_HS_LEFT_SHOULDER_HOLSTER`.
.float holsterweapon1; // Hotspot: `QVR_HS_RIGHT_SHOULDER_HOLSTER`.
.float holsterweapon2; // Hotspot: `QVR_HS_LEFT_HIP_HOLSTER`.
.float holsterweapon3; // Hotspot: `QVR_HS_RIGHT_HIP_HOLSTER`.
.float holsterweapon4; // Hotspot: `QVR_HS_LEFT_UPPER_HOLSTER`.
.float holsterweapon5; // Hotspot: `QVR_HS_RIGHT_UPPER_HOLSTER`.

//
//
//
// ----------------------------------------------------------------------------
// Holster weapon model fields
// ----------------------------------------------------------------------------

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
//
// ----------------------------------------------------------------------------
// Holster weapon flags fields
// ----------------------------------------------------------------------------

.float holsterweaponflags0; // Hotspot: `QVR_HS_LEFT_SHOULDER_HOLSTER`.
.float holsterweaponflags1; // Hotspot: `QVR_HS_RIGHT_SHOULDER_HOLSTER`.
.float holsterweaponflags2; // Hotspot: `QVR_HS_LEFT_HIP_HOLSTER`.
.float holsterweaponflags3; // Hotspot: `QVR_HS_RIGHT_HIP_HOLSTER`.
.float holsterweaponflags4; // Hotspot: `QVR_HS_LEFT_UPPER_HOLSTER`.
.float holsterweaponflags5; // Hotspot: `QVR_HS_RIGHT_UPPER_HOLSTER`.

//
//
//
// ----------------------------------------------------------------------------
// Hotspot fields
// ----------------------------------------------------------------------------

.float offhand_hotspot;
.float mainhand_hotspot;

//
//
//
// ----------------------------------------------------------------------------
// Off-hand ammunition fields
// ----------------------------------------------------------------------------

.float currentammo2;  // off-hand
.float ammocounter2;  // off-hand

//
//
//
// ----------------------------------------------------------------------------
// Off-hand firing button field
// ----------------------------------------------------------------------------

.float button3;        // off-hand fire

//
//
//
// ----------------------------------------------------------------------------
// VR Yaw Angle (Thumbstick-Controlled Turn Angle)
// ----------------------------------------------------------------------------

.float vryaw;

//
//
//
// ----------------------------------------------------------------------------
// Main hand physics fields
// ----------------------------------------------------------------------------

.vector handpos;
.vector handrot;
.vector handvel;
.vector handthrowvel;
.float  handvelmag;
.vector handavel;

//
//
//
// ----------------------------------------------------------------------------
// Off-hand physics fields
// ----------------------------------------------------------------------------

.vector offhandpos;
.vector offhandrot;
.vector offhandvel;
.vector offhandthrowvel;
.float  offhandvelmag;
.vector offhandavel;

//
//
//
// ----------------------------------------------------------------------------
// Head physics fields
// ----------------------------------------------------------------------------

.vector headvel;

//
//
//
// ----------------------------------------------------------------------------
// Weapon muzzle fields
// ----------------------------------------------------------------------------

.vector muzzlepos;
.vector offmuzzlepos;

//
//
//
// ----------------------------------------------------------------------------
// VR-related bits
// ----------------------------------------------------------------------------

.float vrbits0;

//
//
//
// ----------------------------------------------------------------------------
// Teleportation fields
// ----------------------------------------------------------------------------

.vector teleport_target;

//
//
//
// ----------------------------------------------------------------------------
// Roomscale movement fields
// ----------------------------------------------------------------------------

.vector roomscalemove;

//
//
//
// ----------------------------------------------------------------------------
// Handtouch-related fields
// ----------------------------------------------------------------------------

.float  touchinghand;
.float  handtouch_hand;
.entity handtouch_ent;

//
//
//
// ----------------------------------------------------------------------------
// Bot-related fields
// ----------------------------------------------------------------------------

.float ishuman;
