//================================================
// constants
//================================================

string version = "Copper Quake 1.05 - 16 July 2019\n";

float	FALSE					= 0;
float 	TRUE					= 1;

// edict.flags
float	FL_FLY					= 1;
float	FL_SWIM					= 2;
float	FL_CLIENT				= 8;	// set for all client edicts
float	FL_INWATER				= 16;	// for enter / leave water splash
float	FL_MONSTER				= 32;
float	FL_GODMODE				= 64;	// player cheat
float	FL_NOTARGET				= 128;	// player cheat
float	FL_ITEM					= 256;	// extra wide size for bonus items
float	FL_ONGROUND				= 512;	// standing on something
float	FL_PARTIALGROUND		= 1024;	// not all corners are valid
float	FL_WATERJUMP			= 2048;	// player jumping out of water
float	FL_JUMPRELEASED			= 4096;	// for jump debouncing

// edict.customflags
float	CFL_LEFTY			= 1;	// used by slidemovers (scrags) to alternate strafe direction
float	CFL_ZAPPED			= 2;
float	CFL_AXEHITME		= 4;
float	CFL_LOCKED			= 16;	// special state-independent deactivation
float	CFL_INPAIN			= 16;	// zombie behavior
float	CFL_KNOCKEDDOWN		= 32;	// zombie behavior
float	CFL_PLUNGE			= 64;	// has fallen in a death pit
float	CFL_SPAWNER			= 128;	// is a generator of monsters
float	CFL_UNDYING			= 256;	// god mode minus one point (player)
float	CFL_LIQUID_IMMUNE	= 512;	// monster spawned touching slime or lava, so assume designer wants it immune to damage
float	CFL_MELEEONLY		= 1024;	// has no ranged attack (monster)
// serverflags
float	SVFL_EPISODE1		= 1;
float	SVFL_EPISODE2		= 2;
float	SVFL_EPISODE3		= 4;
float	SVFL_EPISODE4		= 8;
float	SVFL_ALLEPISODES	= 15;
float	SVFL_RESPAWNING		= 2097152;	// to notify target_autosave that we just restarted after dying



// edict.movetype values
float	MOVETYPE_NONE			= 0;	// never moves
//float	MOVETYPE_ANGLENOCLIP	= 1;
//float	MOVETYPE_ANGLECLIP		= 2;
float	MOVETYPE_WALK			= 3;	// players only
float	MOVETYPE_STEP			= 4;	// discrete, not real time unless fall
float	MOVETYPE_FLY			= 5;
float	MOVETYPE_TOSS			= 6;	// gravity
float	MOVETYPE_PUSH			= 7;	// no clip to world, push and crush
float	MOVETYPE_NOCLIP			= 8;
float	MOVETYPE_FLYMISSILE		= 9;	// fly with extra size against monsters
float	MOVETYPE_BOUNCE			= 10;
float	MOVETYPE_BOUNCEMISSILE	= 11;	// bounce with extra size

// edict.solid values
float	SOLID_NOT				= 0;	// no interaction with other objects
float	SOLID_TRIGGER			= 1;	// touch on edge, but not blocking
float	SOLID_BBOX				= 2;	// touch on edge, block
float	SOLID_SLIDEBOX			= 3;	// touch on edge, but not an onground
float	SOLID_BSP				= 4;	// bsp clip, touch on edge, block

// range values
float	RANGE_MELEE				= 0;	// 0-120
float	RANGE_NEAR				= 1;	// 120-500
float	RANGE_MID				= 2;	// 500-900, max ranged attack dist
float	RANGE_FAR				= 3;	// 900-1250, max awake dist
float	RANGE_TOOFAR			= 4;	// 1250+

// deadflag values
float	DEAD_NO					= 0;
float	DEAD_DYING				= 1;
float	DEAD_DEAD				= 2;
float	DEAD_RESPAWNABLE		= 3;

// takedamage values
float	DAMAGE_NO				= 0;
float	DAMAGE_YES				= 1;
float	DAMAGE_AIM				= 2;

float	TRACE_NOMONSTERS		= 1;
float	TRACE_WATER				= 2;

// monsters
float	SPAWN_AMBUSH		= 1;
float	SPAWN_CRUCIFIEDX	= 1;
/*
float	SPAWN_TRIGGERED		= 2;
float	SPAWN_TFOG			= 4;
float	SPAWN_NO_TFRAG		= 8;
float	SPAWN_INSTAWAKE		= 16;
float	SPAWN_LYING			= 32;	// zombies
*/
float	SPAWN_TRIGGERED		= 16;
float	SPAWN_NO_TFOG		= 32;
float	SPAWN_NO_TFRAG		= 64;
float	SPAWN_INSTAWAKE		= 128;
float	SPAWN_LYING			= 2;	// zombies
float	SPAWN_CRUCIFIED		= 16384;	// was 128, but random zombies in stock quake have spawnflags&128 left over from something :P

// mode flags
float	SPAWN_NOTEASY		= 256;
float	SPAWN_NOTNORMAL		= 512;
float	SPAWN_NOTHARD		= 1024;
float	SPAWN_NOTDM			= 2048;
float	SPAWN_COOPONLY		= 4096;
float	SPAWN_NOTCOOP		= 8192;

// items
float	IT_AXE					= 4096;
float	IT_SHOTGUN				= 1;
float	IT_SUPER_SHOTGUN		= 2;
float	IT_NAILGUN				= 4;
float	IT_SUPER_NAILGUN		= 8;
float	IT_GRENADE_LAUNCHER		= 16;
float	IT_ROCKET_LAUNCHER		= 32;
float	IT_LIGHTNING			= 64;
float	IT_WEAPONS				= 127 + 4096;

float	IT_SHELLS				= 256;
float	IT_NAILS				= 512;
float	IT_ROCKETS				= 1024;
float	IT_CELLS				= 2048;

float	IT_ARMOR1				= 8192;
float	IT_ARMOR2				= 16384;
float	IT_ARMOR3				= 32768;
//float	IT_SUPERHEALTH			= 65536;	// never gets used now, but the .exe expects the following flags

float	IT_KEY1					= 131072;
float	IT_KEY2					= 262144;

float	IT_INVISIBILITY			= 524288;
float	IT_INVULNERABILITY		= 1048576;
float	IT_SUIT					= 2097152;
float	IT_QUAD					= 4194304;
float	IT_POWERUPS				= IT_INVISIBILITY + IT_INVULNERABILITY + IT_SUIT + IT_QUAD;


// point content values
float	CONTENT_EMPTY		= -1;
float	CONTENT_SOLID		= -2;
float	CONTENT_WATER		= -3;
float	CONTENT_SLIME		= -4;
float	CONTENT_LAVA		= -5;
float	CONTENT_SKY			= -6;

float	WL_NONE				= 0;
float	WL_FEET				= 1;
float	WL_WAIST			= 2;
float	WL_EYES				= 3;


// movers
float	STATE_TOP			= 0;
float	STATE_BOTTOM		= 1;
float	STATE_UP			= 2;
float	STATE_DOWN			= 3;

float	STATE_OFF			= 0;
float	STATE_ON			= 1;



float	START_OFF			= 1;


// geometry
vector	VEC_ORIGIN		= '0 0 0';
vector	VEC_HULL_MIN	= '-16 -16 -24';
vector	VEC_HULL_MAX	= '16 16 32';

vector	VEC_HULL2_MIN	= '-32 -32 -24';
vector	VEC_HULL2_MAX	= '32 32 64';

vector	VEC_ONE			= '1 1 1';
vector	VEC_UP			= '0 0 1';
vector	VEC_DOWN 		= '0 0 -1';

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) always override a playing sound on that channel
float	CHAN_AUTO		= 0;
float	CHAN_WEAPON		= 1;
float	CHAN_VOICE		= 2;
float	CHAN_ITEM		= 3;
float	CHAN_BODY		= 4;
float	CHAN_OTHER		= 5;

float	ATTN_NONE		= 0;
float	ATTN_NORM		= 1;
float	ATTN_IDLE		= 2;
float	ATTN_STATIC		= 3;

// entity effects
float	EF_BRIGHTFIELD	= 1;
float	EF_MUZZLEFLASH 	= 2;
float	EF_BRIGHTLIGHT 	= 4;
float	EF_DIMLIGHT 	= 8;

// attack states
float	AS_STRAIGHT		= 1;
float	AS_SLIDING		= 2;
float	AS_MELEE		= 3;
float	AS_MISSILE		= 4;
float	AS_LEAP			= 5;


// protocol bytes
float	SVC_UPDATESTAT		= 3;
float	SVC_SETVIEW			= 5;
float	SVC_TEMPENTITY		= 23;
float	SVC_CENTERPRINT		= 26;
float	SVC_KILLEDMONSTER	= 27;
float	SVC_FOUNDSECRET		= 28;
float	SVC_INTERMISSION	= 30;
float	SVC_FINALE			= 31;
float	SVC_CDTRACK			= 32;
float	SVC_SELLSCREEN		= 33;
float	SVC_CUTSCENE		= 34;
float	SVC_FOG				= 41;	// in fitzquake, byte byte byte byte short for density r g b lerptime

// svc updatestat byte types
float	STAT_HEALTH			= 0;
float	STAT_FRAGS			= 1;
float	STAT_WEAPON			= 2;
float	STAT_AMMO			= 3;
float	STAT_ARMOR			= 4;
float	STAT_WEAPONFRAME	= 5;
float	STAT_SHELLS			= 6;
float	STAT_NAILS			= 7;
float	STAT_ROCKETS		= 8;
float	STAT_CELLS			= 9;
float	STAT_ACTIVEWEAPON	= 10;
float	STAT_TOTALSECRETS	= 11;	// hmmm ...
float	STAT_TOTALMONSTERS	= 12;	// must send this if new monsters are spawned after the map starts
float	STAT_SECRETS		= 13;	// bumped on client side by svc_foundsecret
float	STAT_MONSTERS		= 14;	// bumped by svc_killedmonster

// temporary entities
float	TE_SPIKE		= 0;
float	TE_SUPERSPIKE	= 1;
float	TE_GUNSHOT		= 2;
float	TE_EXPLOSION	= 3;
float	TE_TAREXPLOSION	= 4;
float	TE_LIGHTNING1	= 5;
float	TE_LIGHTNING2	= 6;
float	TE_WIZSPIKE		= 7;
float	TE_KNIGHTSPIKE	= 8;
float	TE_LIGHTNING3	= 9;
float	TE_LAVASPLASH	= 10;
float	TE_TELEPORT		= 11;
float	TE_EXPLOSION2	= 12;
float	TE_BEAM			= 13;

// update types
float	UPDATE_GENERAL	= 0;
float	UPDATE_STATIC	= 1;
float	UPDATE_BINARY	= 2;
float	UPDATE_TEMP		= 3;

// messages
float	MSG_BROADCAST	= 0;		// unreliable to all
float	MSG_ONE			= 1;		// reliable to one (msg_entity)
float	MSG_ALL			= 2;		// reliable to all
float	MSG_INIT		= 3;		// write to the init string


// misc
float	A_SHITLOAD		= 9999999;
float	AUTOAIM_DIST	= 2048;

float	DEBUG_DMG_PER_ROCKET	= 180;
float	DEBUG_DMG_PER_NAIL		= 9;
float	DEBUG_DMG_PER_SHELL		= 24;
float	DEBUG_DMG_PER_CELL		= 30;

float	MAX_AMMO_SHELLS			= 100;
float	MAX_AMMO_NAILS			= 200;
float	MAX_AMMO_ROCKETS		= 100;
float	MAX_AMMO_CELLS			= 100;

//================================================
