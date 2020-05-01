//
//
//
// ----------------------------------------------------------------------------
// ROGUE Constants
// ----------------------------------------------------------------------------

float STAT_TOTALMONSTERS = 12;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Globals
// ----------------------------------------------------------------------------

float  earthquake_active;
float  earthquake_intensity;
float  cutscene_running;
entity tag_token_owner;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Fields
// ----------------------------------------------------------------------------

.float rogue_items; // Renamed from `.items2`.
// .float ammo_shells1, ammo_nails1, ammo_rockets1, ammo_cells1; // TODO VR: (P0) needed?
.float ammo_lava_nails;
.float ammo_multi_rockets;
.float ammo_plasma;
.float gravity;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Fields (player-only)
// ----------------------------------------------------------------------------

.float  shield_finished, antigrav_finished;
.float  shield_time, antigrav_time;
.entity shield_entity;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Fields (elevator)
// ----------------------------------------------------------------------------

.float  elevatorLastUse;
.float  elevatorOnFloor;
.float  elevatorToFloor;
.vector elevatorDestination;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Fields (plat2)
// ----------------------------------------------------------------------------

.float plat2Called;
.float plat2LastMove;
.float plat2GoTime;
.float plat2GoTo;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Fields (AI)
// ----------------------------------------------------------------------------

.float orbitPosition;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Fields (dragon)
// ----------------------------------------------------------------------------

.float playerInRoom;
.float playerInTransit;
.float dragonInRoom;
.float dragonInTransit;
.float dragonAttacking;
.float dragonPainSequence;
.vector dragonLastVelocity;

//
//
//
// ----------------------------------------------------------------------------
// ROGUE Fields (other)
// ----------------------------------------------------------------------------

.float AGping;
.float childrenSpawned;
.float ltrailLastUsed;
.float shield_death_time;
.float shieldSoundTime;
.float dragonDeathState;
.float tag_frags;
.float tag_message_time;
