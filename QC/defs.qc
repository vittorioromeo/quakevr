//
// system globals
//
entity        self;
entity        other;
entity        world;
float        time;
float        frametime;

float        force_retouch;        // force all entities to touch triggers
                                // next frame.  this is needed because
                                // non-moving things don't normally scan
                                // for triggers, and when a trigger is
                                // created (like a teleport trigger), it
                                // needs to catch everything.
                                // decremented each frame, so set to 2
                                // to guarantee everything is touched
string        mapname;

float        deathmatch;
float        coop;
float        teamplay;

float        serverflags;        // propagated from level to level, used to
                                // keep track of completed episodes

float        total_secrets;
float        total_monsters;

float        found_secrets;        // number of secrets found
float        killed_monsters;    // number of monsters killed


// spawnparms are used to encode information about clients across server
// level changes
float parm1;  // Spawnparm for `.items`.
float parm2;  // Spawnparm for `.health`.
float parm3;  // Spawnparm for `.armorvalue`.
float parm4;  // Spawnparm for `.ammo_shells`.
float parm5;  // Spawnparm for `.ammo_nails`.
float parm6;  // Spawnparm for `.ammo_rockets`.
float parm7;  // Spawnparm for `.ammo_cells`.
float parm8;  // Spawnparm for `.weapon`.
float parm9;  // Spawnparm for `.armortype`.
float parm10; // Spawnparm for `.weapon2`.
float parm11; // Spawnparm for `.holsterweapon0`.
float parm12; // Spawnparm for `.holsterweapon1`.
float parm13; // Spawnparm for `.holsterweapon2`.
float parm14; // Spawnparm for `.holsterweapon3`.
float parm15; // Spawnparm for `.holsterweapon4`.
float parm16; // Spawnparm for `.holsterweapon5`.

//
// global variables set by built in functions
//
vector        v_forward, v_up, v_right;    // set by makevectors()

// set by traceline / tracebox
float        trace_allsolid;
float        trace_startsolid;
float        trace_fraction;
vector        trace_endpos;
vector        trace_plane_normal;
float        trace_plane_dist;
entity        trace_ent;
float        trace_inopen;
float        trace_inwater;

entity        msg_entity;                // destination of single entity writes

//
// required prog functions
//
void()         main;                        // only for testing

void()        StartFrame;

void()         PlayerPreThink;
void()         PlayerPostThink;

void()        ClientKill;
void()        ClientConnect;
void()         PutClientInServer;        // call after setting the parm1... parms
void()        ClientDisconnect;

void()        SetNewParms;            // called when a client first connects to
                                    // a server. sets parms so they can be
                                    // saved off for restarts

void()        SetChangeParms;            // call to set parms for self so they can
                                    // be saved for a level transition


//================================================
void        end_sys_globals;        // flag for structure dumping
//================================================

/*
==============================================================================

            SOURCE FOR ENTVARS_T C STRUCTURE

==============================================================================
*/

//
// system fields (*** = do not set in prog code, maintained by C code)
//
.float        modelindex;        // *** model index in the precached list
.vector        absmin, absmax;    // *** origin + mins / maxs

.float        ltime;            // local time for entity
.float        movetype;
.float        solid;

.vector        origin;            // ***
.vector        oldorigin;        // ***
.vector        velocity;
.vector        angles;
.vector        avelocity;

.vector        punchangle;        // temp angle adjust from damage or recoil

.string        classname;        // spawn function
.string        model;
.float        frame;
.float        skin;
.float        effects;

.vector        mins, maxs;        // bounding box extents reletive to origin
.vector        size;            // maxs - mins

.void()        touch;
.void()        use;
.void()        think;
.void()        blocked;        // for doors or plats, called when can't push other

.float        nextthink;
.entity        groundentity;

// stats
.float        health;
.float        frags;

.float        weapon;             // WID
.string       weaponmodel;
.float        weaponframe;

.float        currentammo;

.float        ammocounter;

.float        ammo_shells, ammo_nails, ammo_rockets, ammo_cells;

.float        items;            // bit flags

.float        takedamage;
.entity        chain;
.float        deadflag;

.vector        view_ofs;            // add to origin to get eye point


.float        button0;        // fire
.float        button1;        // use
.float        button2;        // jump

.float        impulse;        // weapon changes

.float        fixangle;
.vector        v_angle;        // view / targeting angle for players
.float        idealpitch;        // calculated pitch angle for lookup up slopes


.string        netname;

.entity     enemy;

.float        flags;

.float        colormap;
.float        team;

.float        max_health;        // players maximum health is stored here

.float        teleport_time;    // don't back up

.float        armortype;        // save this fraction of incoming damage
.float        armorvalue;

.float        waterlevel;        // 0 = not in, 1 = feet, 2 = wast, 3 = eyes
.float        watertype;         // a contents value
.float        lastwatertime;     // time of last water transition

.float        ideal_yaw;
.float        yaw_speed;

.entity        aiment;

.entity     goalentity;        // a movetarget or an enemy

.float        spawnflags;

.string        target;
.string        targetname;

// damage is accumulated through a frame. and sent as one single
// message, so the super shotgun doesn't generate huge messages
.float        dmg_take;
.float        dmg_save;
.entity        dmg_inflictor;

.entity        owner;        // who launched a missile
.vector        movedir;    // mostly for doors, but also used for waterjump

.string        message;        // trigger messages

.float        sounds;        // either a cd track number or sound number

.string        noise, noise1, noise2, noise3;    // contains names of wavs to play

.vector        v_viewangle;

#include "vrfields.qc"

//================================================
void        end_sys_fields;            // flag for structure dumping
//================================================

/*
==============================================================================

                VARS NOT REFERENCED BY C CODE

==============================================================================
*/


//
// constants
//
float    FALSE                    = 0;
float     TRUE                    = 1;

// edict.flags
float    FL_FLY                   = 1;
float    FL_SWIM                  = 2;
float    FL_CLIENT                = 8;    // set for all client edicts
float    FL_INWATER               = 16;   // for enter / leave water splash
float    FL_MONSTER               = 32;
float    FL_GODMODE               = 64;   // player cheat
float    FL_NOTARGET              = 128;  // player cheat
float    FL_ITEM                  = 256;  // extra wide size for bonus items
float    FL_ONGROUND              = 512;  // standing on something
float    FL_PARTIALGROUND         = 1024; // not all corners are valid
float    FL_WATERJUMP             = 2048; // player jumping out of water
float    FL_JUMPRELEASED          = 4096; // for jump debouncing
float    FL_EASYHANDTOUCH         = 8192; // adds bonus to boundaries for handtouch

// edict.movetype values
float    MOVETYPE_NONE            = 0;    // never moves
// float MOVETYPE_ANGLENOCLIP     = 1;
// float MOVETYPE_ANGLECLIP       = 2;
float    MOVETYPE_WALK            = 3;    // players only
float    MOVETYPE_STEP            = 4;    // discrete, not real time unless fall
float    MOVETYPE_FLY             = 5;
float    MOVETYPE_TOSS            = 6;    // gravity
float    MOVETYPE_PUSH            = 7;    // no clip to world, push and crush
float    MOVETYPE_NOCLIP          = 8;
float    MOVETYPE_FLYMISSILE      = 9;    // fly with extra size against monsters
float    MOVETYPE_BOUNCE          = 10;
float    MOVETYPE_BOUNCEMISSILE   = 11;   // bounce with extra size

// edict.solid values
float    SOLID_NOT                = 0;    // no interaction with other objects
float    SOLID_TRIGGER            = 1;    // touch on edge, but not blocking
float    SOLID_BBOX               = 2;    // touch on edge, block
float    SOLID_SLIDEBOX           = 3;    // touch on edge, but not an onground
float    SOLID_BSP                = 4;    // bsp clip, touch on edge, block
float    SOLID_NOT_BUT_TOUCHABLE  = 5;    // not solid, but can be [hand]touched

// range values
float    RANGE_MELEE              = 0;
float    RANGE_NEAR               = 1;
float    RANGE_MID                = 2;
float    RANGE_FAR                = 3;

// deadflag values

float    DEAD_NO                  = 0;
float    DEAD_DYING               = 1;
float    DEAD_DEAD                = 2;
float    DEAD_RESPAWNABLE         = 3;

// takedamage values

float    DAMAGE_NO                = 0;
float    DAMAGE_YES               = 1;
float    DAMAGE_AIM               = 2;

// items
// TODO VR: (P2) decide what to do with these, as they are not enough
float    IT_FIST                  = 0;
float    IT_SHOTGUN               = 1;
float    IT_SUPER_SHOTGUN         = 2;
float    IT_NAILGUN               = 4;
float    IT_SUPER_NAILGUN         = 8;
float    IT_GRENADE_LAUNCHER      = 16;
float    IT_ROCKET_LAUNCHER       = 32;
float    IT_LIGHTNING             = 64;
float    IT_MJOLNIR               = 128;
float    IT_SHELLS                = 256;
float    IT_NAILS                 = 512;
float    IT_ROCKETS               = 1024;
float    IT_CELLS                 = 2048;
float    IT_AXE                   = 4096;

float    IT_ARMOR1                = 8192;
float    IT_ARMOR2                = 16384;
float    IT_ARMOR3                = 32768;
// TODO VR: (P1) deal with this, hipnotic comments the line below
//MED 11/02/96 added new hip weapon removed SUPERHEALTH
float    IT_SUPERHEALTH           = 65536;
// TODO VR: (P1) this conflicts with super health, grabbing super health gives
// the player proximity gun
float    IT_PROXIMITY_GUN         = 65536;

float    IT_KEY1                  = 131072;
float    IT_KEY2                  = 262144;

float    IT_INVISIBILITY          = 524288;
float    IT_INVULNERABILITY       = 1048576;
float    IT_SUIT                  = 2097152;
float    IT_QUAD                  = 4194304;
float    IT_LASER_CANNON          = 8388608;


// hipnotic items
float    HIP_IT_WETSUIT           = 2;
float    HIP_IT_EMPATHY_SHIELDS   = 4;
float    HIP_IT_HORN_OF_CONJURING = 8;

// point content values

float    CONTENT_EMPTY            = -1;
float    CONTENT_SOLID            = -2;
float    CONTENT_WATER            = -3;
float    CONTENT_SLIME            = -4;
float    CONTENT_LAVA             = -5;
float    CONTENT_SKY              = -6;

float    STATE_TOP                = 0;
float    STATE_BOTTOM             = 1;
float    STATE_UP                 = 2;
float    STATE_DOWN               = 3;

vector   VEC_ORIGIN               = '0 0 0';
vector   VEC_HULL_MIN             = '-16 -16 -24';
vector   VEC_HULL_MAX             = '16 16 32';

vector   VEC_HULL2_MIN            = '-32 -32 -24';
vector   VEC_HULL2_MAX            = '32 32 64';

// protocol bytes
float    SVC_TEMPENTITY           = 23;
float    SVC_KILLEDMONSTER        = 27;
float    SVC_FOUNDSECRET          = 28;
float    SVC_INTERMISSION         = 30;
float    SVC_FINALE               = 31;
float    SVC_CDTRACK              = 32;
float    SVC_SELLSCREEN           = 33;

float    TE_SPIKE                 = 0;
float    TE_SUPERSPIKE            = 1;
float    TE_GUNSHOT               = 2;
float    TE_EXPLOSION             = 3;
float    TE_TAREXPLOSION          = 4;
float    TE_LIGHTNING1            = 5;
float    TE_LIGHTNING2            = 6;
float    TE_WIZSPIKE              = 7;
float    TE_KNIGHTSPIKE           = 8;
float    TE_LIGHTNING3            = 9;
float    TE_LAVASPLASH            = 10;
float    TE_TELEPORT              = 11;
float    TE_EXPLOSION2            = 12;
float    TE_BEAM                  = 13;

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
float    CHAN_AUTO                = 0;
float    CHAN_WEAPON              = 1;
float    CHAN_VOICE               = 2;
float    CHAN_ITEM                = 3;
float    CHAN_BODY                = 4;
float    CHAN_WEAPON2             = 5;
float    CHAN_OTHER               = 6;

float    ATTN_NONE                = 0;
float    ATTN_NORM                = 1;
float    ATTN_IDLE                = 2;
float    ATTN_STATIC              = 3;

// update types

float    UPDATE_GENERAL           = 0;
float    UPDATE_STATIC            = 1;
float    UPDATE_BINARY            = 2;
float    UPDATE_TEMP              = 3;

// entity effects

float    EF_BRIGHTFIELD           = 1;
float    EF_MUZZLEFLASH           = 2;
float    EF_BRIGHTLIGHT           = 4;
float    EF_DIMLIGHT              = 8;


// messages
float    MSG_BROADCAST            = 0; // unreliable to all
float    MSG_ONE                  = 1; // reliable to one (msg_entity)
float    MSG_ALL                  = 2; // reliable to all
float    MSG_INIT                 = 3; // write to the init string


//================================================

//
// globals
//
float    movedist;
float    gameover;        // set when a rule exits

string    string_null;    // null string, nothing should be held here

entity    newmis;            // launch_spike sets this after spawning it

entity    activator;        // the entity that activated a trigger or brush

entity    damage_attacker;    // set by T_Damage
float    framecount;

float        skill;

#include "vrdefs.qc"

//================================================

//
// world fields (FIXME: make globals)
//
.string       wad;
.string       map;
.float        worldtype;    // 0=medieval 1=metal 2=base

//================================================

.string        killtarget;

//
// quakeed fields
//
.float        light_lev;        // not used by game, but parsed by light util
.float        style;


//
// monster ai
//
.void()        th_stand;
.void()        th_walk;
.void()        th_run;
.void()        th_missile;
.void()        th_melee;
.void(entity attacker, float damage)        th_pain;
.void()        th_die;

.entity        oldenemy;        // mad at this player before taking damage

.float        speed;

.float    lefty;

.float    search_time;
.float    attack_state;

float    AS_STRAIGHT = 1;
float    AS_SLIDING  = 2;
float    AS_MELEE    = 3;
float    AS_MISSILE  = 4;

//
// player only fields
//
.float        walkframe;

.float         attack_finished;
.float        pain_finished;

.float        invincible_finished;
.float        invisible_finished;
.float        super_damage_finished;
.float        radsuit_finished;

.float        invincible_time, invincible_sound;
.float        invisible_time, invisible_sound;
.float        super_time, super_sound;
.float        rad_time;
.float        fly_sound;

.float        axhitme;

.float        show_hostile;    // set to time+0.2 whenever a client fires a
                            // weapon or takes damage.  Used to alert
                            // monsters that otherwise would let the player go
.float        jump_flag;        // player jump flag
.float        swim_flag;        // player swimming sound flag
.float        air_finished;    // when time > air_finished, start drowning
.float        bubble_count;    // keeps track of the number of bubbles
.string        deathtype;        // keeps track of how the player died

//
// object stuff
//
.string        mdl;
.vector        mangle;            // angle at start

.vector        oldorigin;        // only used by secret door

.float        t_length, t_width;


//
// doors, etc
//
.vector        dest, dest1, dest2;
.float        wait;            // time from firing to restarting
.float        delay;            // time from activation to firing
.entity        trigger_field;    // door's trigger entity
.string        noise4;

//
// monsters
//
.float         pausetime;
.entity     movetarget;


//
// doors
//
.float        aflag;
.float        dmg;            // damage done by door when hit

//
// misc
//
.float        cnt;             // misc flag

//
// subs
//
.void()        think1;
.vector        finaldest, finalangle;

//
// triggers
//
.float        count;            // for counting triggers


//
// plats / doors / buttons
//
.float        lip;
.float        state;
.vector        pos1, pos2;        // top and bottom positions
.float        height;

//
// sounds
//
.float        waitmin, waitmax;
.float        distance;
.float        volume;

#include "builtins.qc"
