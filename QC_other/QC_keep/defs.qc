/*==============================================================================

            SOURCE FOR GLOBALVARS_T C STRUCTURE

==============================================================================*/

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
float        parm1, parm2, parm3, parm4, parm5, parm6, parm7, parm8, parm9, parm10, parm11, parm12, parm13, parm14, parm15, parm16;

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

/*==============================================================================
            SOURCE FOR ENTVARS_T C STRUCTURE
==============================================================================*/
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
.entity        groundentity;    // Do not re-use, engine resets value all the time

// stats
.float        health;
.float        frags;
.float        weapon;            // one of the IT_SHOTGUN, etc flags
.string        weaponmodel;
.float        weaponframe;
.float        currentammo;
.float        ammo_shells, ammo_nails, ammo_rockets, ammo_cells;

.float        items;            // bit flags

.float        takedamage;
.entity        chain;
.float        deadflag;

.vector        view_ofs;        // add to origin to get eye point


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

.float        waterlevel;        // 0 = not in, 1 = feet, 2 = waist, 3 = eyes
.float        watertype;        // a contents value

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

//================================================
void        end_sys_fields;            // flag for structure dumping
//================================================

/*==============================================================================
                VARS NOT REFERENCED BY C CODE
==============================================================================*/
float FALSE        = 0;
float TRUE        = 1;
float NEGATIVE    = -1;

// edict.flags
float    FL_FLY                    = 1;
float    FL_SWIM                    = 2;
float    FL_CLIENT                = 8;    // set for all client edicts
float    FL_INWATER                = 16;    // for enter / leave water splash
float    FL_MONSTER                = 32;
float    FL_GODMODE                = 64;    // player cheat
float    FL_NOTARGET                = 128;    // player cheat
float    FL_ITEM                    = 256;    // extra wide size for bonus items
float    FL_ONGROUND                = 512;    // standing on something
float    FL_PARTIALGROUND        = 1024;    // not all corners are valid
float    FL_WATERJUMP            = 2048;    // player jumping out of water
float    FL_JUMPRELEASED            = 4096;    // for jump debouncing

// edict.movetype values
float    MOVETYPE_NONE            = 0;    // never moves
//float    MOVETYPE_ANGLENOCLIP    = 1;
//float    MOVETYPE_ANGLECLIP        = 2;
float    MOVETYPE_WALK            = 3;    // players only
float    MOVETYPE_STEP            = 4;    // discrete, not real time unless fall
float    MOVETYPE_FLY            = 5;
float    MOVETYPE_TOSS            = 6;    // gravity
float    MOVETYPE_PUSH            = 7;    // no clip to world, push and crush
float    MOVETYPE_NOCLIP            = 8;
float    MOVETYPE_FLYMISSILE        = 9;    // fly with extra size against monsters
float    MOVETYPE_BOUNCE            = 10;
float    MOVETYPE_BOUNCEMISSILE    = 11;    // bounce with extra size

// edict.solid values
float    SOLID_NOT            = 0;    // no interaction with other objects
float    SOLID_TRIGGER        = 1;    // touch on edge, but not blocking
float    SOLID_BBOX            = 2;    // touch on edge, block
float    SOLID_SLIDEBOX        = 3;    // touch on edge, but not an onground
float    SOLID_BSP            = 4;    // bsp clip, touch on edge, block

// range values
float    RANGE_MELEE            = 0;
float    RANGE_NEAR            = 1;
float    RANGE_MID            = 2;
float    RANGE_FAR            = 3;

// deadflag values
float    DEAD_NO                = 0;
float    DEAD_DYING            = 1;
float    DEAD_DEAD            = 2;
float    DEAD_RESPAWNABLE    = 3;
float    DEAD_REMOVE            = 4;
float    DEAD_EXPLODE        = 5;
float    DEAD_FINISHED        = 6;

// takedamage values
float    DAMAGE_NO            = 0;
float    DAMAGE_YES            = 1;
float    DAMAGE_AIM            = 2;

// OVERRIDDEN
/*  See defskeep.qc for IT_'s
// items
//float    IT_AXE                = 4096;
//float    IT_SHOTGUN            = 1;
//float    IT_SUPER_SHOTGUN    = 2;
//float    IT_NAILGUN            = 4;
//float    IT_SUPER_NAILGUN    = 8;
//float    IT_GRENADE_LAUNCHER    = 16;
//float    IT_ROCKET_LAUNCHER    = 32;
//float    IT_LIGHTNING        = 64;
//float    IT_EXTRA_WEAPON            = 128;

// Used by the engine to update the Sbar with gfx type
//float    IT_SHELLS            = 256;
//float    IT_NAILS            = 512;
//float    IT_ROCKETS            = 1024;
//float    IT_CELLS            = 2048;

//float    IT_ARMOR1            = 8192;
//float    IT_ARMOR2            = 16384;
//float    IT_ARMOR3            = 32768;
//float    IT_SUPERHEALTH        = 65536;

//float    IT_KEY1                = 131072;
//float    IT_KEY2                = 262144;

//float    IT_INVISIBILITY        = 524288;
//float    IT_INVULNERABILITY    = 1048576;
//float    IT_SUIT                = 2097152;
//float    IT_QUAD                = 4194304;
*/

// point content values

float    CONTENT_EMPTY        = -1;
float    CONTENT_SOLID        = -2;
float    CONTENT_WATER        = -3;
float    CONTENT_SLIME        = -4;
float    CONTENT_LAVA        = -5;
float    CONTENT_SKY            = -6;

float    STATE_TOP        = 0;
float    STATE_BOTTOM    = 1;
float    STATE_UP        = 2;
float    STATE_DOWN        = 3;
float    STATE_DISABLED    = 4;
float    STATE_ENABLED    = 5;
float    STATE_DELETE    = 6;
float    STATE_MOVING    = 7;
float    STATE_ONE        = 1;
float    STATE_TWO        = 2;

vector    VEC_ORIGIN = '0 0 0';

// Minion Eggs = Scorpions, Spiders and Voreling
vector    VEC_HULLE_MIN = '-2 -2 -2';
vector    VEC_HULLE_MAX = '2 2 2';
vector    VEC_HULLT_MIN = '-24 -24 0';
vector    VEC_HULLT_MAX = '24 24 40';

// Player
vector    VEC_HULLSHORT_MIN = '-16 -16 -24';
vector    VEC_HULLSHORT_MAX = '16 16 32';

// Original ID Ogre, Shalrath, Demon, Shambler
vector    VEC_HULL2_MIN = '-32 -32 -24';
vector    VEC_HULL2_MAX = '32 32 64';

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

float    TE_SPIKE        = 0;
float    TE_SUPERSPIKE    = 1;
float    TE_GUNSHOT        = 2;
float    TE_EXPLOSION    = 3;
float    TE_TAREXPLOSION    = 4;
float    TE_LIGHTNING1    = 5;
float    TE_LIGHTNING2    = 6;
float    TE_WIZSPIKE        = 7;
float    TE_KNIGHTSPIKE    = 8;
float    TE_LIGHTNING3    = 9;
float    TE_LAVASPLASH    = 10;
float    TE_TELEPORT        = 11;
float   TE_EXPLOSION2   = 12;
float    TE_BEAM            = 13;              

/*------------------------------------------------------------------
New particle types added by (rogue software)

TE_EXPLOSION2 (coloured version of TE_EXPLOSION)
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    WriteByte (MSG_BROADCAST, 228);  // 247
    WriteByte (MSG_BROADCAST, 5);

 TE_BEAM used to draw a line between two points (grapple hook)
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_BEAM);
    WriteEntity (MSG_BROADCAST, h);
    WriteCoord (MSG_BROADCAST, h.origin_x);
    WriteCoord (MSG_BROADCAST, h.origin_y);
    WriteCoord (MSG_BROADCAST, h.origin_z);
    WriteCoord (MSG_BROADCAST, player.origin_x);
    WriteCoord (MSG_BROADCAST, player.origin_y);
    WriteCoord (MSG_BROADCAST, player.origin_z + 16);
------------------------------------------------------------------*/

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
float    CHAN_AUTO    = 0;
float    CHAN_WEAPON    = 1;
float    CHAN_VOICE    = 2;
float    CHAN_ITEM    = 3;
float    CHAN_BODY    = 4;
float    CHAN_FEET    = 5;
float    CHAN_EXTRA1    = 6;
float    CHAN_EXTRA2    = 7;

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
float    UPDATE_GENERAL    = 0;
float    UPDATE_STATIC    = 1;
float    UPDATE_BINARY    = 2;
float    UPDATE_TEMP        = 3;

// entity effects
float    EF_BRIGHTFIELD    = 1;
float    EF_MUZZLEFLASH     = 2;
float    EF_BRIGHTLIGHT     = 4;
float    EF_DIMLIGHT     = 8;

/* Stuff added by DP/community
// Fitz/QS protocol only supports effects up 256
// defined in dpextensions.qc (here for quick reference)
float    EF_NODRAW        = 16;
float    EF_ADDITIVE        = 32;
float    EF_BLUE            = 64;
float    EF_RED            = 128;
float    EF_NOGUNBOB        = 256;
float    EF_FULLBRIGHT    = 512;
float    EF_FLAME        = 1024;
float    EF_STARDUST        = 2048;
float    EF_NOSHADOW        = 4096;
float    EF_NODEPTHTEST    = 8192;
float    EF_DOUBLESIDED    = 32768;
float    EF_RESTARTANIM_BIT    = 1048576;
float    EF_TELEPORT_BIT    = 2097152;
*/

// messages
float    MSG_BROADCAST    = 0;        // unreliable to all
float    MSG_ONE            = 1;        // reliable to one (msg_entity)
float    MSG_ALL            = 2;        // reliable to all
float    MSG_INIT        = 3;        // write to the init string

//======================================================================
// globals
//======================================================================
float    movedist;
float    gameover;            // set when a rule exits

// make sure string_null remains NULL even after save/loading
// #define string_null __NULL__
string    string_null;        // null string, nothing should be held here
//float    empty_float;

entity    newmis;                // launch_spike sets this after spawning it
entity    activator;            // the entity that activated a trigger or brush
entity    damage_attacker;    // set by T_Damage
float    framecount;

float        skill;
float     developer;

//======================================================================
// world fields (FIXME: make globals)
//======================================================================
.string        wad;
.string     map;
.float        worldtype;        // 0=medieval 1=metal 2=base

.string        killtarget;

//======================================================================
// quakeed fields
//======================================================================
.float        light_lev;        // not used by game, but parsed by light util
.float        style;

//======================================================================
// monster ai
//======================================================================
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

//======================================================================
// player only fields
//======================================================================
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

//======================================================================
// object stuff
.string        mdl;
.vector        mangle;            // angle at start
.float        t_length, t_width;

//======================================================================
// doors, etc
.vector        dest0, dest1, dest2;
.float        wait;            // time from firing to restarting
.float        delay;            // time from activation to firing
.entity        trigger_field;    // door's trigger entity
.string        noise4;
.float        dmg;            // damage done by door when hit

//======================================================================
// monsters
.float         pausetime;
.entity     movetarget;
.float         inpain;            // Originally defined in zombie.qc

//======================================================================
// items
.float        aflag;            // Ammo quantity stored on items
    
//======================================================================
// misc
.float        cnt;             // misc flag
    
//======================================================================
// subs
.void()        think1;
.vector        finaldest, finalangle;

//======================================================================
// triggers
.float        count;            // for counting triggers

//======================================================================
// plats / doors / buttons
.float        lip;
.float        state;
.vector        pos1, pos2, pos3;        // top and bottom positions
.float        height;

//======================================================================
// sounds
.float        waitmin, waitmin2; /*, waitmax*/
.float        distance;
.float        volume;

//======================================================================
// builtin functions
//======================================================================
void(vector ang)    makevectors        = #1;    // sets v_forward, etc globals
void(entity e, vector o) setorigin    = #2;
void(entity e, string m) setmodel    = #3;    // set movetype and solid first
void(entity e, vector min, vector max) setsize = #4;
// #5 was removed
void() break                        = #6;
float() random                        = #7;    // returns 0 - 1
void(string s) dprint;
void(entity e, float chan, string samp, float vol, float atten) sound = #8;

/*-------------------------------------------------------------------------
// A wrapper for the sound command, to help track down precache errors
//---------------------------------------------------------------------
void(entity e, float chan, string samp, float vol, float atten) sound_builtin = #8;
void(entity e, float chan, string samp, float vol, float atten) sound = {
    if (samp == "") {
        dprint("\b[SOUND]\b Entity ("); dprint(e.classname);
        dprint(") missing sound!\n");
    }
    else sound_builtin (e, chan, samp, vol, atten);
}; */

vector(vector v) normalize            = #9;
void(string e) error                = #10;
void(string e) objerror                = #11;
float(vector v) vlen                = #12;
float(vector v) vectoyaw            = #13;
entity() spawn                        = #14;
void(entity e) remove                = #15;
//----------------------------------------------------------------------
// sets trace_* globals
// nomonsters can be:
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
//----------------------------------------------------------------------
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
void() coredump                        = #28;    // prints all edicts
void() traceon                        = #29;    // turns statment trace on
void() traceoff                        = #30;
void(entity e) eprint                = #31;    // prints an entire edict
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
float() droptofloor= #34;                    // TRUE if landed on floor
void(float style, string value) lightstyle = #35;
float(float v) rint                    = #36;    // round to nearest int
float(float v) floor                = #37;    // largest integer <= v
float(float v) ceil                    = #38;    // smallest integer >= v
// #39 was removed
float(entity e) checkbottom            = #40;    // true if self is on ground
float(vector v) pointcontents        = #41;    // returns a CONTENT_*
// #42 was removed
float(float f) fabs = #43;
vector(entity e, float speed) aim = #44;    // returns the shooting vector
float(string s) cvar = #45;                    // return cvar.value
void(string s) localcmd = #46;                // put string into local que
entity(entity e) nextent = #47;                // for looping through all ents
void(vector o, vector d, float color, float count) particle = #48;// start a particle effect
void() ChangeYaw = #49;                        // turn towards self.ideal_yaw at self.yaw_speed
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
//#71 was removed
void(string var, string val) cvar_set = #72;    // sets cvar.value
void(string var, string s, string s) cvar_setlong = #72; // sets cvar.value
//----------------------------------------------------------------------
.float suppressCenterPrint;
void(entity client, string s) centerprint_builtin = #73;
void(entity client, string s, string s) centerprint_msg = #73;
void(entity client, string s, string s, string s) centerprint_msg3 = #73;
void(entity client, string s, string s, string s, string s) centerprint_msg4 = #73;
void(entity client, string s) centerprint = {
    // Is the centerprint message being used by something else?
    if (!client.suppressCenterPrint)
        centerprint_builtin(client, s);
    else {
        // Send message to client console instead
        sprint(client, "(centerprint) ");
        sprint(client, s);
        sprint(client, "\n");
    }
}
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

/*======================================================================
 All global variables and func definitions from all other QC files
======================================================================*/
// ai.qc
entity sight_entity;        // Highlight an angry monster
float sight_entity_time;    // Time got angry

float(entity targ) infront;
float (entity targ) visible;
float(entity targ, float flat2d) range_distance;
float(float attackdist) ai_checkmelee;
float(entity targ) visblocked;
float(entity targ, vector s_offset, vector t_offset, float nomonsters) visxray;
float (entity targ, vector s_ofset, vector t_ofset) visblocked_wide;
float() FacingIdeal;
float(entity source, entity targ, float ignorenoshoot) ai_foundbreakable;

// client.qc
float modelindex_eyes, modelindex_player;
float intermission_running;                                            
float intermission_exittime;
string nextmap;                // Used by GotoNextMap()
.float dmgtime;                // Used by WaterMove()
.float poisonous;            // A monster with poisonous attack
void(entity targ) BleedDeBuff;
void(entity targ) BurnDeBuff;
void(entity targ) PoisonDeBuff;

//    combat.qc
float DAMARMOR = 1;
float NOARMOR = 2;
float DAMAGEALL = 4;
float IGNORECLASS = 8;
void(entity targ, entity inflictor, entity attacker, float damage, float checkarmor) T_Damage;
void(entity inflictor, entity attacker, float damage, entity ignore, float checkclass) T_RadiusDamage;
float(entity targ, entity inflictor) CanDamage;

// fight.qc
float enemy_vis, enemy_infront, enemy_range;
float enemy_yaw;

// items.qc
.float    healamount, healtype;
float (entity e, float healamount, float ignore) T_Heal;

// monsters.qc
void() monster_sightsound;
void() monster_liquid_check;
void() monster_death_use;
void(float gib_no, float gib_qty) ThrowGib;
void() monster_ThrowGib;

// subs.qc
void() SUB_Null;
void() SUB_UseTargets;
void() SUB_Remove;

// traps_pendulum.qc
void() pendulum_setuparray;

// weapon.qc
entity multi_ent;
float multi_damage;
void(entity targ) W_ChangeWeapon;
void(entity targ) W_SetCurrentAmmo;
float(entity targ) W_BestWeapon;
void() W_WeaponFrame;

// ai_explosion.qc
void(float sprite_type, vector org, string expl_sound) SpawnExplosion;
void(vector org, float velrnd, float upbase, float uprnd) SpawnProjectileSmoke;
void(entity source, entity targ, float side) SpawnMeatSpray;
void(entity targ, vector org, vector vel, float part_qty) SpawnBlood;

//======================================================================
void() DummyFunction = //gets rid of the last compiler warnings ; - )
{
    local string w;
    local float l;
    w = self.wad;
    l = self.light_lev;
}