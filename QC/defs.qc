#include "vr_macros.qc"

//
//
//
// ----------------------------------------------------------------------------
// System globals
// ----------------------------------------------------------------------------

entity self;
entity other;
entity world;

float time;
float frametime;

float force_retouch; // Force all entities to touch triggers
                     // next frame. This is needed because
                     // non-moving things don't normally scan
                     // for triggers, and when a trigger is
                     // created (like a teleport trigger), it
                     // needs to catch everything.
                     // decremented each frame, so set to 2
                     // to guarantee everything is touched.

string mapname;

float deathmatch;
float coop;
float teamplay;

float serverflags; // Propagated from level to level, used to
                   // keep track of completed episodes.

float total_secrets;
float total_monsters;
float found_secrets;   // Number of secrets found.
float killed_monsters; // Number of monsters killed.

//
//
//
// ----------------------------------------------------------------------------
// Spawnparms
// ----------------------------------------------------------------------------

// Used to encode information about clients across server level changes.
float parm1;  // Field: `.items`.
float parm2;  // Field: `.health`.
float parm3;  // Field: `.armorvalue`.
float parm4;  // Field: `.ammo_shells`.
float parm5;  // Field: `.ammo_nails`.
float parm6;  // Field: `.ammo_rockets`.
float parm7;  // Field: `.ammo_cells`.
float parm8;  // Field: `.weapon`.
float parm9;  // Field: `.armortype`.
float parm10; // Field: `.weapon2`.
float parm11; // Field: `.holsterweapon0`.
float parm12; // Field: `.holsterweapon1`.
float parm13; // Field: `.holsterweapon2`.
float parm14; // Field: `.holsterweapon3`.
float parm15; // Field: `.holsterweapon4`.
float parm16; // Field: `.holsterweapon5`.
float parm17; // Field: `.hipnotic_items`.
float parm18; // Field: `.rogue_items`.
float parm19; // Field: `.ammo_lava_nails`.
float parm20; // Field: `.ammo_multi_rockets`.
float parm21; // Field: `.ammo_plasma`.
float parm22; // Field: `.steam`.
float parm23; // Field: `.weaponflags`.
float parm24; // Field: `.weaponflags2`.
float parm25; // Field: `.holsterweaponflags0`.
float parm26; // Field: `.holsterweaponflags1`.
float parm27; // Field: `.holsterweaponflags2`.
float parm28; // Field: `.holsterweaponflags3`.
float parm29; // Field: `.holsterweaponflags4`.
float parm30; // Field: `.holsterweaponflags5`.
float parm31; // Unused.
float parm32; // Unused.

//
//
//
// ----------------------------------------------------------------------------
// Global variables set by builtins
// ----------------------------------------------------------------------------

vector v_forward; // Set by `makevectors` and `makeforward`.
vector v_up;      // Set by `makevectors`.
vector v_right;   // Set by `makevectors`.

// Set by `traceline` and/or `tracebox`.
float  trace_allsolid;
float  trace_startsolid;
float  trace_fraction;
vector trace_endpos;
vector trace_plane_normal;
float  trace_plane_dist;
entity trace_ent;
float  trace_inopen;
float  trace_inwater;

entity msg_entity; // Destination of single entity writes.

//
//
//
// ----------------------------------------------------------------------------
// Required prog functions
// ----------------------------------------------------------------------------

void() main; // Only for testing.

void() StartFrame;

void() PlayerPreThink;
void() PlayerPostThink;

void() ClientKill;
void() ClientConnect;
void() PutClientInServer; // Call after setting the parm1... parms.
void() ClientDisconnect;

void() SetNewParms; // Called when a client first connects to
                    // a server. Sets parms so they can be
                    // saved off for restarts.

void() SetChangeParms; // Call to set parms for self so they can
                       // be saved for a level transition.

void() OnSpawnServer; // Called when a new server is spawned.

//
//
//
// ----------------------------------------------------------------------------
// Special flag (!)
// ----------------------------------------------------------------------------

void  end_sys_globals; // Flag for structure dumping.

//
//
//
// ----------------------------------------------------------------------------
// System fields
// ----------------------------------------------------------------------------

// `(***)` = do not set in prog code, maintained by C code)

.float modelindex; // (***) model index in the precached list

.vector absmin, absmax; // (***) origin + mins / maxs

.float ltime; // Local time for entity.

.float movetype;
.float solid;

.vector origin;    // (***)
.vector oldorigin; // (***)
.vector velocity;
.vector angles;
.vector avelocity;

.vector punchangle; // Temp angle adjust from damage or recoil.

.string classname; // Spawn function.

.string model;

.float frame;
.float skin;
.float effects;

.vector mins, maxs; // Bounding box extents reletive to origin.
.vector size;       // `maxs - mins`.

.void() touch;
.void() use;
.void() think;
.void() blocked; // For doors or plats, called when can't push other.

.float  nextthink;
.entity groundentity;

// Stats.
.float health;
.float frags;

.float  weapon; // Weapon ID.
.string weaponmodel;
.float  weaponframe;

.float currentammo;

.float ammocounter;

.float ammo_shells, ammo_nails, ammo_rockets, ammo_cells;

.float items; // Bit flags.

.float  takedamage;
.entity chain;
.float  deadflag;

.vector view_ofs; // Add to origin to get eye point.


.float button0; // Fire.
.float button1; // Use.
.float button2; // Jump.

.float impulse; // Weapon changes.

.float  fixangle;
.vector v_angle;    // View / targeting angle for players.
.float  idealpitch; // Calculated pitch angle for lookup up slopes.

.string netname;

.entity enemy;

.float flags;

.float colormap;
.float team;

.float max_health; // Players maximum health is stored here.

.float teleport_time; // Don't back up.

.float armortype;  // Save this fraction of incoming damage.
.float armorvalue;

.float waterlevel;    // 0 = not in, 1 = feet, 2 = waist, 3 = eyes.
.float watertype;     // A contents value.
.float lastwatertime; // Time of last water transition.

.float ideal_yaw;
.float yaw_speed;

.entity aiment;

.entity goalentity; // A movetarget or an enemy.

.float spawnflags;

.string target;
.string targetname;

// Damage is accumulated through a frame, and sent as one single
// message, so the super shotgun doesn't generate huge messages.
.float  dmg_take;
.float  dmg_save;
.entity dmg_inflictor;

.entity owner;   // Who launched a missile.
.vector movedir; // Mostly for doors, but also used for waterjump.

.string message; // Trigger messages.

.float sounds; // Either a cd track number or sound number.

.string noise, noise1, noise2, noise3; // Contains names of wavs to play.

.vector v_viewangle;

#include "vr_sys_fields.qc"

//
//
//
// ----------------------------------------------------------------------------
// Special flag (!)
// ----------------------------------------------------------------------------

void end_sys_fields; // Flag for structure dumping.

//
//
//
// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

float FALSE = 0;
float TRUE  = 1;

//
//
//
// ----------------------------------------------------------------------------
// Edict `.flags` flag values
// ----------------------------------------------------------------------------

float FL_FLY            = VRUTIL_POWER_OF_TWO(0);
float FL_SWIM           = VRUTIL_POWER_OF_TWO(1);
float FL_CONVEYOR       = VRUTIL_POWER_OF_TWO(2);
float FL_CLIENT         = VRUTIL_POWER_OF_TWO(3);  // Set for all client edicts.
float FL_INWATER        = VRUTIL_POWER_OF_TWO(4);  // For enter / leave water splash.
float FL_MONSTER        = VRUTIL_POWER_OF_TWO(5);
float FL_GODMODE        = VRUTIL_POWER_OF_TWO(6);  // Player cheat.
float FL_NOTARGET       = VRUTIL_POWER_OF_TWO(7);  // Player cheat.
float FL_ITEM           = VRUTIL_POWER_OF_TWO(8);  // Extra wide size for bonus items.
float FL_ONGROUND       = VRUTIL_POWER_OF_TWO(9);  // Standing on something.
float FL_PARTIALGROUND  = VRUTIL_POWER_OF_TWO(10); // Not all corners are valid.
float FL_WATERJUMP      = VRUTIL_POWER_OF_TWO(11); // Player jumping out of water.
float FL_JUMPRELEASED   = VRUTIL_POWER_OF_TWO(12); // For jump debouncing.
float FL_EASYHANDTOUCH  = VRUTIL_POWER_OF_TWO(13); // Adds bonus to boundaries for handtouch.
float FL_SPECIFICDAMAGE = VRUTIL_POWER_OF_TWO(14); // HONEY.

//
//
//
// ----------------------------------------------------------------------------
// Edict `.movetype` values
// ----------------------------------------------------------------------------

float MOVETYPE_NONE           = 0;  // Never moves.
// float MOVETYPE_ANGLENOCLIP = 1;
// float MOVETYPE_ANGLECLIP   = 2;
float MOVETYPE_WALK           = 3;  // Players only.
float MOVETYPE_STEP           = 4;  // Discrete, not real time unless fall.
float MOVETYPE_FLY            = 5;
float MOVETYPE_TOSS           = 6;  // Gravity.
float MOVETYPE_PUSH           = 7;  // No clip to world, push and crush.
float MOVETYPE_NOCLIP         = 8;
float MOVETYPE_FLYMISSILE     = 9;  // Fly with extra size against monsters.
float MOVETYPE_BOUNCE         = 10;
float MOVETYPE_BOUNCEMISSILE  = 11; // Bounce with extra size.

//
//
//
// ----------------------------------------------------------------------------
// Edict `.solid` values
// ----------------------------------------------------------------------------

float SOLID_NOT               = 0; // No interaction with other objects.
float SOLID_TRIGGER           = 1; // Touch on edge, but not blocking.
float SOLID_BBOX              = 2; // Touch on edge, block.
float SOLID_SLIDEBOX          = 3; // Touch on edge, but not an onground.
float SOLID_BSP               = 4; // Bsp clip, touch on edge, block.
float SOLID_NOT_BUT_TOUCHABLE = 5; // Not solid, but can be [hand]touched.

//
//
//
// ----------------------------------------------------------------------------
// Range values
// ----------------------------------------------------------------------------

float RANGE_MELEE = 0;
float RANGE_NEAR  = 1;
float RANGE_MID   = 2;
float RANGE_FAR   = 3;

//
//
//
// ----------------------------------------------------------------------------
// Edict `.deadflag` values
// ----------------------------------------------------------------------------

float DEAD_NO          = 0;
float DEAD_DYING       = 1;
float DEAD_DEAD        = 2;
float DEAD_RESPAWNABLE = 3;

//
//
//
// ----------------------------------------------------------------------------
// Edict `.takedamage` values
// ----------------------------------------------------------------------------

float DAMAGE_NO  = 0;
float DAMAGE_YES = 1;
float DAMAGE_AIM = 2;

//
//
//
// ----------------------------------------------------------------------------
// Edict `.item` flag values
// ----------------------------------------------------------------------------

float IT_FIST             = 0;
float IT_SHOTGUN          = VRUTIL_POWER_OF_TWO(0);
float IT_SUPER_SHOTGUN    = VRUTIL_POWER_OF_TWO(1);
float IT_NAILGUN          = VRUTIL_POWER_OF_TWO(2);
float IT_SUPER_NAILGUN    = VRUTIL_POWER_OF_TWO(3);
float IT_GRENADE_LAUNCHER = VRUTIL_POWER_OF_TWO(4);
float IT_ROCKET_LAUNCHER  = VRUTIL_POWER_OF_TWO(5);
float IT_LIGHTNING        = VRUTIL_POWER_OF_TWO(6);
float IT_MJOLNIR          = VRUTIL_POWER_OF_TWO(7);  // HIPNOTIC.
float IT_SHELLS           = VRUTIL_POWER_OF_TWO(8);
float IT_NAILS            = VRUTIL_POWER_OF_TWO(9);
float IT_ROCKETS          = VRUTIL_POWER_OF_TWO(10);
float IT_CELLS            = VRUTIL_POWER_OF_TWO(11);
float IT_AXE              = VRUTIL_POWER_OF_TWO(12);
float IT_ARMOR1           = VRUTIL_POWER_OF_TWO(13);
float IT_ARMOR2           = VRUTIL_POWER_OF_TWO(14);
float IT_ARMOR3           = VRUTIL_POWER_OF_TWO(15);
float IT_SUPERHEALTH      = VRUTIL_POWER_OF_TWO(16);
float IT_KEY1             = VRUTIL_POWER_OF_TWO(17);
float IT_KEY2             = VRUTIL_POWER_OF_TWO(18);
float IT_INVISIBILITY     = VRUTIL_POWER_OF_TWO(19);
float IT_INVULNERABILITY  = VRUTIL_POWER_OF_TWO(20);
float IT_SUIT             = VRUTIL_POWER_OF_TWO(21);
float IT_QUAD             = VRUTIL_POWER_OF_TWO(22);
float IT_LASER_CANNON     = VRUTIL_POWER_OF_TWO(23); // HIPNOTIC.

//
//
//
// ----------------------------------------------------------------------------
// Edict `.hipnotic_items` flag values
// ----------------------------------------------------------------------------

float HIP_IT_WETSUIT           = VRUTIL_POWER_OF_TWO(1); // HIPNOTIC.
float HIP_IT_EMPATHY_SHIELDS   = VRUTIL_POWER_OF_TWO(2); // HIPNOTIC.
float HIP_IT_HORN_OF_CONJURING = VRUTIL_POWER_OF_TWO(3); // HIPNOTIC.
float HIP_IT_PROXIMITY_GUN     = VRUTIL_POWER_OF_TWO(4); // HIPNOTIC.

//
//
//
// ----------------------------------------------------------------------------
// Edict `.rogue_items` flag values
// ----------------------------------------------------------------------------

float ROGUE_IT_LAVA_NAILGUN       = VRUTIL_POWER_OF_TWO(1);  // ROGUE.
float ROGUE_IT_LAVA_SUPER_NAILGUN = VRUTIL_POWER_OF_TWO(2);  // ROGUE.
float ROGUE_IT_MULTI_GRENADE      = VRUTIL_POWER_OF_TWO(3);  // ROGUE.
float ROGUE_IT_MULTI_ROCKET       = VRUTIL_POWER_OF_TWO(4);  // ROGUE.
float ROGUE_IT_PLASMA_GUN         = VRUTIL_POWER_OF_TWO(5);  // ROGUE.
float ROGUE_IT_GRAPPLE            = VRUTIL_POWER_OF_TWO(6);  // ROGUE.
float ROGUE_IT_LAVA_NAILS         = VRUTIL_POWER_OF_TWO(7);  // ROGUE.
float ROGUE_IT_PLASMA_AMMO        = VRUTIL_POWER_OF_TWO(8);  // ROGUE.
float ROGUE_IT_MULTI_ROCKETS      = VRUTIL_POWER_OF_TWO(9);  // ROGUE.
float ROGUE_IT_SHIELD             = VRUTIL_POWER_OF_TWO(10); // ROGUE.
float ROGUE_IT_ANTIGRAV           = VRUTIL_POWER_OF_TWO(11); // ROGUE.
float ROGUE_IT_EARTHQUAKE         = VRUTIL_POWER_OF_TWO(12); // ROGUE.
float ROGUE_IT_V_SPHERE           = VRUTIL_POWER_OF_TWO(13); // ROGUE.

//
//
//
// ----------------------------------------------------------------------------
// Point content values
// ----------------------------------------------------------------------------

float CONTENT_EMPTY = -1;
float CONTENT_SOLID = -2;
float CONTENT_WATER = -3;
float CONTENT_SLIME = -4;
float CONTENT_LAVA  = -5;
float CONTENT_SKY   = -6;

//
//
//
// ----------------------------------------------------------------------------
// Edict `.state` values
// ----------------------------------------------------------------------------

float STATE_TOP      = 0;
float STATE_BOTTOM   = 1;
float STATE_UP       = 2;
float STATE_DOWN     = 3;
float STATE_FALLING  = 4; // HONEY.
float STATE_DISABLED = 5; // HONEY.

//
//
//
// ----------------------------------------------------------------------------
// Vector constants
// ----------------------------------------------------------------------------

vector VEC_ORIGIN    = '0 0 0';
vector VEC_HULL_MIN  = '-16 -16 -24';
vector VEC_HULL_MAX  = '16 16 32';
vector VEC_HULL2_MIN = '-32 -32 -24';
vector VEC_HULL2_MAX = '32 32 64';

//
//
//
// ----------------------------------------------------------------------------
// Protocol bytes
// ----------------------------------------------------------------------------

float SVC_CDTRACK       = 32;
float SVC_CUTSCENE      = 34; // ROGUE.
float SVC_FINALE        = 31;
float SVC_FOUNDSECRET   = 28;
float SVC_INTERMISSION  = 30;
float SVC_KILLEDMONSTER = 27;
float SVC_SELLSCREEN    = 33;
float SVC_SERVERINFO    = 11; // HONEY.
float SVC_SETANGLES     = 10; // HONEY.
float SVC_SETVIEWPORT   = 5;  // HONEY.
float SVC_TEMPENTITY    = 23;
float SVC_UPDATESTAT    = 3;  // ROGUE, HONEY. [pgm black magic]

float TE_SPIKE        = 0;
float TE_SUPERSPIKE   = 1;
float TE_GUNSHOT      = 2;
float TE_EXPLOSION    = 3;
float TE_TAREXPLOSION = 4;
float TE_LIGHTNING1   = 5;
float TE_LIGHTNING2   = 6;
float TE_WIZSPIKE     = 7;
float TE_KNIGHTSPIKE  = 8;
float TE_LIGHTNING3   = 9;
float TE_LAVASPLASH   = 10;
float TE_TELEPORT     = 11;
float TE_EXPLOSION2   = 12;
float TE_BEAM         = 13;

//
//
//
// ----------------------------------------------------------------------------
// Sound channels
// ----------------------------------------------------------------------------

// Channel 0 never willingly overrides.
// Other channels (1-7) allways override a playing sound on that channel.
float CHAN_AUTO    = 0;
float CHAN_WEAPON  = 1;
float CHAN_VOICE   = 2;
float CHAN_ITEM    = 3;
float CHAN_BODY    = 4;
float CHAN_WEAPON2 = 5;
float CHAN_OTHER   = 6;

float ATTN_NONE   = 0;
float ATTN_NORM   = 1;
float ATTN_IDLE   = 2;
float ATTN_STATIC = 3;

//
//
//
// ----------------------------------------------------------------------------
// Update types
// ----------------------------------------------------------------------------

float UPDATE_GENERAL = 0;
float UPDATE_STATIC  = 1;
float UPDATE_BINARY  = 2;
float UPDATE_TEMP    = 3;

//
//
//
// ----------------------------------------------------------------------------
// Edict `.effects` flag values
// ----------------------------------------------------------------------------

float EF_BRIGHTFIELD  = VRUTIL_POWER_OF_TWO(0);
float EF_MUZZLEFLASH  = VRUTIL_POWER_OF_TWO(1);
float EF_BRIGHTLIGHT  = VRUTIL_POWER_OF_TWO(2);
float EF_DIMLIGHT     = VRUTIL_POWER_OF_TWO(3);
float EF_VERYDIMLIGHT = VRUTIL_POWER_OF_TWO(4);
float EF_MINIROCKET   = VRUTIL_POWER_OF_TWO(5);
float EF_LAVATRAIL    = VRUTIL_POWER_OF_TWO(6);

//
//
//
// ----------------------------------------------------------------------------
// Message types
// ----------------------------------------------------------------------------

float MSG_BROADCAST = 0; // Unreliable to all.
float MSG_ONE       = 1; // Reliable to one (`msg_entity`).
float MSG_ALL       = 2; // Reliable to all.
float MSG_INIT      = 3; // Write to the init string.

//
//
//
// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

float movedist;
float gameover; // Set when a rule exits.

string string_null; // Null string, nothing should be held here.

entity activator; // The entity that activated a trigger or brush.

entity damage_attacker; // Set by `T_Damage`.

float framecount;
float skill;

#include "vr_defs.qc"


//
//
//
// ----------------------------------------------------------------------------
// World fields
// ----------------------------------------------------------------------------

// World fields (FIXME: make globals).
.string wad;
.string map;
.float  worldtype; // 0=medieval, 1=metal, 2=base.

.string killtarget;

//
//
//
// ----------------------------------------------------------------------------
// QuakeED fields
// ----------------------------------------------------------------------------

.float light_lev;        // not used by game, but parsed by light util
.float style;

//
//
//
// ----------------------------------------------------------------------------
// Monster AI
// ----------------------------------------------------------------------------

.void() th_stand;
.void() th_walk;
.void() th_run;
.void() th_missile;
.void() th_melee;

.void(entity attacker, float damage) th_pain;

.void() th_die;

.entity oldenemy; // Mad at this player before taking damage.

.float speed;

.float lefty;

.float search_time;
.float attack_state;

float AS_STRAIGHT = 1;
float AS_SLIDING  = 2;
float AS_MELEE    = 3;
float AS_MISSILE  = 4;

//
//
//
// ----------------------------------------------------------------------------
// Player-only fields
// ----------------------------------------------------------------------------

.float walkframe;

.float attack_finished;
.float pain_finished;

.float invincible_finished;
.float invisible_finished;
.float super_damage_finished;
.float radsuit_finished;

.float invincible_time, invincible_sound;
.float invisible_time, invisible_sound;
.float super_time, super_sound;
.float rad_time;
.float fly_sound;

.float axhitme;

.float show_hostile; // Set to time+0.2 whenever a client fires a
                     // weapon or takes damage. Used to alert
                     // monsters that otherwise would let the player go.

.float  jump_flag;    // Player jump flag.
.float  swim_flag;    // Player swimming sound flag.
.float  air_finished; // When `time > air_finished`, start drowning.
.float  bubble_count; // Keeps track of the number of bubbles.
.string deathtype;    // Keeps track of how the player died.

//
//
//
// ----------------------------------------------------------------------------
// Object stuff
// ----------------------------------------------------------------------------

.string mdl;
.vector mangle; // Angle at start.

.vector oldorigin; // Only used by secret door.

.float  t_length, t_width;

//
//
//
// ----------------------------------------------------------------------------
// Doors, etc
// ----------------------------------------------------------------------------

.vector dest, dest1, dest2;

.float  wait;  // Time from firing to restarting.
.float  delay; // Time from activation to firing.

.entity trigger_field; // Door's trigger entity.
.string noise4;

//
//
//
// ----------------------------------------------------------------------------
// Monsters
// ----------------------------------------------------------------------------

.float  pausetime;
.entity movetarget;

//
//
//
// ----------------------------------------------------------------------------
// Doors
// ----------------------------------------------------------------------------

.float aflag;

.float dmg; // Damage done by door when hit.

//
//
//
// ----------------------------------------------------------------------------
// Misc
// ----------------------------------------------------------------------------

.float cnt; // Misc flag.

//
//
//
// ----------------------------------------------------------------------------
// Subs
// ----------------------------------------------------------------------------

.void() think1;
.vector finaldest, finalangle;

//
//
//
// ----------------------------------------------------------------------------
// Triggers
// ----------------------------------------------------------------------------

.float count; // For counting triggers.

//
//
//
// ----------------------------------------------------------------------------
// Plats / doors / buttons
// ----------------------------------------------------------------------------

.float  lip;
.float  state;
.vector pos1, pos2; // Top and bottom positions.
.float  height;

//
//
//
// ----------------------------------------------------------------------------
// Sounds
// ----------------------------------------------------------------------------

.float waitmin, waitmax;
.float distance;
.float volume;

//
//
//
// ----------------------------------------------------------------------------
// Other non-system fields
// ----------------------------------------------------------------------------

#include "vr_fields.qc"

//
//
//
// ----------------------------------------------------------------------------
// Builtins
// ----------------------------------------------------------------------------

#include "builtins.qc"
