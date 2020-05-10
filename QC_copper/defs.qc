/*
==============================================================================

			SOURCE FOR GLOBALVARS_T C STRUCTURE

==============================================================================
*/

//
// system globals
//
entity		self;
entity		other;
entity		world;
float		time;
float		frametime;

float		force_retouch;		// force all entities to touch triggers
								// next frame.  this is needed because
								// non-moving things don't normally scan
								// for triggers, and when a trigger is
								// created (like a teleport trigger), it
								// needs to catch everything.
								// decremented each frame, so set to 2
								// to guarantee everything is touched
string		mapname;

float		deathmatch;
float		coop;
float		teamplay;

float		serverflags;		// propagated from level to level, used to
								// keep track of completed episodes

float		total_secrets;
float		total_monsters;

float		found_secrets;		// number of secrets found
float		killed_monsters;	// number of monsters killed


// spawnparms are used to encode information about clients across server level changes
float		parm1, parm2, parm3, parm4, parm5, parm6, parm7, parm8, parm9, parm10, parm11, parm12, parm13, parm14, parm15, parm16;

// global variables set by built in functions
vector		v_forward, v_up, v_right;	// set by makevectors()
	
// set by traceline / tracebox
float		trace_allsolid;
float		trace_startsolid;
float		trace_fraction;
vector		trace_endpos;
vector		trace_plane_normal;
float		trace_plane_dist;
entity		trace_ent;
float		trace_inopen;
float		trace_inwater;

entity		msg_entity;				// destination of single entity writes

// required prog functions
void() 		main;						// only for testing

void()		StartFrame;

void() 		PlayerPreThink;
void() 		PlayerPostThink;

void()		ClientKill;
void()		ClientConnect;
void() 		PutClientInServer;		// call after setting the parm1... parms
void()		ClientDisconnect;

void()		SetNewParms;			// called when a client first connects to
									// a server. sets parms so they can be
									// saved off for restarts

void()		SetChangeParms;			// call to set parms for self so they can
									// be saved for a level transition


//================================================
void		end_sys_globals;		// flag for structure dumping
//================================================

/*
==============================================================================

			SOURCE FOR ENTVARS_T C STRUCTURE

==============================================================================
*/

//
// system fields (*** = do not set in prog code, maintained by C code)
//
.float		modelindex;		// *** model index in the precached list
.vector		absmin, absmax;	// *** origin + mins / maxs

.float		ltime;			// local time for entity
.float		movetype;
.float		solid;

.vector		origin;			// ***
.vector		oldorigin;		// ***
.vector		velocity;
.vector		angles;
.vector		avelocity;

.vector		punchangle;		// temp angle adjust from damage or recoil

.string		classname;		// spawn function
.string		model;
.float		frame;
.float		skin;
.float		effects;

.vector		mins, maxs;		// bounding box extents reletive to origin
.vector		size;			// maxs - mins

.void()		touch;
.void()		use;
.void()		think;
.void()		blocked;		// for doors or plats, called when can't push other

.float		nextthink;
.entity		groundentity;

// stats
.float		health;
.float		frags;
.float		weapon;			// one of the IT_SHOTGUN, etc flags
.string		weaponmodel;
.float		weaponframe;
.float		currentammo;
.float		ammo_shells, ammo_nails, ammo_rockets, ammo_cells;

.float		items;			// bit flags

.float		takedamage;
.entity		chain;
.float		deadflag;

.vector		view_ofs;			// add to origin to get eye point

.float		button0;		// fire
.float		button1;		// use
.float		button2;		// jump

.float		impulse;		// weapon changes

.float		fixangle;
.vector		v_angle;		// view / targeting angle for players
.float		idealpitch;		// calculated pitch angle for lookup up slopes


.string		netname;

.entity 	enemy;

.float		flags;

.float		colormap;
.float		team;

.float		max_health;

.float		teleport_time;	// don't back up

.float		armortype;		// save this fraction of incoming damage
.float		armorvalue;

.float		waterlevel;		// 0 = not in, 1 = feet, 2 = wast, 3 = eyes, 4 = entire bbox
.float		watertype;		// a contents value

.float		ideal_yaw;
.float		yaw_speed;

.entity		aiment;

.entity 	goalentity;		// a movetarget or an enemy

.float		spawnflags;

.string		target;
.string		targetname;

// damage is accumulated through a frame. and sent as one single
// message, so the super shotgun doesn't generate huge messages
.float		dmg_take;
.float		dmg_save;
.entity		dmg_inflictor;

.entity		owner;		// who launched a missile
.vector		movedir;	// mostly for doors, but also used for waterjump

.string		message;	// trigger messages

.float		sounds;		// either a cd track number or sound number

.string		noise, noise1, noise2, noise3;	// contains names of wavs to play

//================================================
void		end_sys_fields;			// flag for structure dumping
//================================================

/*
==============================================================================

				VARS NOT REFERENCED BY C CODE

==============================================================================
*/

.void()		use;

// globals

float	gameover;		// set when a rule exits

string	nextmap;		// used by changelevel code in client.qc - 'mapname' is current map

string	string_null;	// null string, nothing should be held here

entity	activator;		// the entity that activated a trigger or brush

entity	damage_attacker;	// set by T_Damage

float	framecount;
float	reset_flag;
float	startingserverflags;
nosave float	gamestarted;
nosave float	cleanUpClientShit;
nosave float	clients;

string	autosavename;
// this is not 'nosave' so that if you die, then autoreload from the autosave, then 
// die again, the autosave remembers that you autosaved

float	skill;

// for balancing: total monster HP/ammo -HP/zombies/grenades in the level
float	monster_health;	
float	ammo_total;
float	num_zombies;
float	num_rockets;

float	trace_debug;

// moved from ai.qc or maybe combat.qc - used for target finding
float	enemy_vis, enemy_range;//, enemy_infront;
float	enemy_yaw();

// when a monster becomes angry at a player, that monster will be used
// as the sight target the next frame so that monsters near that one
// will wake up even if they wouldn't have noticed the player
entity	sight_entity;
float	sight_entity_time;

// moved from world.qc
entity	lastspawn, bodyqueue_head;

// moved from client.qc
float	intermission_running, intermission_exittime;
float	modelindex_eyes, modelindex_player;

float	icantsee;

//================================================

// world fields
.string		wad;
.string 	map;
.float		worldtype;	// 0=medieval 1=metal 2=base

.string		killtarget;
.string		target2;
.string		target3;
.string		target4;
//.string		pretarget;		// to be fired before ... things
//.string		posttarget;		// to be fired after them

.float		alpha;

// lights
.float		style;
.float		oldstyle;

// monster ai
.void()		th_stand;
.void()		th_walk;
.void()		th_run;
.void()		th_missile;
.void()		th_melee;
.void(entity attacker, float damage) th_pain;
.void()		th_die;
.float()	th_checkattack;
.entity		oldenemy;		// mad at this player before taking damage
.float 		attack_finished;
.float		pain_finished;
.float		search_time;
.float		attack_state;
.float 		pausetime;

// player only fields
.float		walkframe;
.float		invincible_time, invincible_sound, invincible_finished;
.float		invisible_time, invisible_sound, invisible_finished;
.float		super_time, super_sound, super_damage_finished;
.float		rad_time, radsuit_finished;
.float		nextmovesound;	// replaces swim_flag and fly_sound, both of which were timers
.float		show_hostile;	// set to time+0.2 whenever a client fires a
							// weapon or takes damage.  Used to alert
							// monsters that otherwise would let the player go
.float		jump_flag;		// player jump "flag"
.float		air_finished;	// when time > air_finished, start drowning
.float		bubble_count;	// keeps track of the number of bubbles
.float		healthtime;
.string		deathtype;		// keeps track of how the player died

// fog
.vector		fog_color, fog_color2;
.float		fog_density, fog_density2;


// object stuff
//.string		mdl;			// only used by items to hide until respawn time
.float		gravity;
.vector		mangle;			// angle at start

// doors/movers
.vector		dest, dest1, dest2;
.vector		pos1, pos2;		// top and bottom positions
//.vector		oldorigin;		// only used by secret door
.vector		finaldest, finalangle;
.void()		think1;
.float		speed, speed2;
.float		wait;			// time from firing to restarting
.float		delay;			// time from activation to firing
.float		length, width, height;
.float t_length = length;	// preserve old func_door_secret maphacks
.float t_width = width;


.float		touch_time;
.float		lip;			
.float		state;
.float		dmg;			// damage done by door when hit
.entity		trigger_field;	// door's trigger entity
.string		noise4;

// triggers
.float		count;		// for counting triggers
.float		cnt;		// used on grunts, enforcers, dguards, and ogres for attack refire
						// used on nouronihar for attack patterning
						// reused on players for nailgun barrel cycling

// sounds
.float		distance;
.float		volume;

//.float	inpain;			// from zombie.qc, moved to .customflags
//.float	lefty;			// moved to .customflags
//.float	axhitme;		// moved to .customflags
//.float	zapped;			// moved to .customflags
.float		customflags;	// inpain, axehitme, zapped, etc, see constants.qc
.float		healamount;		// moved from items.qc - only used on health boxes
.float		dmgtime;		// moved from client.qc - used in drowning code & as a coop aggro timer
.entity		multidmg_chain;	// moved from combat.qc - for multidmg	
.float		multi_dmg;

.string		type;			// to make find()ing easier, and for tons of useful misc. classification
.entity		buddy;			// a common entity reference without the specific implications of 'owner' or 'enemy'
.float		strength;

.float		lifetime_finished;
.float		notrace;		// entity will be invisible to traceline2()
.vector		oldvelocity;	// necessary for getting projectiles to pass through .notrace entities
.entity		trueowner;		// now this is getting dumb

// OVERLOADS:
.float rand = healamount;	// for trigger_timer
.float svflags = jump_flag;	// for target_give and item_sigil
.vector fadecolor = fog_color;	// for func_void fadeout
.string	include = wad;		// for monsterjump filters
.string exclude = map;
.float light_lev = bubble_count;	// shuts up a stupid stupid warning

//===========================================================================
// builtin functions
//===========================================================================

void(vector ang)	makevectors		= #1;		// sets v_forward, etc globals
void(entity e, vector o) setorigin	= #2;
void(entity e, string m) setmodel	= #3;		// set movetype and solid first
void(entity e, vector min, vector max) setsize = #4;
// #5 was removed
void() break						= #6;
float() random						= #7;		// returns 0 - 1
void(entity e, float chan, string samp, float vol, float atten) sound = #8;
vector(vector v) normalize			= #9;
void(string e) error				= #10;
void(string e) objerror				= #11;
float(vector v) vlen				= #12;
float(vector v) vectoyaw			= #13;
entity() spawn						= #14;
void(entity e) remove				= #15;

// sets trace_* globals
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
void(vector v1, vector v2, float nomonsters, entity forent) traceline = #16;	
// fun fact for future self: 'nomonsters' actually means no entities. the next time you 
// go out of your motherfucking mind trying to trace against an entity that isn't a monster 
// and get world every time, this is why.

entity() checkclient				= #17;	// returns a client to look for
entity(entity start, .string fld, string match) find = #18;
string(string s) precache_sound		= #19;
string(string s) precache_model		= #20;
void(entity client, string s)stuffcmd = #21;
entity(vector org, float rad) findradius = #22; // findradius only works on solid entities
void(string s) bprint				= #23;
void(entity client, string s) sprint = #24;
void(string s) dprint				= #25;
string(float f) ftos				= #26;
string(vector v) vtos				= #27;
void() coredump						= #28;		// prints all edicts
void() traceon						= #29;		// turns statement trace on
void() traceoff						= #30;
void(entity e) eprint				= #31;		// prints an entire edict
float(float yaw, float dist) walkmove	= #32;	// returns TRUE or FALSE
// #33 was removed
float(float yaw, float dist) droptofloor = #34;	// TRUE if landed on floor
void(float style, string value) lightstyle = #35;
float(float v) rint					= #36;		// round to nearest int
float(float v) floor				= #37;		// largest integer <= v
float(float v) ceil					= #38;		// smallest integer >= v
// #39 was removed

float(entity e) checkbottom			= #40;		// true if self is on ground
// "Returns false if any part of the bottom of the entity is off an edge that is not a staircase."

float(vector v) pointcontents		= #41;		// returns a CONTENT_*
// #42 was removed
float(float f) fabs = #43;
vector(entity e, float speed) aim = #44;		// returns the shooting vector
float(string s) cvar = #45;						// return cvar.value
void(string s) localcmd = #46;					// put string into local que
entity(entity e) nextent = #47;					// for looping through all ents
void(vector org, vector dir, float color, float count) particle = #48;// start a particle effect
void() ChangeYaw = #49;						// turn towards self.ideal_yaw
											// at self.yaw_speed
// #50 was removed
vector(vector v) vectoangles			= #51;

// direct client message generation
void(float to, float f) WriteByte		= #52;
void(float to, float f) WriteChar		= #53;
void(float to, float f) WriteShort		= #54;
void(float to, float f) WriteLong		= #55;
void(float to, float f) WriteCoord		= #56;
void(float to, float f) WriteAngle		= #57;
void(float to, string s) WriteString	= #58;
void(float to, entity s) WriteEntity	= #59;

// broadcast client message generation
// void(float f) bWriteByte		= #59;
// void(float f) bWriteChar		= #60;
// void(float f) bWriteShort		= #61;
// void(float f) bWriteLong		= #62;
// void(float f) bWriteCoord		= #63;
// void(float f) bWriteAngle		= #64;
// void(string s) bWriteString	= #65;
// void(entity e) bWriteEntity = #66;

void(float step) movetogoal				= #67;

string(string s) precache_file		= #68;	// no effect except for -copy
void(entity e) makestatic		= #69;
void(string s) changelevel = #70;

//#71 was removed

void(string var, string val) cvar_set = #72;	// sets cvar.value

void(entity client, string s) centerprint = #73;	// sprint, but in middle
void(entity client, string s, string s) centerprint2 = #73;	// sprint, but in middle
void(entity client, string s, string s, string s) centerprint3 = #73;	// sprint, but in middle
void(entity client, string s, string s, string s, string s) centerprint4 = #73;	// sprint, but in middle
void(entity client, string s, string s, string s, string s, string s) centerprint5 = #73;	// sprint, but in middle

void(vector pos, string samp, float vol, float atten) ambientsound = #74;

string(string s) precache_model2	= #75;		// registered version only
string(string s) precache_sound2	= #76;		// registered version only
string(string s) precache_file2		= #77;		// registered version only

void(entity e) setspawnparms		= #78;		// set parm1... to the
												// values at level start
												// for coop respawn


//================================================

/*FGD
@baseclass = Angle [ angle(integer) : "Direction" ]
@baseclass = Appearflags [
	spawnflags(Flags) =
	[
		256 : "Not on Easy" : 0
		512 : "Not on Normal" : 0
		1024 : "Not on Hard+" : 0
		2048 : "Not in Deathmatch" : 0
		4096 : "Only in Coop" : 0
		8192 : "Not in Coop" : 0
	]
]

@baseclass = Targetname [ targetname(target_source) : "Name" ]
@baseclass = Target [ 
	target(target_destination) : "Target" 
	target2(target_destination) : "Target" 
	target3(target_destination) : "Target" 
	target4(target_destination) : "Target" 
	killtarget(target_destination) : "Killtarget"
]

@baseclass = Deathtype [ deathtype(string) : "Obituary message" ]
@baseclass = Bounds [ mangle(string) : "Bounds size, centered (x y z)" ]

@baseclass = Phong [
	_phong(integer) : "Use phong smoothing" : 0
	_phong_angle(integer) : "Phong angle" : 88
]
*/