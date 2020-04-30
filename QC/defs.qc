
/*
==============================================================================

            SOURCE FOR GLOBALVARS_T C STRUCTURE

==============================================================================
*/

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
float        parm1;  // items
float        parm2;  // health
float        parm3;  // armorvalue
float        parm4;  // ammo_shells
float        parm5;  // ammo_nails
float        parm6;  // ammo_rockets
float        parm7;  // ammo_cells
float        parm8;  // weapon
float        parm9;  // armortype
float        parm10; // weapon2
float        parm11; // holsterweapon0
float        parm12; // holsterweapon1
float        parm13; // holsterweapon2
float        parm14; // holsterweapon3
float        parm15; // holsterweapon4
float        parm16; // holsterweapon5

//
// global variables set by built in functions
//
vector        v_forward, v_up, v_right;    // set by makevectors()

// set by traceline / tracebox
float        trace_allsolid;        // both start and end vectors were in a solid (in the void)
float        trace_startsolid;    // the start point was in a solid
float        trace_fraction;        // how much of the vector (% from 0 to 1) was traced before it hit something
vector        trace_endpos;        // the final position (if it didn't hit anything, then this is the same as vec2, otherwise it's the impact point
vector        trace_plane_normal;    // the normal of the surface it hit
float        trace_plane_dist;    // used for angled surfaces
entity        trace_ent;            // the entity it hit (or world if it hit nothing)
float        trace_inopen;        // if some portion of the trace is in the air
float        trace_inwater;        // if some portion of the trace is in water (if it's in both air and water, both are set)

entity        msg_entity;            // destination of single entity writes

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
.vector        scale;
.vector        scale_origin;

.vector        punchangle;        // temp angle adjust from damage or recoil

.string        classname;        // spawn function
.string        model;
.float        frame;
.float        skin;
.float        effects;

.vector        mins, maxs;        // bounding box extents reletive to origin
.vector        size;            // maxs - mins

.void()        touch;
.void()        handtouch;        // touched by VR hand
.void()        vr_wpntouch;      // touched by VR weapon
.void()        use;
.void()        think;            // Next function to execute
.void()        think2;
.void()        blocked;        // for doors or plats, called when can't push other

.float        nextthink;
.float        nextthink2;
.entity        groundentity;    // Do not re-use, engine resets value all the time

// stats
.float        health;
.float        frags;

.float        weapon;             // WID
.string       weaponmodel;
.float        weaponframe;

// QUAKE VR
.float        weapon2;            // WID
.string       weaponmodel2;
.float        weaponframe2;

// TODO VR: (P2) document, check meaning of prevweapon
.float prevweapon;
.float prevweapon2;
.float holsterweapon0;
.float holsterweapon1;
.float holsterweapon2;
.float holsterweapon3;
.float holsterweapon4;
.float holsterweapon5;
.string holsterweaponmodel0;
.string holsterweaponmodel1;
.string holsterweaponmodel2;
.string holsterweaponmodel3;
.string holsterweaponmodel4;
.string holsterweaponmodel5;

.float        currentammo;
.float        currentammo2;  // off-hand

.float        ammocounter;
.float        ammocounter2;  // off-hand

.float        ammo_shells, ammo_nails, ammo_rockets, ammo_cells;

.float        items;            // bit flags

.float        takedamage;        // Check by many functions for damage
.entity        chain;            // Can be overwritten by find command
.float        deadflag;        // Used by client functions

.vector        view_ofs;        // add to origin to get eye point


.float        button0;        // fire
.float        button1;        // use
.float        button2;        // jump
.float        button3;        // off-hand fire

.float        impulse;        // weapon changes

.float        fixangle;        // Make an entity instantly turn
.vector        v_angle;        // view / targeting angle for players
.float        idealpitch;        // calculated pitch angle for lookup up slopes


.string        netname;        // Used mostly for MP

.entity     enemy;            // Pointer to current entity enemy

.float        flags;            // FL bitflag operations

.float        colormap;
.float        team;            // Used mostly for Coop

.float        max_health;        // players maximum health is stored here

.float        teleport_time;    // don't back up

.float        armortype;        // save this fraction of incoming damage
.float        armorvalue;

.float        waterlevel;        // 0 = not in, 1 = feet, 2 = waist, 3 = eyes
.float        watertype;        // a contents value
.float        lastwatertime;     // time of last water transition

.float        ideal_yaw;        // Ideal direction for entity to face
.float        yaw_speed;        // Speed (in degrees) turning towards ideal

.entity        aiment;

.entity     goalentity;        // a movetarget or an enemy

.float        spawnflags;        // Mostly custom options for each entity

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

.vector        handpos;
.vector        handrot;
.vector        handvel;
.vector        handthrowvel;
.float         handvelmag;
.vector        handavel;

.vector        v_viewangle;

.vector        offhandpos;
.vector        offhandrot;
.vector        offhandvel;
.vector        offhandthrowvel;
.float        offhandvelmag;
.vector        offhandavel;

.float        touchinghand;
.vector        muzzlepos;
.vector        offmuzzlepos;
.float        teleporting;
.vector        teleport_target;

.float        offhand_grabbing;
.float        offhand_prevgrabbing;

.float        mainhand_grabbing;
.float        mainhand_prevgrabbing;

.float        offhand_forcegrabbing;
.float        mainhand_forcegrabbing;

.float        offhand_hotspot;
.float        mainhand_hotspot;
.vector       roomscalemove;
.float        throwhit;
.float        throwstabilize;
.float        throwstabilizedim;
.float      handtouch_hand;
.entity     handtouch_ent;

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

float cVR_OffHand = 0;
float cVR_MainHand = 1;
float cVR_FakeHand = 2;

float    FALSE                    = 0;
float     TRUE                    = 1;
float NEGATIVE    = -1;
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
float    DEAD_NO                = 0;
float    DEAD_DYING            = 1;
float    DEAD_DEAD            = 2;
float    DEAD_RESPAWNABLE    = 3;
float    DEAD_REMOVE            = 4;
float    DEAD_EXPLODE        = 5;
float    DEAD_FINISHED        = 6;
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

// weapon ids
float    WID_FIST                  = 0;
float    WID_GRAPPLE               = 1;
float    WID_AXE                   = 2;
float    WID_MJOLNIR               = 3;
float    WID_SHOTGUN               = 4;
float    WID_SUPER_SHOTGUN         = 5;
float    WID_NAILGUN               = 6;
float    WID_SUPER_NAILGUN         = 7;
float    WID_GRENADE_LAUNCHER      = 8;
float    WID_PROXIMITY_GUN         = 9;
float    WID_ROCKET_LAUNCHER       = 10;
float    WID_LIGHTNING             = 11;
float    WID_LASER_CANNON          = 12;

// ammo ids
float    AID_SHELLS                = 0;
float    AID_NAILS                 = 1;
float    AID_ROCKETS               = 2;
float    AID_CELLS                 = 3;
float    AID_NONE                  = 4;

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
float    STATE_DISABLED    = 4;
float    STATE_ENABLED    = 5;
float    STATE_DELETE    = 6;
float    STATE_MOVING    = 7;
float    STATE_ONE        = 1;
float    STATE_TWO        = 2;

// Minion Eggs = Scorpions, Spiders and Voreling
vector    VEC_HULLE_MIN = '-2 -2 -2';
vector    VEC_HULLE_MAX = '2 2 2';
vector    VEC_HULLT_MIN = '-24 -24 0';
vector    VEC_HULLT_MAX = '24 24 40';

vector   VEC_ORIGIN               = '0 0 0';
vector   VEC_HULL_MIN             = '-16 -16 -24';
vector   VEC_HULL_MAX             = '16 16 32';
vector    VEC_HULLSHORT_MIN = '-16 -16 -24';
vector    VEC_HULLSHORT_MAX = '16 16 32';

vector   VEC_HULL2_MIN            = '-32 -32 -24';
vector   VEC_HULL2_MAX            = '32 32 64';

// protocol bytes
//----------------------------------------------------------------------
float    SVC_DISCONNECT        = 2;    // Mainly used in client.qc
float    SVC_UPDATESTAT        = 3;    // Force update of stats
float    SVC_SETVIEW            = 5;    // Set camera viewpoint entity
float    STAT_TOTALSECRETS     = 11;    //added total monster message
float    STAT_TOTALMONSTERS     = 12;
float    SVC_TEMPENTITY        = 23;
float    SVC_SETPAUSE        = 24;
float    SVC_CENTERPRINT        = 26;
float    SVC_KILLEDMONSTER    = 27;
float    SVC_FOUNDSECRET        = 28;
float    SVC_SPAWNSTATICSOUND= 29;
float    SVC_INTERMISSION    = 30;
float    SVC_FINALE            = 31;
float    SVC_CDTRACK            = 32;
float    SVC_SELLSCREEN        = 33;
float    SVC_CUTSCENE        = 34;

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
float    CHAN_FEET                = 7;
float    CHAN_EXTRA1              = 8;
float    CHAN_EXTRA2              = 9;

float    ATTN_NONE    = 0;
float    ATTN_NORM    = 1;
float    ATTN_FEETL    = 1.5;
float    ATTN_FEET    = 2;
float    ATTN_IDLE    = 2;
float    ATTN_ARMOUR    = 2;
float    ATTN_GIB    = 2;
float    ATTN_BREAK  = 2;
float    ATTN_WEAPON = 2.3;
float    ATTN_LOW    = 3;
float    ATTN_STATIC    = 3;
float    ATTN_QUIET    = 3.99;

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

// Quake VR hotspots
float QVR_HS_NONE                   = 0;
float QVR_HS_OFFHAND_2H_GRAB        = 1; // 2H grab - helper offhand
float QVR_HS_MAINHAND_2H_GRAB       = 2; // 2H grab - helper mainhand
float QVR_HS_LEFT_SHOULDER_HOLSTER  = 3;
float QVR_HS_RIGHT_SHOULDER_HOLSTER = 4;
float QVR_HS_LEFT_HIP_HOLSTER       = 5;
float QVR_HS_RIGHT_HIP_HOLSTER      = 6;
float QVR_HS_HAND_SWITCH            = 7;
float QVR_HS_LEFT_UPPER_HOLSTER     = 8;
float QVR_HS_RIGHT_UPPER_HOLSTER    = 9;

// Quake VR throwhit values
float QVR_THROWHIT_NEVER_HIT = 0;
float QVR_THROWHIT_HIT = 1;
float QVR_THROWHIT_FORCEGRAB = 2;

// Quake VR Enum: `VrHolsterMode'
float QVR_WEAPONMODE_IMMERSIVE      = 0;
float QVR_WEAPONMODE_CYCLEQUICKSLOT = 1;

// Quake VR Enum: `VrWeaponThrowMode'
float QVR_WEAPONTHROWMODE_IMMERSIVE      = 0;
float QVR_WEAPONTHROWMODE_DISAPPEARONHIT = 1;
float QVR_WEAPONTHROWMODE_DISCARD        = 2;

// Quake VR Enum: `VrWeaponCycleMode'
float QVR_WEAPONCYCLEMODE_DISABLED = 0;
float QVR_WEAPONCYCLEMODE_ALLOWED  = 1;

// Quake VR Enum: `VrMeleeBloodlust'
float QVR_MELEEBLOODLUST_ENABLED  = 0;
float QVR_MELEEBLOODLUST_DISABLED = 1;

// Quake VR Enum: `VrEnemyDrops`
float QVR_ENEMYDROPS_WHENELIGIBLE = 0;
float QVR_ENEMYDROPS_ALWAYS = 1;
float QVR_ENEMYDROPS_DISABLE = 2;

// Quake VR Enum: `VrAmmoBoxDrops`
float QVR_AMMOBOXDROPS_WHENELIGIBLE = 0;
float QVR_AMMOBOXDROPS_ALWAYS = 1;
float QVR_AMMOBOXDROPS_DISABLE = 2;

// Quake VR Enum: `VrForceGrabMode`
float QVR_VRFORCEGRABMODE_DISABLED = 0;
float QVR_VRFORCEGRABMODE_PARABOLA = 1;
float QVR_VRFORCEGRABMODE_LINEAR = 2;
float QVR_VRFORCEGRABMODE_INSTANT = 3;

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
float     developer;

float    readytime; // time after loading a map when the player is considered
                    // ready

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
.void(entity inflictor, entity attacker, float damage) th_pain;
.void()        th_die;

.entity        oldenemy;        // mad at this player before taking damage

.float        speed;

.float    lefty;

.float    search_time;
.float    attack_state;

float AS_RUNNING   = 0; // Hunt mode (find player)
float AS_STRAIGHT  = 1; // Running at player
float AS_SLIDING   = 2; // Straft attack
float AS_MELEE     = 3; // Melee attack
float AS_MISSILE   = 4; // Range attack
float AS_JUMP      = 5; // Jumping attack
float AS_TURRET    = 6; // Turret Range attack
float AS_SIDESTEP  = 7; // Turn 90 degrees and move to the side
float AS_BACKWARD  = 8; // Turn 180 degrees and move backwards

//
// player only fields
//
.float        walkframe;

.float         attack_finished;
.float         offhand_attack_finished;
.float         mainhand_melee_attack_finished;
.float         offhand_melee_attack_finished;
.float        in_melee;
.float        offhand_in_melee;
.float        melee_hit_sound_played;
.float        offhand_melee_hit_sound_played;
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
.vector        dest, dest0, dest1, dest2;
.float        wait;            // time from firing to restarting
.float        delay;            // time from activation to firing
.entity        trigger_field;    // door's trigger entity
.string        noise4;

//
// monsters
//
.float         pausetime;
.entity     movetarget;
.float         inpain;            // Originally defined in zombie.qc


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
.vector        pos1, pos2, pos3;        // top and bottom positions
.float        height;

//
// sounds
//
.float        waitmin, waitmax, waitmin2;
.float        distance;
.float        volume;




//===========================================================================


//
// builtin functions
//

void(vector ang)    makevectors        = #1;        // sets v_forward, etc globals
void(entity e, vector o) setorigin    = #2;
void(entity e, string m) setmodel    = #3;        // set movetype and solid first
void(entity e, vector min, vector max) setsize = #4;
// #5 was removed
void() break                        = #6;
float() random                        = #7;        // returns 0 - 1
void(entity e, float chan, string samp, float vol, float atten) sound = #8;
vector(vector v) normalize            = #9;
void(string e) error                = #10;
void(string e) objerror                = #11;
float(vector v) vlen                = #12;
float(vector v) vectoyaw            = #13;
entity() spawn                        = #14;
void(entity e) remove                = #15;

// sets trace_* globals
// nomonsters can be:
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
void(vector v1, vector v2, float nomonsters, entity forent) traceline = #16;

entity() checkclient                = #17;    // returns a client to look for
entity(entity start, .string fld, string match) find = #18;
string(string s) precache_sound        = #19;
string(string s) precache_model        = #20;
void(entity client, string s)stuffcmd = #21;
entity(vector org, float rad) findradius = #22;
void(string s) bprint                = #23;
void(entity client, string s) sprint = #24;
void(string s) dprint                = #25;
string(float f) ftos                = #26;
string(vector v) vtos                = #27;
void() coredump                        = #28;        // prints all edicts
void() traceon                        = #29;        // turns statment trace on
void() traceoff                        = #30;
void(entity e) eprint                = #31;        // prints an entire edict
//----------------------------------------------------------------------
.float movespeed;        // Movement speed (linked to skill)
.float tetherlock;        // Is the monster locked from movement
float(float yaw, float dist) walkmove_builtin = #32; // returns TRUE or FALSE
float(float yaw, float dist) walkmove = {
    if (self.attack_state == AS_TURRET) return FALSE;
    if (self.movespeed < 0) return FALSE;
    if (self.tetherlock == TRUE) return FALSE;
    return walkmove_builtin(yaw,dist);
};
//----------------------------------------------------------------------
// #33 was removed
float() droptofloor= #34;    // TRUE if landed on floor
void(float style, string value) lightstyle = #35;
float(float v) rint                    = #36;        // round to nearest int
float(float v) floor                = #37;        // largest integer <= v
float(float v) ceil                    = #38;        // smallest integer >= v
// #39 was removed
float(entity e) checkbottom            = #40;        // true if self is on ground
float(vector v) pointcontents        = #41;        // returns a CONTENT_*
// #42 was removed
float(float f) fabs = #43;
vector(entity e, float speed) aim = #44;        // returns the shooting vector
float(string s) cvar = #45;                        // return cvar.value
void(string s) localcmd = #46;                    // put string into local que
entity(entity e) nextent = #47;                    // for looping through all ents
void(vector o, vector d, float color, float count) particle = #48;// start a particle effect
void() ChangeYaw = #49;                        // turn towards self.ideal_yaw
                                            // at self.yaw_speed
// #50 was removed
vector(vector v) vectoangles            = #51;
//----------------------------------------------------------------------
// direct client message generation
void(float to, float f) WriteByte        = #52;
void(float to, float f) WriteChar        = #53;
void(float to, float f) WriteShort        = #54;
void(float to, float f) WriteLong        = #55;
void(float to, float f) WriteCoord        = #56;
void(float to, float f) WriteAngle        = #57;
void(float to, string s) WriteString    = #58;
void(float to, entity s) WriteEntity    = #59;
//----------------------------------------------------------------------
// broadcast client message generation
// void(float f) bWriteByte        = #59;
// void(float f) bWriteChar        = #60;
// void(float f) bWriteShort        = #61;
// void(float f) bWriteLong        = #62;
// void(float f) bWriteCoord        = #63;
// void(float f) bWriteAngle        = #64;
// void(string s) bWriteString    = #65;
// void(entity e) bWriteEntity = #66;
//----------------------------------------------------------------------
void(float step) movetogoal_builtin = #67;
void(float step) movetogoal = {
    if (self.attack_state == AS_TURRET) return;
    if (self.movespeed < 0) return;
    if (self.tetherlock == TRUE) return;
    movetogoal_builtin(step);
};
//----------------------------------------------------------------------
string(string s) precache_file        = #68;        // no effect except for -copy
void(entity e) makestatic        = #69;
void(string s) changelevel = #70;
void(string var, string val) cvar_set = #72;    // sets cvar.value
void(string var, string s, string s) cvar_setlong = #72; // sets cvar.value
//----------------------------------------------------------------------
.float suppressCenterPrint;
void(entity client, string s1) centerprint_builtin = #73;
void(entity client, string s1, string s2) centerprint_msg = #73;
void(entity client, string s1) centerprint = {
    // Is the centerprint message being used by something else?
    if (!client.suppressCenterPrint)
        centerprint_builtin(client, s1);
    else {
        // Send message to client console instead
        sprint(client, "(centerprint) ");
        sprint(client, s1);
        sprint(client, "\n");
    }
}
void(entity client, string s1, string s2, string s3) centerprint_msg3 = #73;
void(entity client, string s1, string s2, string s3) centerprint3 = {
    // Is the centerprint message being used by something else?
    if (!client.suppressCenterPrint)
        centerprint_msg3(client, s1, s2, s3);
    else {
        // Send message to client console instead
        sprint(client, "(centerprint) ");
        sprint(client, s1);
        sprint(client, s2);
        sprint(client, s3);
        sprint(client, "\n");
    }
}
void(entity client, string s1, string s2, string s3, string s4) centerprint_msg4 = #73;
void(entity client, string s1, string s2, string s3, string s4, string s5, string s6, string s7) centerprint_msg7 = #73;
void(entity client, string s1, string s2, string s3, string s4, string s5, string s6, string s7) centerprint7 = {
    // Is the centerprint message being used by something else?
    if (!client.suppressCenterPrint)
        centerprint_msg7(client, s1, s2, s3, s4, s5, s6, s7);
    else {
        // Send message to client console instead
        sprint(client, "(centerprint) ");
        sprint(client, s1);
        sprint(client, s2);
        sprint(client, s3);
        sprint(client, s4);
        sprint(client, s5);
        sprint(client, s6);
        sprint(client, s7);
        sprint(client, "\n");
    }
}
//----------------------------------------------------------------------
void(vector pos, string samp, float vol, float atten) ambientsound = #74;
string(string s) precache_model2    = #75;    // not used anymore
string(string s) precache_sound2    = #76;    // registered version only
string(string s) precache_file2        = #77;    // not used anymore
void(entity e) setspawnparms        = #78;    // set parm1... to the values at level start for coop respawn

void(vector o, vector d, float preset, float count) particle2 = #79;// start a particle2 effect
float(float base, float exp) pow = #80; // power function
void(float hand, float delay, float duration, float frequency, float amplitude) haptic = #81; // VR haptics function
float(float a, float b) min = #82;
float(float a, float b) max = #83;
void(vector ang) makeforward = #84;        // sets v_forward
float(float input, float inputmin, float inputMax, float outputMin, float outputMax) maprange = #85;        // sets v_forward

//============================================================================

/*======================================================================
 All global variables and func definitions from all other QC files
======================================================================*/
// ai.qc
entity sight_entity;        // Highlight an angry monster
float sight_entity_time;    // Time got angry

// client.qc
string nextmap;                // Used by GotoNextMap()
.float dmgtime;                // Used by WaterMove()
.float poisonous;

//
//    combat.qc
//
float DAMARMOR = 1;
float NOARMOR = 2;
float DAMAGEALL = 4;
float IGNORECLASS = 8;

// fight.qc
float enemy_vis, enemy_infront, enemy_range;
float enemy_yaw;

// items.qc
.float healamount, healtype;

// weapon.qc
entity multi_ent;
float multi_damage;

//======================================================================
void() DummyFunction = //gets rid of the last compiler warnings ; - )
{
    local string w;
    local float l;
    w = self.wad;
    l = self.light_lev;
}
