//======================================================================
// Author : Simon "Sock" OCallaghan
// Website: www.simonoc.com
//======================================================================
nosave float configflag;        // Temp cvar used to hold serverflags until live
nosave float temp1flag;            // Temp cvar used to hold temp1 console flag
nosave float prethink;             // always runs everytime the map is start/load/quickload
nosave float postthink;            // Used once after map load
nosave float coop_cvar;            // Has the coop cvar been read from console
nosave float skill_cvar;        // Has the skill cvar been read from console
nosave float autoaim_cvar;        // What is the current state of autoaim
nosave float mapvar_cvar;        // Detects if map variables have been loaded

// Client related features
entity        client_ent;            // Client entity that started server
entity        camera_ent;            // Start of the info_intermission camera chain
entity        coop_ent;            // Start of the info_coop spawn locations
float        coop_artifacts;        // Do artifacts drop from backpacks for coop
float        coop_ammoboxes;        // Toggle ammo boxes respawning in coop
float        coop_weapons;        // Toggle weapons respawning in coop
float        coop_health;        // Toggle health packs respawning in coop
float        coop_powerups;        // Toggle powerups respawning in coop
float        trig_cdtrack;        // Trigger changed CD track
string        trig_skybox;        // Trigger changed Skybox
float        engine;                // Type of engine active 0=Fitz, 1=DP/FTE/QSS
float        ext_active;            // PR extenstions active (DP/FTE)
float        ext_dpfog;            // Extra DP fog parameters
float        ext_dppart;            // Extra DP particle system
float        ext_dpsurf;            // Extra DP query surface information
float        ext_dprain;            // Extra DP particle rain system
float        ext_dpsnow;            // Extra DP particle snow system
//float        ext_dprot;            // Extra DP rotating bmodel system
float        pixelmode;            // Change texture pixel mode (linear/nearest)
float        playerprojsize;        // Player projectiles 0=Large or 1=Small
entity        gibitem;            // Latest gib to be generated
float        gibstone;            // Has stone gibs been precached
float        gibpoison;            // Has poison gibs been precached
float        liquid_alpha;        // Stores water alpha, used for monster sight checks
float        map_gravity;        // Current map gravity setting
float        map_bodyflrcheck;    // Check floor for monster dead bodies
float        map_bodyfadeaway;    // Fade all dead monster bodies (time in secs)
entity        debugent1, debugent2, debugent3; // Used on wide visibility tests

// Worldspawn features
.float        alpha;                // Alpha value (requires modern engine)
.float        gravity;            // per-entity gravity (support added in engines after hipnotic xpac)
.float        water_alpha;        // Map specific alpha value for water surfaces
.float        proj_shotgun;        // Enable/Disable projectile shotgun
.float        upgrade_axe;        // Upgrade the axe with new weapon
.float        upgrade_ssg;        // Upgrade Super Shotgun with new weapon
.float        upgrade_lg;            // Upgrade Lightning Gun with new weapon
.float        no_item_offset;     // All ammo + heal items to use central rotation
.float        no_item_rotate;        // Not used anymore, kept for compatibility
.float        no_zaware;            // Switches OFF ALL zaware monsters
.float        no_sgprojectile;    // Switches OFF ALL shotgun projectiles
.float        no_sgcasing;        // Switches OFF ALL shotgun casings
.float        no_bigprojectiles;    // Switches OFF large player projectiles size
.float        no_liquiddmg;        // Blocks liquid damage checks for monsters
.float        no_surfacecheck;    // Stops surface checks for large maps
.float        sprite_particles;    // Forces all particles to be dots/sprites
.float        knight_defskin;        // Default skin (0-3) type for new knight model
.float        give_weapons;        // Which weapons the player always starts with
.float        take_weapons;        // Which weapons the player never starts with
.float        reset_health;        // Reset health to this value upon spawning
.float        bodyflrcheck;        // Check floor for monster dead bodies
.float        bodyfadeaway;        // Time in seconds before body/head fades away
.vector        mapvar_update;        // Update map variables (Range X->Y=Z)

// Global fog system
float        fog_active;            // Global fog values are active
entity        fog_control;        // Global fog controller (unique to map)
.float        fog_density;        // Global fog density when map loads
.vector        fog_colour;            // Global fog colour when map loads
.string        fog_dpextra;        // Extra parameters for DP engine
.float        fog_currden;        // Current fog density
.float        fog_targden;        // Target fog density
.float        fog_diffden;        // Difference between current and target
.vector        fog_currcol;        // Current fog colour (RGB)
.vector        fog_targcol;        // Target fog colour (RBG)
.vector        fog_diffcol;        // Difference between current and target
.float        fog_difftime;        // Amount of time to fade fog (seconds)
.float        fog_steptime;        // Amount of steps to fade (20 per second)

// Eric Compiler - Switchable Style Shadows
.float        switchshadstyle;    // Switchable style for bmodels (shadows)
.entity        switchfader;        // Controller for fading shadows on bmodels

// General features
.float        moditems;            // New items flag for new stuff in the mod
.float        customkey;            // Bit flag for custom keys (func_door)
.float        bsporigin;            // All bmodel origins are 0,0,0 check this first
.float        savedeffects;        // Saved effects to apply after spawn
.float        nightmare;            // Only spawn entity if nightmare skill active
.float        cooponly;            // Only spawn entity if coop gameplay active
.float        startspawn2;        // player_start2 unique spawn location number
.entity        activate;            // When something is triggered, this is the culprit!
.entity        bmodel_act;            // Trigger bmodel generic touch/kill/use functions
.float        bboxtype;            // Different types of bounding box setups
.vector        bbmins;                // Saved bounding box size (minimum)
.vector        bbmaxs;                // Saved bounding box size (maximum)
.vector        idmins;                // Original ID bounding box (min)
.vector        idmaxs;                // Used for stuck issues in original id maps
.entity        touching;            // The trigger entity that is touching
.string        startmsg;            // Start message for trigger_counter entity
.string        message2;            // Additional messages for all entities
.string        message3;            // Used by misc_books for addtional text
.string        message4;            // Used by misc_books for addtional text
.string        target2;            // Secondary target field (additional fires)
.void()        touch2;                // Used by items for final touch function
.float        classtype;            // Class type - used for quick reference
.float        classgroup;            // Class group - used for quick reference
.float        classmove;            // 1=Walk, 2=Fly, 4=Swim
.float        classproj;            // Projectile used by entity class
.float        projeffect;            // Special effects and states active
.float        gibhealth;            // The negative health value for gibbing
.float        gibbed;                // true/false flag on body state
.float        touchedvoid;        // true/false flag if touched void bmodel
.float        touchedliquid;        // true/false flag if touched fake liquid
.string        touchedsound;        // Sound string for fake liquid exit 
.string        headmdl;            // Head model for gib routine
.float        bleedcolour;        // Alternative colour for 'bleeding' objects
.float        locksounds;            // Locked sound for doors etc
.float        skin_override;        // Override skin selection based on world theme
.float        frame_override;        // Override frame number (ammo_boxes)
.float        frame_box;            // Set frame number for model animation (-1=random)
.float        item_flrcheck;        // Check the floor surface under items
.float        item_expired;        // Removal timer for item in item_thinkloop
.float        item_skinanim;        // Does the skin require animating?
.float        item_skincycle;        // Skin animations are usually 0.3s (health packs)
.float        item_skinanim_no;    // Skin no for animation groups
.float        noradiusdmg;        // Block all T_RadiusDamage, stops grenade spam
.float        nodebuginfo;        // Blocks certain debug info for entities
.float        onladder;            // Update state from ladder entity to client
.entity        entladder;            // Ladder entity for reference (time, sounds etc)
.float        timeladder;            // Amount of time before playing climb sound
.entity        attachment;            // 1st Entity attachment (additional model)
.entity        attachment2;        // 2nd entity attachment (additional model)
.entity        attachment3;        // 3rd entity attachment (additional model)
.entity        sound_emitter;        // Used by various entities for add sound
.float        check_weather;        // Checks for weather system on sound emitters
.float        entno_unique;        // Unique number for linking entities
.string        str_unique;            // Unique string name for testing
.string        ckeyname1, ckeyname2, ckeyname3, ckeyname4;    // Custom key netnames
.float        ckeyhint;            // Hint to the player about inventory key

// Pressure switch/ volume trigger parameters
.float        presstype;            // 0/1 if touching entity type is valid
.float        presstimersound;    // Timer for sound FX (stops repeating)
.float        no_deadbody;        // Monster dead bodies don't work with trigger

// Item respawning parameter
.float        respawn_time;        // Amount of time to spend respawning item
.float        respawn_count;        // Total amount of times to respawn an item
.float        respawn_trig;        // Wait for a trigger before respawning
.float        respawn_effect;        // Show particle effect for respawning items
.float        respawn_style;        // Which particle colour and pattern to use
.vector        respawn_ofs;        // Z axis height for respawn burst
.entity        respawn_part;        // Respawn particle emiiter link on items

// AI Pathing (corners/trains)
.float        direction;            // Travel direction for func_trains
.float        corner_route;        // Change path_corner route (1-3 routes, 4=exact, 5=random)
.float        corner_switch;        // Change spawnflags REVERSE (-1=NO, 1=YES, 2=Toggle)
.float        corner_pause;        // Change spawnflags NOPAUSE (-1=NO, 1=YES, 2=Toggle)
.float        corner_speed;        // Change path_corner speed (def=100)
.string        corner_event;        // Special target event on path_corners
.string        targetback;            // Override backwards pathing route on path_corners
.entity     movetarget2;        // Additional move target for AI pathing
.entity     movetarget3;        // Additional move target for AI pathing
.entity        movelast;            // The last AI pathing corner been at

// Ingame number displays
.float        targetnumber;        // Float number to pass to misc_targetnumber
.entity        tno1, tno2, tno3, tno4, tno5, tno6, tno7, tno8;    // Entity digits

// Entity state system
.float        estate_trigger;        // Entity state to be applied to target
.float        estate;                // Entity state (off,on and disable)
.void()        estate_on;            // Entity state ON  function
.void()        estate_off;            // Entity state OFF function
.void()        estate_disable;        // Entity state DISABLE function
.void()        estate_use;            // Entity state USE function
.void()        estate_fire;        // Entity state USE++ function
.void()        estate_reset;        // Entity state RESET function
.void()        estate_aframe;        // Entity state AFRAME function

// Breakable system
.float        brksound;            // Initial sound type
.float        brkimpsound;        // Impact sound type
.float        brkimpqty;            // Total impact sounds setup
.float        brkobjects;            // Breakable sub object type
.string        brkobj1;            // Breakable object 1 (bsp/mdl)
.string        brkobj2;            // Breakable object 2 (bsp/mdl)
.string        brkobj3;            // Breakable object 3 (bsp/mdl)
.string        brkobj4;            // Breakable object 4 (bsp/mdl)
.float        brkobjqty;            // Total breakable sub objects setup
.vector        brkvelbase;            // Base amount for velocity of broken parts
.vector        brkveladd;            // Random additions for velocity of broken parts
.float        brkavel;            // Amount of breaking object angle velocity
.float        brkfade;            // Fade time before rumble fades away
.vector        brkvol;                // Spawning volume for breakable point entity
.float        brkgravity;            // Change the gravity for rumble, useful for underwater
.float        brkmondmg;            // Damage multipler for monster attacks
.float        brktrigmissile;        // Trigger breakable if hit by - rocket/grenade/shalball
.float        brktrigjump;        // Trigger breakable if hit by jumping monster attack
.float        brktrignoplayer;    // Players cannot damage this breakable
.float        brkdelaydamage;        // Pause from damage when triggered on

// Monster features
.float        bossflag;            // Flag set on all bosses
.entity        enemytarget;        // flying monsters enemy target
.float        debuglvl;            // Used to debug monsters behaviour
.float        randomskin;            // 0=nothing, x=random number of skins
.float        exactskin;            // 1-x exact skin number to choose

.void()        th_checkattack;        // Which check attack function to use
.void()        th_slide;            // Mainly used by wizard monster
.void()        th_charge;            // Mainly used by Hell/Death Knight monsters
.void()        th_altstand;        // Alternative stand animation (Knights only)
.void()        th_wakeup;            // Special wakeup animations before combat
.void()        th_jump;            // Special jump animation for jumpers!
.void()        th_jumpexit;        // Exit animation to stop flying (stuck)
.void()        th_updmissile;        // Update monster missiles while flying
.void()        th_gibdie;            // Special function for gib death

.float        liquidbase;            // Liquid content at base of model (feet)
.float        liquidcheck;        // Timer to prevent over checking of liqdmg
.float        liquidblock;        // Cannot see through liquids for wakeup
.float        distmin;            // Minimum amount of distance to teleport
.float        distmax;            // Maximum amount of distance to teleport

.float        attack_elev;        // Iternative attack angle for zaware monsters
.float        move_elev;            // Z movement adjustment for flying units
.vector        move_state;            // AI state movement; Stand, Walk & Run
.float        bouncegrenade;        // All grenades bounce off monster body
.float        reflectlightning;    // All Lightning strikes are reflected
.float        reflectplasma;        // All plasma projectiles are reflected
.float        reflectnails;        // All NG/SNG projectiles are reflected
.entity        reflectent;            // Entity to generate lightning from

.vector        attack_track;        // Iternative tracking for range attacks
.float        attack_chance;        // The percentage chance the monster will attack
.float        attack_speed;        // Skill based adjusted projectile speed
.float        attack_timer;        // Blocking of certain attack types (charging)
.float        attack_count;        // Total amount of consecutive attacks
.vector        attack_offset;        // Vector where AI fires projectiles from
.float        attack_rage;        // Is the monster in a rage mode
.float        attack_sidestep;    // Timer to switch sides for movement
.float        attack_sidedeny;    // Timer to block side movement
.float        attack_instant;        // No pause when waking up to attack
.float        attack_sniper;        // Will wake up at any range (def=1000)

.float        meleeattack;        // Which type of melee attack is happening
.float        meleerange;            // Distance used for checking melee contact
.float        meleetimer;            // Blood and gore sound/effect timer
.string        meleehitsound;        // Sound to play when melee impacts on enemy
.float        meleecontact;        // Flag set if monster has melee contact sound
.vector        meleeoffset;        // Offset for melee contact (spawning blood)

.float        plasma_burn;        // Timer to stop constant plasma explosions
.float        weaponstate;        // State of the weapon (up/down etc)
.float        weaponswitch;        // Switch timer for different weapons
.string        weaponglow;            // Special glowing model for magic attacks
.float        bodyphased;            // Used for teleporter monsters
.float        bodystatic;            // Used to prevent teleporting monsters
.float        spawnstatue;        // Start/spawn as a statue (cannot be move)
.float        spawnnosight;        // Spawn when not in sight of player
.float        spawnnosighttime;    // Time to wait for sight condition
.float        spawnnotelefrag;    // Spawn when nothing is blocking location
.float        nospawndamage;        // Will not do any telefrag damage on spawn
.float        wakeuptrigger;        // Trigger flag to use special wakeup animation
.entity        turretactive;        // Set when a monster touches a triggerturret
.float        turrettimer;        // Timer used to prevent constant spamming
.float        turrethealth;        // % HP at which a turret monster is released
.string        turrettarget;        // Target to fire when turret % HP is released

.float        pain_flinch;        // Random chance to ignore this much damage
.float        pain_longanim;        // Force monster into long pain animations
.float        pain_finstate;        // Store the current pain finished state
.float        pain_check;            // Pain condition results
.float        pain_timeout;        // Time block to pain function
.float        pain_ignore;        // Ignore pain when hit by other monsters
.string        pain_sound;            // Pain sound wav file
.string        pain_sound2;        // Alternative Pain sound wav file
.string        death_sound;        // Death sound wav file
.string        death_sound2;        // Alternative death sound wav file
.float        death_dmg;            // Used by tarbaby for explosive damage on death
.string        deathstring;        // Used by client for player death messages

.float        idlebusy;            // Busy with an alternative idle animation
.float        idlereverse;        // Reverse direction for idle animation
.float        idletimer;            // Idle sound timer (next time to check)
.float        idlemoreoften;        // Chance of more idle sound (def=random)
.string        idle_sound;            // Idle sound wav file
.string        idle_sound2;        // Alternative Idle sound wav file
.string        idle_soundcom;        // Combat Idle sound wav file
.string        idle_soundcom2;        // Alternative Combat Idle sound wav file
.float        sight_timeout;        // Time block for sight sound
.string        sight_sound;        // Sight sound wav file 1
.string        sight_sound2;        // Sight sound wav file 2
.string        sight_sound3;        // Sight sound wav file 3
.string        sight_sound4;        // Sight sound wav file 4
.float        sight_count;        // Total amount of sight sounds defined
.float        sight_nofront;        // Monster has no infront check

.float        enemydist;            // Distance the enemy is from the monster
.float        enemymaxdist;        // Maximum distance enemy is out of range
.entity        lostenemy;            // Lost soul enemy before losing sight
.float        losttimer;            // Lost soul idle timer after losing sight
.float        lostsearch;            // Lost soul searching for previous enemy

.string        angrytarget;        // Alternative target to attack upon spawn
.string        deathtarget;        // Alternative target to fire when dying
.string        sighttarget;        // Will always attack this target if sighted
.entity        switchattacker;        // Last entity to attack and cause damage
.float        switchtimer;        // Cooldown before switching targets (infighting)
.float        switchoverride;        // Cooldown override timer to stop rapid switch
.float        infightextra;        // Damage multiplier for infighting damage
.float        noinfighting;        // Will ignore/start any infighting
.float        nomonstercount;        // excluded from map monster count
.float        delaymonstercount;    // Monster added to count when spawned
.entity        jumptouch;            // Last entity a jumping monster touched

.float        steptype;            // Primary footstep type
.float        altsteptype;        // Secondary footstep type
.float        steplast;            // Last footstep sound
.float        altsteplast;        // Last club foot sound
.string        stepc1, stepc2, stepc3, stepc4, stepc5; // Custom feet sounds
.float        zombieflr;            // Get up frame for zombie on floor
.string        bodyonflr;            // String used for find function
.string        bodyonflrtrig;        // Trigger to fire when body is axed!
.float        dmgtimeframe;        // Time before damage frame can be reset
.float        dmgcombined;        // combined damage over 1 frame (0.1 time)

.float        bosswave;            // Current wave of the boss (backwards)
.float        bosswavetotal;        // Total amount of boss waves
.float        bosswaveqty;        // Total HP for each boss wave
.float        bosswavetrig;        // Current HP trigger for wave

// Minions (rest of the parameters in ai_minions.qc)
.float        minion_active;        // is the minion system active

// Projectile resistance (% protection) for monsters
.float        resist_shells, resist_nails, resist_rockets, resist_cells;
.float        lightning_timer;    // Cool down for LG resist effect

//----------------------------------------------------------------------
float ENG_UNKNOWN = 0;            // Default engine type
float ENG_FITZ = 1;                // Fitz clone engines
float ENG_DPEXT = 2;            // Engine with Darkplaces extensions

float DEF_GRAVITY = 800;        // Default gravity for maps
float DEF_FOGDEN = 0.1;            // Default fog parameter
vector DEF_FOGCOL = '0.1 0.1 0.1';        // Wispy white
string DEF_FOGEXT = "1 0 8192 1024 32";    // DP only extra
float TELE_SPEED = 300;            // Default teleport velocity
float LARGE_TIMER = 999999;        // Don't run out of time!
float MEGADEATH = 50000;        // Time to die!
float TIME_MINTICK = 0.01;        // Smallest amount of time to tick functions
float FADEMODEL_TIME = 0.01;    // Small amount of time
float FADEFOG_TIME = 0.05;        // 20 updates per second
float PARTICLE_DEBUGMAX = 2;    // Maximum debug level
float part_debug;                // Particle debug level (visual details)
float POWERUP_TIMER = 30;        // Default timer for powerups
float DEF_SHELLS = 25;            // Default ammo given to new players
float FLOOR_TRACE_DIST = 256;    // Default distance for floor trace checks
float FLOOR_TRACE_MONSTER = 40;    // Distance for monster/body checks
float FLOOR_TRACE_GIBS = 16;    // Distance for gib/head checks
float FLOOR_TRACE_BREAK = 16;    // Distance for breakable checks

//----------------------------------------------------------------------
// Server flags (only way to carry items over between maps)
// bits 1-4 Runes, 5-8 Additional keys/items
float SVR_RUNE_KEY1 = 1;        // Rune 1
float SVR_RUNE_KEY2 = 2;        // Rune 2
float SVR_RUNE_KEY3 = 4;        // Rune 3
float SVR_RUNE_KEY4 = 8;        // Rune 4
float SVR_RUNE_ALL = 15;        // Runes 1-4 (together)

float SVR_SPAWN_BIT1 = 16;        // 3 bit spawn location
float SVR_SPAWN_BIT2 = 32;        // Supports 7 locations 001-111
float SVR_SPAWN_BIT3 = 64;        // Set via trigger_change_level

float SVR_LIVE = 128;            // Bit      8 - Serverflag has active data
float SVR_RUNEFLAG = 255;        // Bits  1- 8 - Runes and Keys
float SVR_LOWFLAG = 65535;        // Bits  1-16 - rune keys and spawn settings
float SVR_WORLDFLAG = 65280;    // Bits  9-16 - default/worldspawn options
float SVR_HIGHFLAG = 8323072;    // Bits 17-23 - these options carry over

// MOD Options that can only be changed via temp1 console variable
// These affect the spawning functions of monster / items
float SVR_ITEMOFFSET = 256;        // Toggle item offset (corner/center)
float SVR_SPRPARTON = 512;        // Always use sprite particles (default OFF)
float SVR_PARTICLES = 1024;        // Turn ON particle system (default OFF)
float SVR_NOFOGCMDS = 2048;        // Disables all fog commands (winquake)
float SVR_DEVHELPER = 4096;        // Turn OFF Dev helpers (marks+arrows)

float SVR_UPDAXE = 8192;        // Upgrade Axe, +75% dmg, gib zombies
float SVR_UPDSSG = 16384;        // Upgrade Super Shotgun, +50% dmg, uses 3 shells
float SVR_UPDLG = 32768;        // Upgrade Lightning Gun, Direct+Splashdamage

// MOD Options that can be changed via impulse commands (need a map loaded)
// These options will override the temp1 console variable
float SVR_WEATHER = 65536;        // Turn OFF All weather systems
float SVR_SHOTGPROJ = 131072;    // Turn OFF Shotgun projectiles
float SVR_SHOTGCASE = 262144;    // Turn OFF Shotgun casings
float SVR_ZAWARE = 524288;        // Turn OFF Z aware monsters
float SVR_FOOTSTEP = 1048576;    // Turn OFF Enemy/player footsteps
//float SVR_ = 2097152;            // Not used atm
float SVR_LIQDAM = 4194304;        // Turn OFF Monster Liquid damage

//----------------------------------------------------------------------
float IT_UPGRADE_AXE    = 4096;    // Shadow Axe, +75% dmg, +gib bodies
float IT_UPGRADE_SSG    = 2;    // The Widowmaker, +50 dmg, +3 shells
float IT_UPGRADE_LG        = 64;    // Plasma Gun, direct + splashdamage
float IT_ALLARMOR        = 57344;// Test for player grn/yel/red armour

float IT_CKEY1        = 8192;        // Custom Key 1
float IT_CKEY2        = 16384;    // Custom Key 2
float IT_CKEY3        = 32768;    // Custom Key 3
float IT_CKEY4        = 65536;    // Custom Key 4
float IT_CKEYALL    = 122880;    // All Custom keys

float IT_ARTTOME    = 524288;    // Tome of Power (event trigger)
float IT_ARTSHARP    = 1048576;    // Reduce SG/SSG/RG spread pattern
float IT_ARTWETSUIT    = 2097152;    // Underwater Wetsuit + tank 5mins Oxygen
float IT_ARTPIERCE    = 4194304;    // NG/SNG Nails travel through enemy bodies

.float powerup_sound;
.float sharpshoot_finished, sharpshoot_time, sharpshooter_sound;
.float nailpiercer_finished, nailpiercer_time, nailpiercer_sound;
.float wetsuit_finished, wetsuit_time, wetsuit_sound;
.float wetsuit_level, wetsuit_bubbles;

//----------------------------------------------------------------------
float SKILL_EASY = 0;        // Skill level constants
float SKILL_NORMAL = 1;
float SKILL_HARD = 2;
float SKILL_NIGHTMARE = 3;

float HUD_SECRETS = 2;        // Update secrets screen counter
float HUD_MONSTERS = 4;        // Update monster screen counter

float BUFFER_STUFFCMD = 1;    // Long float print function (lftos)
float BUFFER_SPRINT = 2;    // Defined in subs.soc.qc
float BUFFER_DPRINT = 4;

//----------------------------------------------------------------------
float STATE_SETUP = -1;        // Setup state (on/off functions)
float STATE_ON    = 1;        // Stupid idea to have on states as 0
float STATE_OFF    = 2;        // Impossible to test for 0 as 'not setup' is 0

float ENT_SPNSTATIC = 32;    // Will convert entity to static on spawn
float ENT_STARTOFF = 64;    // Global spawnflags setting
float ESTATE_BLOCK = 6;        // Blocked OFF+DISABLE
float ESTATE_LOWER = 7;        // ON+OFF+DISABLE
float ESTATE_ON = 1;        // Switch ON
float ESTATE_OFF = 2;        // Switch OFF
float ESTATE_DISABLE = 4;    // Disabled (blocks toggle)
float ESTATE_RESET = 8;        // Reset parameters
float ESTATE_AFRAME = 16;    // Changes Aframe only

float TARGET_BACK = 0;        // Behind - 315-45
float TARGET_LEFT = 1;        // Side - 45-135
float TARGET_FRONT = 2;        // Front - 135-225
float TARGET_RIGHT = 3;        // Right - 225-315

float SPNMARK_YELLOW = 0;    // Error something is broken
float SPNMARK_BLUE = 1;        // Delay spawn monster/item
float SPNMARK_GREEN = 2;    // Delay spawn mosnter with no counter
float SPNMARK_RED = 3;        // Nightmare only spawn
float SPNMARK_PURPLE = 4;    // Coop only spawn
float SPNMARK_WHITE = 5;    // Not used atm

//----------------------------------------------------------------------
// Environmental damage
float LIQUID_TIMER = 1;        // Amount of time to check for liquid damage
float MON_MULTIPLIER = 10;    // Multiplier for liquid damage against monsters
float WATER_DAMAGE = 2;        // water drowning
float WATER_AIR = 12;        // time before damage
float LIQUID_DMG = 1;        // Base damage from liquid damage
float SLIME_DAMAGE = 4;        // Acid burning
float LAVA_DAMAGE = 10;        // Getting crispy

//----------------------------------------------------------------------
// Footstep types (slow, drag, light, medium, heavy, large, giant)
float FS_TYPESLOW = 1;        // Demon, Dogs, Ogres, Zombies
float FS_TYPEDRAG = 2;        // Alt Zombie foot
float FS_TYPELIGHT = 3;
float FS_TYPEMEDIUM = 4;
float FS_TYPEHEAVY = 5;        // Death/Hell Knight, Drole, Player, Enforcer
float FS_TYPELARGE = 6;        // Minotaur, Shambler
float FS_TYPEGIANT = 7;        // Golem
float FS_TYPECUSTOM = 9;    // Defined/precached by monster
float FS_FLYING = 10;        // Exclusion, no happy feet!

//----------------------------------------------------------------------
// Monster constant values
float MON_MOVEWALK = 1;                // Walking Monster
float MON_MOVEFLY = 2;                // Flying Monster
float MON_MOVESWIM = 4;                // Swiming Monster
float MON_SIGHTSOUND = 10;            // Stop repeating sightsounds
float PAIN_ALWAY = 5;                // Always go into a pain animation after this time
float MON_VIEWOFS = 24;                // Default view ofset for monsters
float MON_ZMOVEMENT = 8;            // Z Movement for flying monsters
float MON_ZTOL = 16;                // Z movement tolerance for adjustment
string MON_ONFLR = "TRUE";            // Search string for bodies on the floor
float MON_NOGIBVELOCITY = -1;        // Gib system will use minimal velocity
float MON_XYGIBVELOCITY = -2;        // Gib system will add more XY velocity
float MON_GIBFOUNTAIN = -3;            // Gibs fly up and outwards (fountain)
float MON_GIBEXPLOSION = -4;        // Gibs fly upwards with force
float MON_NEVERGIB = -300;            // Monster can never be gibbed

// Used in AI.QC routine for AI distance checks
float MON_RANGE_MELEE = 120;        // Melee range
float MON_RANGE_CLOSE = 192;        // Close melee range
float MON_RANGE_WAKEUP = 256;        // Sight Wakeup range
float MON_RANGE_NEAR = 500;            // Near range
float MON_RANGE_MID = 1000;            // Mid range
float MON_MAX_RANGE = 1000;            // Max range (realistic)
float MON_STEPRANGE = 100;            // Step points for range chance
float MONAI_MDLHEIGHT = 24;            // Offset height of monsters from floor
float MONAI_STEPHEIGHT = 16;        // Maximum step height
float MONAI_STEPLARGE = 32;            // large height distance, flight of steps?
float MONAI_ABOVEMELEE = 120;        // Always be high above enemies inside this distance
float MONAI_ABOVEDIST = 128;        // Distance to maintain above enemytarget
float MONAI_ABOVETIMER = 2;            // Amount of time before changing Z
float MONAI_TURRETMODE = 0.3;        // Percentage change of attack in turret mode

float MON_IDLE_SOUND = 0.2;            // random chance of idle sound
float MON_IDLE_ANIMATION = 0.2;        // random chance of idle animation

//----------------------------------------------------------------------
// Generic options
float MON_AMBUSH = 1;                // Ambush (will only wakeup if see the player)
float MON_SPAWN_NOSIGHT = 8;        // No wakeup sight sound
float MON_SPAWN_NOIDLE = 16;        // no idle sounds (cupboard monster)
float MON_SPAWN_NOGFX = 32;            // no spawn effect or sound
float MON_SPAWN_DELAY = 64;            // Trigger spawn DELAY
float MON_SPAWN_ANGRY = 128;        // Trigger spawn ANGRY
float MON_GHOST_ONLY = 4096;        // Design for any ghost like monster
float MON_POINT_KNIGHT = 8192;        // Pointy Electricity Knight

//----------------------------------------------------------------------
// Unique options
float MON_HOGRE_METAL = 4;            // Metal Skin version +HP
float MON_HOGRE_METUPG = 50;        // Extra health for metal upgrade
float MON_OGRE_NAIL = 2;            // Fires Nails instead of grenades
float MON_OGRE_GREEN = 4;            // Green Skin version +HP
float MON_OGRE_GRNUPG = 50;            // Extra health for green upgrade
float MON_FREDDIE_LASER = 2;        // Fires laser instead of nails
float MON_SPIDER_LARGE = 2;            // Large green spitting version
float MON_SPIDER_CEILING = 4;        // Spider starts on the ceiling
float MON_VORELING_LARGE = 2;        // Large purple spitting version
float MON_VORELING_CEILING = 4;        // Voreling starts on the ceiling
float MON_SWAMPLING_LARGE = 2;        // Large Green spitting version
float MON_SWAMPLING_CEILING = 4;    // Swampling starts on the ceiling
float MON_GARGOYLE_PERCH = 4;        // Start in perched position
float MON_GAUNT_PERCH = 4;            // Start in perched position
float MON_BOIL_HANGING = 2;            // Hanging on wall upside down by spikes
float MON_BOIL_HOBBLED = 4;            // Hobbled on the floor, random A/B position
float MON_SEEK_SHIELD = 4;            // Shield mode until triggered
float MON_SENTINEL_NAIL = 4;        // Red nail firing sentinels (quoth)
float MON_CHTHON_RED = 2;            // New red skin + fire balls
float MON_CHTHON_GREEN = 4;            // Green skin + Slime balls
float MON_SHUB_DMGTRIG = 2;            // Can be triggered with damage
float MON_SHUB_UPSIDE = 4;            // Setup upside (ceiling pose)
float MON_NOUR_BOSS = 2;            // Boss version (with waves+minions)
float MON_BOGL_STRONG = 2;            // Stronger +HP and skin 2
float MON_DCROSSSNIPER = 2;            // No max range limitations for enemies
float MON_DCROSSTRACK = 4;            // Enable tracking for crossbow knights
float MON_ELF_MAGIC = 2;            // Magic range attack (+HP)
float MON_RAINDEER_DEAD = 2;        // Dead body sacrifice
float MON_SANTA_GOOD = 2;            // Good Santa with touch trigger

// Underwater enemies (liquid block option for all of them)
float MON_LIQUIDBLOCK = 4;            // Liquids block sightlines (fish/eel)
float MON_FISH_SMALL = 2;            // Smaller and faster fish!

// Minion spawner options
float MON_MINOTAUR_MINIONS = 4;        // Allow the Minotaur to spawn gargoyles
float MON_SHALRATH_MINIONS = 4;        // Allow the Shalrath to spawn vorlings
float MON_SKULLWIZ_GUARDIAN = 2;    // Special version guarding the runes
float MON_SKULLWIZ_MINIONS = 4;        // Allow the Skull Wizard to spawn skulls
float MON_WRAITH_SCORPIONS = 2;        // Allow the wraith to spawn scorpions
float MON_WRAITH_SPIDERS = 4;        // Allow the wraith to spawn spiders

float MON_JIM_ROCKET = 4;            // Rocket version of Jim the Robot
float MON_SCORPION_STINGER = 4;        // Black stinger scorpion (debuff)
float MON_TARBYLESSJUMP = 4;        // Will not constantly jump around
float MON_WIZARD_ABOVE = 4;            // Wizard will float above enemy
float MON_GOLEM_MELEEONLY = 4;        // Golems only do melee + stomp

// Statue/Zombie options
float MON_CRUCIFIED = 2;            // Zombies - start in crucified position
float MON_STATUE = 2;                // Will start frozen with stone skin
float MON_NOTFROZEN = 4;            // Will not start frozen (works with statues)
float MON_ONFLOOR = 4;                // Zombies - start lying on the floor

// Zombie hacks
float MON_ZOMCRUCIFIED = 1;            // Zombies - crucified
float MON_ZOMBAMBUSH = 2;            // Zombies - Ambush (ID hack)
float MON_ZOMIDHACK = 3;            // Used to switch ID hack around

//----------------------------------------------------------------------
// Player
float RANGE_PLAYAXE = 72;            // Distance for fire axe (player)
float RANGE_CHOPAXE = 64;            // Player distance to chop up bodies
float DAMAGE_PLAYAXE1 = 20;            // Default axe damage
float DAMAGE_PLAYAXE2 = 35;            // Enhanced axe damage

//----------------------------------------------------------------------
// Weapon damage and parameters
float SPEED_PLAYAIM = 10000;        // Player auto aim distance
float SPEED_PLAYERSG = 2000;        // Player speed for SG
float SPEED_PLAYERSSG = 1500;        // Player speed for SSG/Upgrade
float SPEED_MONSG = 800;            // Monster speed for SG
float SPEED_MONSGMULT = 150;        // 800=easy, 950=normal, 1100=hard, 1250=nm
float QUANTITY_SG = 6;                // SG - Shotgun
float QUANTITY_SSG = 14;            // SSG - Super Shotgun
float QUANTITY_WM = 21;                // WM - Widow Maker
float QUANTITY_GRUNT = 4;            // Soldier
float QUANTITY_DEFENDER = 10;        // Defender
vector SPREAD_SG = '0.04 0.04 0';    // Narrow and square spread
vector SPREAD_SG2 = '0.01 0.01 0';    // Sharpshooter version
vector SPREAD_SSG = '0.14 0.08 0';    // Wide and short SSG spread
vector SPREAD_SSG2 = '0.04 0.04 0';    // Sharpshooter version
vector SPREAD_GRUNT = '0.1 0.1 0';    // Solider version (very narrow)
vector SPREAD_DEF = '0.18 0.1 0';    // Defender version (really wide)
float DAMAGE_SHELL = 4;                // Single Shotgun Shell (hitscan)
float DAMAGE_PSHELL = 4;            // Single Shotgun Shell (projectile)
float DAMAGE_PTSHELL = 4;            // Dead center tracer shell (proj only)
float LIFE_SHELLS = 6;                // Lifetime before being removed

float SPEED_PLAYSPIKE = 1000;        // Player speed for Spikes
float SPEED_TRAPSPIKE = 500;        // Trap shooters
float SPEED_HKSPIKE = 200;            // Hell Knight (def=300)
float SPEED_HKSKILL = 50;            // 200=easy, 250=normal, 300=hard, 350=nm
float SPEED_WIZSPIKE = 500;            // Wizards (def=600)
float SPEED_WIZSKILL = 50;            // 500=easy, 550=normal, 600=hard, 650=nm
float SPEED_SPIDER = 600;            // Large Spiders (acid spit)
float SPEED_VORELING = 600;            // Large Voreling (purple spit)
float SPEED_SWAMPLING = 600;        // Large Swamping (poison spit)
float SPEED_BLORD = 600;            // Boglord (acid spit)
float SPEED_ELF = 600;                // Dark Elf (snowballs)
float SPEED_LAVABALL = 250;            // Boss Lava balls
float SPEED_LAVASKILL = 50;            // Lava ball skill upgrade

float SPEED_RLPLAYER = 1000;        // Player rocket speed
float SPEED_PLAYGRENADE = 600;        // Player speed for Grenades
float SPEED_MONGRENADE = 500;        // Ogre grenade speed (org 600)
float SPEED_MONGLSKILL = 50;        // 500=easy, 550=normal, 600=hard, 650=nm
float SPEED_ZOMBFLESH = 550;        // Zombie flesh speed (org 600)
float SPEED_ZOMBIESKILL = 25;        // 550=easy, 575=normal, 600=hard, 625=nm

float SPEED_PLAYPLASMA = 900;        // Player speed for Plasma
float SPEED_REFLECTION = 600;        // Reflection speed for Plasma/Lightning
float SPEED_LASER = 600;            // Enforcer laser speed
float SPEED_PLASMA = 600;            // Eliminator plasma speed
float SPEED_JIMPROJ = 575;            // Crazy flying jim robot laz0r/rockets!?
float SPEED_JIMPROJSKILL = 75;        // 575=easy, 650=normal, 725=hard, nm=800
float SPEED_SENTPROJ = 575;            // Crazy flying jim robot laz0r/rockets!?
float SPEED_SENTPROJSKILL = 75;        // 575=easy, 650=normal, 725=hard, nm=800

float SPEED_DGQFB = 700;            // Chainmail Hell Knight from Quoth
float SPEED_DGQFBSKILL = 100;        // 700=easy, 800=normal, 900=hard, nm=1000

float SPEED_SEEKROCK = 575;            // Large UV ugly robot from rrp
float SPEED_SEEKROCKSKILL = 75;        // 575=easy, 650=normal, 725=hard, nm=800
float SPEED_SEEKLAZ = 400;            // Base speed of Laser attacks
float SPEED_SEEKLAZSKILL = 225;        // 400=easy, 625=normal, 850=hard, 1075=nm

float SPEED_RAINDROCK = 575;        // Mobile xmas meat wagaon
float SPEED_RAINDROCKSKILL = 75;    // 575=easy, 650=normal, 725=hard, nm=800

float SPEED_DFURYSPIKE = 400;        // Base speed of Fury attack A/B
float SPEED_DFURYSKILL = 225;        // 400=easy, 625=normal, 850=hard, 1075=nm

float SPEED_DLORDMISSILE = 650;        // Death lord ball attack
float SPEED_DLORDSKILL = 100;        // 550=easy, 650=normal, 750=hard, 850=nm
float MONAI_DLORDMAX = 30;            // Maximum amount of projectiles to fire

float SPEED_NOURSPIKE = 600;        // Base speed of Nour attack A/B
float SPEED_NOURSKILL = 200;        // 600=easy, 800=normal, 1000=hard, 1200=nm
float SPEED_NOURBOMB = 600;            // Grenade bounce speed (no skill change)

float SPEED_GARGMISSILE = 500;        // Gargoyle fireball attack
float SPEED_GARGSKILL = 150;        // 500=easy, 650=normal, 800=hard, 950=nm

float SPEED_SWIZMISSILE = 525;        // Skull wizard SKULL attack
float SPEED_SWIZSKILL = 75;            // 525=easy, 600=normal, 675=hard, 750=nm
float DAMAGE_SWIZPOISON = 30;        // Direct damage from Poison Rocket

float SPEED_MINOBOLT = 500;            // Minotaur plasma bolt speed
float SPEED_MINOSKILL = 150;        // 500=easy, 650=normal, 800=hard, 950=nm

float SPEED_GAUNTBOLT = 500;        // Gaunt plasma bolt speed
float SPEED_GAUNTSKILL = 150;        // 500=easy, 650=normal, 800=hard, 950=nm
float SPEED_EELBOLT = 300;            // Eel plasma bolt speed
float SPEED_EELSKILL = 100;            // 300=easy, 400=normal, 500=hard, 600=nm

float SPEED_SANTAPROJ = 600;        // Santa snowball attack
float SPEED_SANTASKILL = 200;        // 600=easy, 800=normal, 1000=hard, 1200=nm
float MONAI_SANTAMAX = 30;            // Maximum amount of projectiles to fire

float SPEED_FREDDIEPROJ = 500;        // Freddie spike/laser bolt speed
float SPEED_FREDDIESKILL = 150;        // 500=easy, 650=normal, 800=hard, 950=nm

float SPEED_NAILOGREPROJ = 500;        // Nail Ogre spike speed
float SPEED_NAILOGRESKILL = 150;    // 500=easy, 650=normal, 800=hard, 950=nm

float SPEED_NAILSNOWMPROJ = 500;        // Snowman Nail spike speed
float SPEED_NAILSNOWMSKILL = 150;    // 500=easy, 650=normal, 800=hard, 950=nm

float SPEED_SHALMISSILE = 200;        // Shalrath intial speed of homing missile
float SPEED_SHALSKILL = 25;            // 200=easy, 225=normal, 250=hard, 275=nm
float SPEED_SERGMISSILE = 200;        // Death Sergeant homing missile initial speed
float SPEED_SERGSKILL = 20;            // 200=easy, 220=normal, 240=hard, 260=nm

float SPEED_SCORPTAIL = 400;        // Scorpion Tail Swipe (large arc infront)
float SPEED_SCORPSKILL = 100;        // 400=easy, 500=normal, 600=hard, 700=nm

float SPEED_GOLEMSPIKE = 400;        // Base speed of Golem Rock storm attacks
float SPEED_GOLEMSKILL = 225;        // 400=easy, 625=normal, 850=hard, 1075=nm

float SPEED_EIDOROCK = 400;            // Base speed of Eidolon boulder attack
float SPEED_EIDOSKILL = 150;        // 400=easy, 550=normal, 700=hard, 850=nm
float SPEED_EIDOSPIKE = 400;        // Base speed of Eidolon rock storm attacks
float SPEED_EIDOSKILLSP = 225;        // 400=easy, 625=normal, 850=hard, 1075=nm

float SPEED_ICEGSHARD = 600;        // Base speed for Ice Golem Shard attack
float SPEED_ICEGSKILL = 200;        // 600=easy, 800=normal, 1000=hard, 1200=nm

float SPEED_BLORDBALL = 400;        // Base speed of Bog Lord melee balls
float SPEED_BLORDSKILL = 225;        // 400=easy, 625=normal, 850=hard, 1075=nm
float SPEED_BLORDBOLT = 600;        // Base speed of Bog Lord bolts
                                    // 600=easy, 825=normal, 1025=hard, 1275=nm

float LIFE_PROJECTILE = 6;            // Lifetime before being removed
float LIFE_ROCKET = 5;                // Disappear in a puff of smoke!
float LIFE_GRENADE = 2.5;            // Bounce, bounce, explode
float LIFE_EGG = 1.5;                // Very quick egg hatch!
float LIFE_FLESH = 2.5;                // Life of zombie flesh

float DAMAGE_NGREFSPIKE = 4;        // Reflected Spike for NG
float DAMAGE_NGSPIKE = 9;            // Single Spike for NG
float DAMAGE_SNGSPIKE = 18;            // Single Spike for SNG
float DAMAGE_PLAYGRENADE = 120;        // Direct damage from player grenade
float DAMAGE_MONGRENADE = 40;        // Toned down grenade damage
float DAMAGE_MONROCKET = 40;        // Toned down rocket damage

float ELEV_ZAXIS = 200;                // Standard default Z axis adjustment
float ELEV_DEFAULT = 30;            // Zaware default elevation if range too far
vector ELEV_VIEW_OFS = '0 0 12';    // Zaware target adjustment
vector ELEV_SPID_OFS = '0 0 -16';    // Hit low

vector DAMAGE_RLPLAYER = '100 20 120';    // Base + Random, Splash
vector DAMAGE_RLFURY = '20 0 20';        // Fury rockets - Equal Direct/Radius
vector DAMAGE_RLGARG = '20 10 30';        // Gargoyle rocket, very weak dmg
vector DAMAGE_RLSKULLW = '30 10 20';    // Skull wizard, high direct, low splash
vector DAMAGE_RLWRAITH = '100 20 0';    // Wraith hitscan bolt against monsters
vector DAMAGE_RLDROLE = '40 10 80';        // High splash damage, low direct
vector DAMAGE_RLARMY = '25 5 45';        // Weak direct, high splash
vector DAMAGE_RLJIM2 = '25 15 25';        // Weak direct, med splash
vector DAMAGE_RLDGUARDQ = '25 0 40';    // Weak direct, high splash
vector DAMAGE_RLSEEKER = '25 15 25';    // Weak direct, med splash
vector DAMAGE_RLRAINDEER = '20 0 20';    // Weak direct, low splash
vector DAMAGE_RLFIRETOP = '70 0 20';    // High direct, low splash
vector DAMAGE_RLEIDO = '25 15 25';        // Eidolon Rock - med damage
vector DAMAGE_RLICEG = '20 0 20';        // Ice Golem shard - fury damage
vector DAMAGE_RLBLORD = '20 10 30';        // Bog Lord Slime - High Splashdamage
vector DAMAGE_RLKILLZOM = '120 0 0';    // Direct Damage to kill zombie

vector DAMAGE_PLASMA = '15 5 20';        // Default plasma (very high)
vector DAMAGE_PGPLAYER = '45 0 20';        // Player plasma (very high)
vector DAMAGE_PGREFLECT = '15 0 0';        // Plasma reflection (very low)
vector DAMAGE_PGMINOTAUR = '10 5 10';    // Lower damage for minotaur
vector DAMAGE_PGGAUNT = '10 5 10';        // Lower damage for gaunt
vector DAMAGE_PGNOUR = '10 0 0';        // Direct damage only

float DAMAGE_LGPLAYER = 30;            // Player Lightning gun (per 0.1s)
float DAMAGE_LASER = 15;            // Lasers! (traps AND enforcers)
float DAMAGE_BOGLORD = 20;            // Very high and fast damage
float DAMAGE_BOBLASER = 5;            // Weak laser (x + random()*x) 5-10dmg
float DAMAGE_FLESH = 10;            // Flesh from zombies
float DAMAGE_ZOMBIECLASS = 120;        // Instant death to any zombie class

float SPEED_WRAITHBONE = 400;        // Wraith Nail Storm (large arc infront)
float SPEED_WBONESKILL = 100;        // 400=easy, 500=normal, 600=hard, 700=nm
float DAMGE_WRAITHBOLT = 10;        // Biosuit protection from debuff, base dmg
float DAMAGE_PYROWRAITH = 20;        // Damage from Wraith flame bolt
float DAMAGE_WRAITH = 120;            // Explosive damage from wraiths (org=80)

float SPEED_PYROFLAME = 250;        // Base speed of flames
float SPEED_PYROSKILL = 50;            // 250=easy, 300=normal, 350=hard, 400=nm
float DAMAGE_PYROFLAME = 4;            // Damage from each pyro flame
float DAMAGE_MONFLAME = 20;            // Damage to any monster
float DAMAGE_FUMPOISON = 2;            // Damage from each fumigator burst

// Both crossbow and fury projectiles need to cap at 1100 for NM skill
// Otherwise they are close to impossible to avoid due to spread pattern
float SPEED_DCROSS = 500;            // Base speed of projectile bolts
float SPEED_BOLTSKILL = 200;        // 500=easy, 700=normal, 900=hard, 1100=nm
float DAMAGE_BOLT0 = 5;                // Bolt damge against crossbow knights only
float DAMAGE_BOLT1 = 15;            // High damage hot red bolts!

float DAMAGE_TARBABY = 120;            // Explosive damage from tarbies
float DAMAGE_BOIL = 75;                // Explosive damage from boil
float DAMAGE_NOUR = 50;                // Explosive damage from Nouronihar
float DAMAGE_SEEKER = 50;            // Explosive damage from seeker
float DAMAGE_FREDDIE = 40;            // Explosive damage from Freddie
float DAMAGE_RAINDEER = 20;            // Explosive damage from raindeer
float DAMAGE_SNOWMAN = 40;            // Explosive damage from Snowman

float SPEED_LOSTSTART = 400;        // Initial charging speed
float SPEED_LOSTCHARGE = 50;        // Charging speed multiplier
float SPEED_LOSTMAX = 800;            // Charging Maximum speed
float DAMAGE_LOSTSOUL = 40;            // Explosive damage from lost soul
float DAMAGE_LOSTCHARGE = 10;        // Charging damage for lost soul

//----------------------------------------------------------------------
// Generic melee/jump attack distances
float MONAI_MELEEFRONT = 70;        // Front attacks reach further (original = 60)
float MONAI_MELEESIDE = 60;            // Side attacks less distance
float MONAI_MELEEZAXIS = 48;        // Z axis range for melee attack
float MONAI_CHARGEZAXIS = 20;        // Maximum height for knight charge attack
float MONAI_RANGESUMMON = 128;        // High close to block summoning minion

// Various monsters
float MONAI_MELEEKNIGHT = 80;        // Knight and DKnight sword range
float MONAI_MELEEOGRE = 100;        // Ogre chainsaw got a long range
float MONAI_MELEEBLUNTOGRE = 120;    // Ogre hammer/mace very long range
float MONAI_MELEEOGREHAM = 250;        // Ogre Special Hammer with knockback
float MONAI_MELEESHAM = 100;        // Shambler smash claw attack range
float MONAI_SHAMRANGE = 600;        // Range of Shambler lightning
float MONAI_MELEEGUG = 170;            // Really long arm reach attack range
float MONAI_BOGLORDMELEE = 200;        // over arm slime ball attack
float MONAI_BOGLORDRANGE = 400;        // Inside range for intense melee
float MONAI_MELEESPAWN = 80;        // Jump in-the-face range
float MONAI_MELEEFISH = 60;            // Distance for fish bite
float MONAI_RANGEARMYR = 64;        // Too close range for army rocket
float MONAI_RANGEDEFSSG = 100;        // Close range for SSG Defender
float MONAI_RANGEPYRO = 80;            // Pyro flame melee range
float MONAI_RANGEDEFGL = 256;        // Far range for GL Defender
float MONAI_GARGHEIGHT = 96;        // Try to maintain this height above enemies
float MONAI_MELEEDROLE1 = 90;        // Long arm reach on drole claws
float MONAI_MELEEDROLE2 = 150;        // Inside range for a second claw swipe
float MONAI_RANGEDROLE = 300;        // pre-rage distance to maintain
float MONAI_MELEEFREDDIE = 100;        // Ogre Freddie blade got a long range
float MONAI_RANGEFREDDIE = 300;        // Inside range for stand and fire
float MONAI_MELEEMINOTAUR = 100;    // Large long range minotaur claws
float MONAI_RANGEMINOTAUR = 300;    // pre-rage distance to maintain
float MONAI_RANGESHAL = 400;        // Distance to keep minion spawners at
float MONAI_RANGESHAL2 = 300;        // Too close, move away from enemy
float MONAI_MELEESKULLW = 192;        // Too close, teleport away
float MONAI_RANGESKULLW = 256;        // Distance to be after teleport
float MONAI_MELEEGOLEM = 100;        // Golem punch/pound attack range
float MONAI_GOLEMRANGE = 200;        // How close for golem stomp attack
float MONAI_LOSTTIMER = 15;            // How long a lost soul will stay lost
float MONAI_MELEEBOIL = 80;            // Exploding boil trigger range
float MONAI_HOBBLEBOIL = 120;        // Exploding range for hobble version
float MONAI_MELEEDGUARDQ = 100;        // Long range for Death Guard swing
float MONAI_CHARGEDGARDQ1 = 140;    // Min range for charge attack
float MONAI_CHARGEDGARDQ2 = 190;    // Max range for charge attack
float MONAI_RANGEDGARDQ = 300;        // Max range for fireball attack
float MONAI_RANGEDLORD = 200;        // Range for switching magic attacks

float MONAI_JUMPTIMEOUT = 2;        // Time between jumps
float MONAI_RUNPAUSE = 128;            // If enemy this close, stop pause and run!

// Dog
float MONAI_MELEEDOG = 80;            // Distance for dog bite
float MONAI_JUMPDOGNEAR = 80;        // DOG Jump attack distance ranges
float MONAI_JUMPDOGFAR = 150;

// Spider / Voreling / Swampling / Scorpion / Lost Soul
float MONAI_MELEESPIDER = 60;        // Distance for spider bite
float MONAI_JUMPSPIDERNEAR = 80;    // SPIDER Jump attack ranges
float MONAI_JUMPSPIDERFAR = 200;
float MONAI_JUMPSPIDERDIST = 400;    // Forward distance modifier
float MONAI_MELEEVORELING = 60;        // Distance for voreling bite
float MONAI_JUMPVORELINGNEAR = 80;    // VORELING Jump attack ranges
float MONAI_JUMPVORELINGFAR = 200;
float MONAI_JUMPVORELINGDIST = 500;    // Forward distance modifier
float MONAI_MELEESWAMPLING = 60;    // Distance for swampling bite
float MONAI_JUMPSWAMPLINGNEAR = 80;    // SWAMPLING Jump attack ranges
float MONAI_JUMPSWAMPLINGFAR = 200;
float MONAI_JUMPSWAMPLINGDIST = 500;// Forward distance modifier
float MONAI_MELEELOSTSOUL = 80;        // Distance for Lost Soul bite
float MONAI_RANGELOSTNEAR = 200;    // Too close for charge attack
float MONAI_RANGELOSTFAR = 400;        // Too far for charge attack

// Scorpions
float MONAI_JUMPSCORPTIME = 1;        // Special timer for scorpion
float MONAI_MELEESCORPION = 80;        // Distance for scorpion claws
float MONAI_JUMPSCORPNEAR = 80;        // SCORPION Jump attack ranges
float MONAI_JUMPSCORPFAR = 300;        // long jump distance!

// Demon1
float MONAI_MELEEDEMON = 80;        // Distance for demon claws
float MONAI_JUMPDEMONNEAR = 100;    // DEMON Jump attack distance ranges
float MONAI_JUMPDEMONFAR = 200;
float MONAI_JUMPDEMONCHECK = 128;    // Ceiling height check for jumping demons

// Zombie Knight
float MONAI_JUMPZOMBTIME = 2;        // Time before next jump attack
float MONAI_JUMPZKNEAR = 100;        // Near range for a zombie knight jump
float MONAI_JUMPZKFAR = 160;        // Far range for zombie knight

// Sergeant
float MONAI_JUMPSERGTIME = 2;        // Time before next jump attack
float MONAI_JUMPSERGNEAR = 100;        // SERG jump attack distance
float MONAI_JUMPSERGFAR = 250;
float MONAI_CHARGEFLAIL = 350;        // Flail Knight charging distance

// Fury Knight
float MONAI_JUMPFURYTIME = 2;        // Time before next jump attack
float MONAI_JUMPFURYNEAR = 150;        // FURY jump attack distance
float MONAI_JUMPFURYFAR = 350;

// Minotaur
float MONAI_JUMPMINOTIME = 2;        // Time before next jump attack
float MONAI_JUMPMINONEAR = 150;        // MINOTAUR jump attack distance
float MONAI_JUMPMINOFAR = 300;

// Seeker
float MONAI_MELEESEEKER = 80;        // Long arm reach on Seeker
float MONAI_MELRAGESEEKER = 150;    // Range for melee attack
float MONAI_RANGESWSEEKER = 300;    // Switch between rockets and lasers

// Skull wizard starting state
float MONAI_SKULLWSOLID = 1;        // Regular spawn state
float MONAI_SKULLWINVIS = 2;        // Invisible

// Hunter Ogre - Chainsaw
float MONAI_JUMPHOGRETIME = 5;        // Time before next jump attack
float MONAI_JUMPHOGRENEAR = 200;    // Hunter Ogre jump attack distance
float MONAI_JUMPHOGREFAR = 300;

// Xmas monsters
float MONAI_MELEERAINDEER = 80;        // Medium range attack
float MONAI_MELEEELF = 60;            // Distance for elf melee
float MONAI_JUMPELFNEAR = 80;        // ELF Jump attack ranges
float MONAI_JUMPELFFAR = 200;        // (Based on spiders)
float MONAI_JUMPELFDIST = 400;        // Forward distance modifier
float MONAI_MELEESNOWMAN = 80;        // Standing still attack
float MONAI_MELEESANTA = 100;        // Large Sack attack

// Shockwave floor attacks
vector MONAI_STOMPIMPACT = '12 8 -22';
float MONAI_STOMPDAMAGE = 30;        // Impact damage
float MONAI_STOMPRADIUS = 512;        // Radius of foot stomp attack
float MONAI_STOMPFORWARD = 300;        // Forward momentum
float MONAI_STOMPUP = 600;            // Vertical momentum

vector MONAI_HAMMERIMPACT = '56 0 -22';
float MONAI_HAMMERDAMAGE = 30;        // Impaxct damage
float MONAI_HAMMERRADIUS = 512;        // Radius of hammer slam attack
float MONAI_HAMMERFORWARD = 300;    // Forward momentum
float MONAI_HAMMERUP = 500;            // Vertical momentum

float MONAI_IMPACTZAXIS = 128;        // Above/below limit on affect

// Boss Nour
float MONAI_MELEENOUR = 192;        // Always attack player
float MONAI_CLOSENOUR = 400;        // Close distance spit/bomb
float MONAI_MAXNOUR = 325;            // Maximum tether range

// Boss Eidolon
float MONAI_SPAWNEIDO = 80;            // Close to spawn location
float MONAI_CLOSEEIDO = 192;        // Always attack player
float MONAI_MEDIUMEIDO = 384;        // Good distance for boulder
float MONAI_MAXEIDO = 448;            // Maximum tether range

// Boss Ice Golem
float MONAI_MELEEICEG = 128;        // Always attack with melee
float MONAI_MAXICEG = 400;            // Maximum tether range
float MONAI_STOMPICEG = 400;        // How close for stomp attack
float MONAI_STOMPICERADIUS = 640;    // Radius of foot stomp attack

//======================================================================
// Unique Class Groups for quicker searches (mostly items)
// Previous system used classname (slow string search)
//======================================================================
float CG_PLAYERS = 50;            // All player/clients
float CG_HEALTH = 100;            // All health pack types (15/25/+100)
float CG_AMMOITEM = 200;        // All ammo items/packs
float CG_PROJALL = 205;            // All monster projectiles
float CG_PROJSHELLS = 210;        // To help with combining shell damage
float CG_PROJNAILS = 220;        // Testing ammo resistance in t_damage
float CG_PROJROCKETS = 230;
float CG_PROJCELLS = 240;
float CG_MINIONEGG = 245;        // Special minion eggs
float CG_WEAPON = 250;            // All weapon items
float CG_ARMOR = 275;            // All armor types
float CG_KEY = 300;                // All keys (gold/silver/custom)
float CG_RUNE = 310;            // All runes 1-4
float CG_ARTIFACT = 350;        // All powerups
float CG_TRAPS = 400;            // Endless supply of traps!
float CG_MISCENT = 410;            // All misc entity types
float CG_TEMPENT = 420;            // All temporary types (gibs, smoke, sparks)
float CG_BREAKABLE = 430;        // All breakable types
float CG_FUNCMOVER = 450;        // All FUNC movers
float CG_MONSTERS = 500;        // Default

//----------------------------------------------------------------------
// Useful for infighting grouping checks
//----------------------------------------------------------------------
float CG_ARMY = 600;            // (4) Shotgun, Rocket Launcher and Plasma
float CG_ENFORCER = 610;        // (5) Standard, Grenade and Flame, Pyro, Centurion
float CG_DEMON = 615;            // (3) Demon, Drole, Minotaur, Boglord
float CG_DOG = 620;                // (1) Dog
float CG_FISH = 625;            // (2) Fish
float CG_ROBOT = 630;            // (2) Jim, Laser/Rocket, Seeker
float CG_KNIGHT = 635;            // (3) Knight, Death Guard and Crossbow
float CG_DKNIGHT = 637;            // (2) Hell and Death Knight
float CG_DCAPTAIN = 640;        // (2) Fury, Sergeant Captains
float CG_STONE = 645;            // (4) Boss Eidolon, Statues and Golem
float CG_OGRE = 650;            // (4) Vanilla and Hunter (Chainsaw, Mace, Hammer), Freddie
float CG_SPIDER = 660;            // (4) Shalrath, Vorling, Wraith, Spider
float CG_SHAM = 666;            // (1) Shambler
float CG_SPAWN = 670;            // (2) Blue/Green, Boil
float CG_WIZARD = 675;            // (6) Wizard, Gargoyle, Gaunt, Skull Wizard, Skull, Nour
float CG_XMAS = 680;            // (3) Elf, Raindeer, Snowman, Santa
float CG_ZOMBIE = 685;            // (3) Zombie, Knight, Mummy
float CG_BOSS = 690;            // (3) Cthon, Old one

//======================================================================
// Unique Class Types for quicker entity searches
// Previous system used classname (slow string search)
//======================================================================
float CT_WORLD = 1;                // World entity
float CT_PLAYER = 7;            // Player (007)

float CT_SPAWNSP = 10;            // Spawn location - Single Player
float CT_SPAWNCOOP = 11;        // Spawn location - Coop
float CT_SPAWNDM = 12;            // Spawn location - Deathmatch
float CT_CONTROL = 15;            // Controller entity
float CT_CAMERA = 16;            // Camera entity (info_intermission)

float CT_LIGHT = 20;            // Light entity
float CT_SOUNDEMITTER = 22;        // Sound emitter
float CT_DEVMARKER = 24;        // Developer model marker
float CT_PATHCORNER = 26;        // Standard path nodes

float CT_ENEMYTARGET = 30;        // Target enemy for flying enemies
float CT_ATTACHMENT = 32;        // Attachment for entities
float CT_ESTATE = 34;            // Entity state modifier
float CT_MAPVAR = 35;            // Map Variable logic

float CT_PARTICLE = 40;            // A particle
float CT_PARTICLEEMIT = 41;        // A particle emitter
float CT_PARTICLECONT = 42;        // Particle controller (setup particle banks)
float CT_PARTICLETEMP = 45;        // Particle emitter template

float CT_TEMPGIB = 50;            // Temporary entity (gib)
float CT_TEMPSPARK = 52;        // Temporary entity (spark)
float CT_TEMPSTREAM = 54;        // Temporary entity (stream)

float CT_REFLECTLIGHT = 55;        // Reflection entity (Lightning)
float CT_REFLECTPLASMA = 56;    // Reflection entity (Plasma)

float CT_STORYCONTROL = 60;        // Controller for story system
float CT_STORYBOOK = 62;        // Story book for the player
float CT_STORYCHAPTER = 64;        // Story chapter for story system

float CT_TARGETDUMMY = 90;        // Misc target dummy
float CT_TARGETNUMBER = 95;        // Misc number drawn ingame

//----------------------------------------------------------------------
// Class Types for all monsters 
//----------------------------------------------------------------------
float CT_MONARMY = 100;            // Army Soldier
float CT_MONENFORCER = 110;        // Enforcer
float CT_MONDEMON = 115;        // 2 clawed fiend
float CT_MONDOG = 120;            // 4 legged friend
float CT_MONFISH = 125;            // Fish fish fishy
float CT_MONHELLK = 130;        // Strong spikey Knights
float CT_MONKNIGHT = 140;        // Small metal Knights
float CT_MONOGRE = 150;            // Chainsaw wielding manics
float CT_MONOGREFISH = 155;        // Madfox fishing ogre
float CT_MONSHAL = 160;            // Shalrath / Vore
float CT_MONSHAM = 165;            // Shambler
float CT_MONBOGLORD = 166;        // Lord of the Bog! (Big shammy)
float CT_MONSPAWN = 170;        // Bouncing annoying shit!
float CT_MONWIZARD = 180;        // Wizard / Scrags
float CT_CACHEWIZARD = 181;        // Special entity for precache of assets
float CT_MINIONWIZARD = 182;    // Minion Spawning version for Nour Boss
float CT_MONZOMBIE = 185;        // Rotten flesh

float CT_MONIDCHTHON = 190;        // ID Software Chthon
float CT_MONXXCHTHON = 191;        // Mapper Custom Chthon
float CT_MONFIRETOP = 192;        // Firetop Mountain Chthon
float CT_MONIDSHUB = 195;        // ID Software Shub-Niggurath
float CT_MONXXSHUB = 196;        // Mapper Custom Shub-Niggurath

// New Medieval series (lunaran)
float CT_MONZOMBIEK = 186;        // Undead Knights
float CT_MONNOUR = 197;            // Nour Boss

// New (death) Knight series (sock/necros)
float CT_MONDGUARD = 200;        // HD Knight (Guard)
float CT_MONDCROSS = 205;        // HD Knight (Crossbow)
float CT_MONDFURY = 210;        // HD Knight (Fury)
float CT_MONDBLOOD = 215;        // HD Hell Knight (Blood)
float CT_MONDKNIGHT = 220;        // HD Hell Knight (Death)
float CT_MONDSERG = 225;        // HD Hell Knight (Sergeant)
float CT_MONCOMM = 230;            // HD Hell Knight (Commander)

// New (hunter) Ogre series (sock/necros)
float CT_MONHOGRE = 250;        // HD Ogre (Chainsaw)
float CT_MONHOGREMAC = 255;        // HD Ogre (Mace)
float CT_MONHOGREHAM = 260;        // HD Ogre (Hammer)

// New base monster series (Inspired by lunaran/Quoth)
float CT_MONARMYROCKET = 270;    // Army Soldier (Rocket)
float CT_MONARMYGRENADE = 272;    // Army Soldier (Grenade)
float CT_MONARMYPLASMA = 275;    // Army Soldier (Plasma)
float CT_MONDEFENDER = 280;        // Enforcer SSG/GL (Defender)
float CT_MONELIMATOR = 285;        // Enforcer Plasma (Eliminator)

// Hexen 2 series (Raven Software)
float CT_MONSPIDER = 300;        // Spiders (brown/green versions)
float CT_CACHESPIDER = 302;        // Special entity for precache of assets
float CT_MINIONSPIDER = 304;    // Minion Spawning version from wraith
float CT_MONSCORPION = 310;        // Deadly Scorpion (HL Black Crab!)
float CT_CACHESCORPION = 312;    // Special entity for precache of assets
float CT_MINIONSCORPION = 314;    // Minion Spawning version from wraith
float CT_MONLOSTSOUL = 315;        // The Original Doom flying lost soul
float CT_CACHELOSTSOUL = 317;    // Special entity for precache of assets
float CT_MINIONLOSTSOUL = 319;    // Minion Spawning version from skullwizard
float CT_MONSKULLW = 320;        // Creepy Skull Wizard (Teleport)
float CT_MONGARGOYLE = 325;        // Flying Gargoyle (Flying fireball)
float CT_CACHEGARGOYLE = 327;    // Special entity for precache of assets
float CT_MINIONGARGOYLE = 329;    // Minion Spawning version from minotaur
float CT_MONGOLEM = 330;        // Giant stone Golem (ground slam)
float CT_MONICEGOLEM = 335;        // Giant Ice Golem (shards+ground slam)
float CT_MONICEGOLEMWAVE = 337;    // Wave targets for boss Ice Golem
float CT_MONMINOTAUR = 340;        // Large tank like bull (magic/charge)
float CT_MONEIDOLON = 345;        // Large demon boss Eidolon
float CT_MONEIDOLONWAVE = 347;    // Wave targets for boss Eidolon

// Quake Expansion Packs (Rogue Software)
float CT_MONWRAITH = 350;        // Undead wraith (Burning debuff)
float CT_MONMUMMY = 355;        // Undead mummy (Poison debuff)
float CT_MONEEL = 357;            // Plasma bolt spewing eel

// RMQ MOD(Ijed/?)
float CT_MONBOIL = 360;            // Slow moving zombie bomb
float CT_MONSEEKER = 362;        // Robot replacement for Shambler

// Twisted Xmas MOD (Twisted Matrix)
float CT_MONELF = 365;            // Little melee (knight)
float CT_MONRAINDEER = 366;        // Rocket wielding stag (ogre)
float CT_MONSNOWMAN = 367;        // Quick firing turret
float CT_MONSANTA = 369;        // Good/bad santa (boss)

// Quoth MOD (Kell/Necros/Preach)
float CT_MONVORELING = 370;        // Little baby vorelings! ahh so cute!
float CT_CACHEVORELING = 372;    // Special entity for precache of assets
float CT_MINIONVORELING = 374;    // Minion Spawning version from shalrath
float CT_MONSWAMPLING = 375;    // Tiny swamplings! ahh so cute!
float CT_MONDGUARDQ = 376;        // Special Hell Knight with fireball
float CT_MONDLORD = 377;        // Special Hell Knight with shield
float CT_MONDROLE = 380;        // Fast moving tentacle tank with rockets
float CT_MONGUG = 381;            // Large hulking bile spewing nightmare
float CT_MONFREDDIE = 383;        // Slow moving mechanical ogre mutant
float CT_MONGAUNT = 384;        // Flying harpy thing with plasma bolts!
float CT_MONJIM = 385;            // Jim the flying robot of doom!
float CT_MONJIMROCKET = 386;    // Jim has a rocket launcher!
float CT_MONSENTINEL = 387;        // Fleshy robot Sentry

// Rubicon series (metlslime)
float CT_MONPYRO = 390;            // Enforcer Flame (Pyro)
float CT_MONFUMIGATOR = 392;    // Poison version of the Pyro
float CT_MONCENTURION = 395;    // Enforcer hoverboard (Plasma)

//----------------------------------------------------------------------
// Class Types for all projectiles in game (there is a lot!)
//----------------------------------------------------------------------
float CT_PROJ_TRACE = 400;        // Tracer shot (true aim)
float CT_PROJ_TRACEPART = 402;    // Tracer shot (extra particles)
float CT_PROJ_SHELLC = 405;        // Shellcase projectile

float CT_PROJ_SG = 410;            // SG projectile
float CT_PROJ_SSG = 412;        // SSG projectile

float CT_PROJ_NG = 415;            // NG projectile
float CT_PROJ_SNG = 416;        // SNG projectile
float CT_PROJ_MONNG = 417;        // NG monster projectile (glowing red)
float CT_PROJ_REFNG = 418;        // Reflected (glowing red)
float CT_PROJ_HKN = 419;        // Hell Knight Spike

float CT_PROJ_WBONE = 420;        // Wraith Bone Bolt (nail)
float CT_PROJ_GROCK = 421;        // Golem rock debris (pebble)
float CT_PROJ_SCORP = 422;        // Scorpion Tail (nail)
float CT_PROJ_DLORD = 423;        // Death Lord spike ball
float CT_PROJ_SANTA = 424;        // Santa Snowball shotgun!

float CT_PROJ_WIZ = 425;        // Wizard Spit
float CT_PROJ_NOUR1 = 426;        // Nour Spit 
float CT_PROJ_MPOISON = 427;    // Minotaur Poison Bolt (nail)
float CT_PROJ_FURY2 = 429;        // Fury Spit with grenade trail

float CT_PROJ_PLASMA = 430;        // Generic Plasma (blue laser version)

float CT_PROJ_WBOLT = 440;        // Wraith Hitscan Bolt 
float CT_PROJ_BOLT1 = 442;        // Crossbow Knight Bolt +hknight trail
float CT_PROJ_LASER = 445;        // Enforcer/Traps/Bob Laser
float CT_PROJ_BLBOLT = 448;        // Boglord fast bolt (green)
float CT_PROJ_BLBOLT2 = 449;    // Boglord fast bolt (red)

float CT_PROJ_GL = 450;            // Grenade projectile
float CT_PROJ_GLMON = 452;        // Monster grenades
float CT_PROJ_MEGG = 455;        // Minion Egg (grenade arc+bounce)
float CT_PROJ_FLESH = 460;        // Zombie Flesh (grenade arc)
float CT_PROJ_SPID = 461;        // Spider Spit (grenade arc)
float CT_PROJ_VORE = 462;        // Voreling Spit (grenade arc)
float CT_PROJ_SWAMP = 463;        // Swampling Spit (grenade arc)
float CT_PROJ_ELF = 464;        // Elf snowball (grenade arc)
float CT_PROJ_NOUR2 = 467;        // Nour Spit bomb! (grenade arc+bounce)
float CT_PROJ_CHTHON2 = 468;    // Chthon Frenzy (grenade arc)
float CT_PROJ_SHUB2 = 469;        // Shub Frenzy (grenade arc)

float CT_PROJ_ROCKET = 470;        // Rocket projectile
float CT_PROJ_LAVA = 471;        // Chthon Lavaball
float CT_PROJ_FIRETOP = 472;    // Chthon Lavaball (less splash dmg)
float CT_PROJ_CHTHON = 473;        // Custom Chthon Lava/Slime ball
float CT_PROJ_GARG = 475;        // Gargoyle fireball
float CT_PROJ_SKULLW = 477;        // Skull Wizard Skull (rocket)
float CT_PROJ_FURY1 = 480;        // Fury Knight spike ball +rocket trail
float CT_PROJ_ARMY = 482;        // Army rocket
float CT_PROJ_JIM2 = 483;        // Jim robot rocket
float CT_PROJ_SEEKER = 484;        // Seeker robot rocket
float CT_PROJ_DROLE = 485;        // Drole fireball
float CT_PROJ_DGUARDQ = 486;    // Death Guard (Quoth) fireball
float CT_PROJ_BLORD = 487;        // Bog Lord Green Slime bomb
float CT_PROJ_BLORD2 = 488;        // Bog Lord Red Slime bomb
float CT_PROJ_EIDO1 = 490;        // Eidolon boulder
float CT_PROJ_ICEG1 = 492;        // Ice Golem ice shard
float CT_PROJ_RAINDEER = 494;    // Raindeer rocket

float CT_PROJ_SHAL = 495;        // Shalrath spike ball +purple trail
float CT_PROJ_SERG = 496;        // Death Sergeant spike ball +hknight trail
float CT_PROJ_SHUB1 = 497;        // Custom Shub homing spike attack

//----------------------------------------------------------------------
float CT_UPGRADE_AXE = 500;        // Shadow Axe
float CT_SHOTGUN = 505;            // Shotgun
float CT_SUPER_SHOTGUN = 510;    // Super Shotgun
float CT_UPGRADE_SSG = 515;        // The Widowmaker Shotgun
float CT_NAILGUN = 520;            // Nailgun
float CT_SUPER_NAILGUN = 525;    // Super Nailgun
float CT_GRENADE_LAUNCHER = 530;// Grenade Launcher
float CT_ROCKET_LAUNCHER = 535;    // Rocket Launcher
float CT_LIGHTNING = 540;        // Lightning Gun
float CT_UPGRADE_LG = 545;        // Plasma Gun

float CT_ARTSUIT = 550;            // Environment suit
float CT_ARTPENT = 555;            // Pentagram - Invulnerability
float CT_ARTINVS = 560;            // Ring of Shadows
float CT_ARTQUAD = 565;            // Quad Damage
float CT_ARTSHARP = 570;        // Sharp Shooter
float CT_ARTPIERCE = 575;        // Nail Piercer
float CT_ARTWETSUIT = 580;        // Wet Suit
float CT_ARTTOME = 590;            // Tome of Power

float CT_SILVERKEY = 600;        // Silver key
float CT_GOLDKEY = 605;            // Gold key
float CT_CUSTOMKEY = 610;        // Custom key
float CT_CUSTOMITEM = 615;        // Custom pickup item
float CT_RUNEKEY = 620;            // End of Level rune keys 1-4

float CT_SPIKESHOOTER = 650;    // Trap SPIKE shooter
float CT_GRENADESHOOTER = 652;    // Trap GRENADE shooter
float CT_ROCKETSHOOTER = 654;    // Trap ROCKET shooter
float CT_LIGHTSHOOTER = 656;    // Trap LIGHTNING shooter
float CT_GASSHOOTER = 658;        // Trap GAS shooter
float CT_EXPLO_BOX = 660;        // Exploding box
float CT_PENDULUM = 662;        // Swing Pendulum
float CT_SAWBLADE = 666;        // Saw Blade

float CT_FIREBALL = 670;        // Misc Fireball
float CT_MISCSPARK = 672;        // Misc Spark
float CT_MISCSHAKE = 676;        // Misc Shake of screen/player
float CT_MISCDRIP = 680;        // Misc Drip (water)
float CT_BUBBLE = 682;            // Bubbles
float CT_MISCMODEL = 690;        // Misc Model
float CT_MISCSMOKE = 692;        // Misc Smoke

//----------------------------------------------------------------------
float CT_AMMOPACK = 700;        // Ammo Backpacks
float CT_AMMORANDOM = 702;        // Random Ammo Backpack
float CT_AMMOSHELLS = 710;        // Different ammo types
float CT_AMMONAILS = 720;
float CT_AMMOROCKETS = 730;
float CT_AMMOCELLS = 740;

float CT_HEALROT = 750;            // Different Health packs
float CT_HEALNORM = 755;
float CT_HEALMEGA = 760;

float CT_ARMOR1 = 770;            // Green armour
float CT_ARMOR2 = 775;            // Yellow armour
float CT_ARMORINV = 790;        // Red armour
float ARMOR_BLUE = 2;            // Display a blue armour

//----------------------------------------------------------------------
float CT_FUNCDOOR = 800;        // Func door
float CT_FUNCDOORTRIG = 801;    // Generated touch trigger around door
float CT_FUNCSECDOOR = 805;        // Func SECRET door
float CT_FUNCBUTTON = 810;        // Func button
float CT_FUNCPSWITCH = 812;        // Func Pressure Switch
float CT_FUNCINSVOLUME = 813;    // Func to detect inside volume
float CT_FUNCBOB = 815;            // Func bobbing bmodels
float CT_FUNCTRAIN = 820;        // Func train
float CT_FUNCWALL = 830;        // Func wall
float CT_FUNCILLUSIONARY = 832;    // Func illusionary
float CT_FUNCLASER = 834;        // Func laser
float CT_FUNCSKILL = 836;        // Func_skill pillars

float CT_FUNCBREAK = 840;        // Func breakable (parent)
float CT_FUNCBREAKWALL = 842;    // Bmodel to activate after breakage
float CT_FUNCBREAKMDL = 845;    // Func breakable (model)
float CT_FUNCBREAKSPN = 846;    // Func breakable (spawner)
float CT_FUNCBREAKOBJ = 847;    // Func breakable (rubble))
float CT_FUNCPUSHABLE = 850;    // Func pushable

float CT_FUNCEPISODEGATE = 875;    // Episode gates (not used anymore)
float CT_FUNCBOSSGATE = 880;    // Final boss gate (not used anymore)
float CT_FUNCPLAT = 890;        // Old platform system
float CT_FUNCLIFT = 895;        // New platform system

//----------------------------------------------------------------------
float CT_TRIGONCE = 900;        // Once, volume
float CT_TRIGMULTI = 905;        // Multiple, volume
float CT_TRIGRELAY = 910;        // Relay, point entity no touch
float CT_TRIGRELAYDELAY = 912;    // Relay, used in SUBS for delays
float CT_TRIGDOORSTATE = 914;    // Set func_door to specific state
float CT_TRIGCOUNT = 920;        // Count, point entity no touch
float CR_TRIGENG = 922;            // Engine, point entity no touch
float CT_TRIGRUNES = 925;        // Rune, volume
float CT_TRIGSKILLS = 927;        // Skill, volume
float CT_TRIGTELEPORT = 930;    // Trigger teleport
float CT_MISCTELEPORT = 932;    // info_teleport_destination
float CT_SKULLTELEPORT = 934;    // info_skullwiz_destination
float CT_TRIGEXPLODE = 945;        // Trigger sprite explosion
float CT_TRIGSECRET = 950;        // Secret, volume
float CT_TRIGLADDER = 955;        // Ladder, volume
float CT_TRIGFOG = 957;            // Fog, volume
float CT_TRIGHURT = 960;        // Hurt, volume
float CT_TRIGHEAL = 962;        // Heal, volume
float CT_TRIGGIVEITEM = 964;    // Give items, volume
float CT_TRIGTSOUND = 968;        // Touch sounds, volume
float CT_TRIGPUSH = 970;        // Push, volume
float CT_TRIGCONVEYOR = 972;    // Conveyor, volume
float CT_TRIGLEVEL = 975;        // Change level
float CT_TRIGSKYBOX = 976;        // Change/Load Skybox console command
float CT_TRIGITEMFLAG = 977;    // Change item spawnflags
float CT_TRIGCLMSG = 978;        // Message to all clients
float CT_TRIGCDTRACK = 979;        // Change CD track

float CT_TRIGMONJUMP = 980;        // Triger monster to jump forward
float CT_TRIGNOJUMP = 981;        // Trigger monster to NOT jump!
float CT_TRIGMONDROP = 982;        // Trigger monster to fall naturally
float CT_TRIGMONTURRET = 985;    // Trigger monster to turn into turret
float CT_TRIGMONMOVE = 987;        // Trigger monster movement state
float CT_TRIGMONBREAK = 990;    // Trigger monster/breakable together
float CT_TRIGMONKILL = 992;        // Trigger monster to be killed
float CT_TRIGMONATT = 996;        // Trigger monster to attack something

float CT_TRIGVOID = 999;        // Skybox garbage collector!
//----------------------------------------------------------------------
float AMMO_MAXSHELLS = 200;        // Maximum ammo the player can carry
float AMMO_MAXNAILS = 200;        // Shells use to be 100
float AMMO_MAXROCKETS = 100;
float AMMO_MAXCELLS = 100;

float AMMO_SHELLS_SML = 20;        // Ammo quantities for small / large boxes 
float AMMO_SHELLS_LGR = 40;        // and from weapon pickups
float AMMO_SHELLS_WPN = 5;
float AMMO_NAILS_SML = 25;
float AMMO_NAILS_LGR = 50;
float AMMO_NAILS_WPN = 30;        // Both Nailgun and Super
float AMMO_ROCKETS_SML = 5;
float AMMO_ROCKETS_LGR = 10;
float AMMO_ROCKETS_WPN = 5;        // Both Rocket and Grenade launcher
float AMMO_CELLS_SML = 6;
float AMMO_CELLS_LGR = 12;
float AMMO_CELLS_WPN = 15;

//----------------------------------------------------------------------
float ARMOR_GRN_TYPE = 0.3;        // Armour damage reduction and quantity
float ARMOR_GRN_VALUE = 100;
float ARMOR_YEL_TYPE = 0.6;
float ARMOR_YEL_VALUE = 150;
float ARMOR_RED_TYPE = 0.8;
float ARMOR_RED_VALUE = 200;

//----------------------------------------------------------------------
float HEAL_ROT = 15;            // Health packs
float HEAL_NORM = 25;
float HEAL_MEGA = 100;            // How much extra health to add
float HEAL_PLAYMAX = 100;        // Maximum player health (default)
float HEAL_MEGAMAX = 250;        // Maximum health (several megahealths)

//----------------------------------------------------------------------
// Required here because of link to particle emitters
float MISCBOOK_GREEN = 1;        // Green Cover
float MISCBOOK_BLUE = 2;        // Blue Cover
float MISCBOOK_RED = 3;            // Red Cover

//----------------------------------------------------------------------
string SOUND_EMPTY = "misc/null.wav";            // No sound
string MODEL_EMPTY = "progs/misc_empty.mdl";    // Empty Model (1 point)
string MODEL_BROKEN = "progs/misc_broken.mdl";    // Model error
string MODEL_IMPACT = "progs/misc_impact.mdl";    // Impact Model Marker
string MODEL_CORNER1 = "progs/misc_corner1.mdl";// Path corner helper (Forward)
string MODEL_CORNER2 = "progs/misc_corner2.mdl";// Path corner helper (Backward)
string MODEL_BACKPACK = "progs/w_backpack.mdl";    // HD backpack

string MODEL_PLAYER = "progs/player.mdl";
string MODEL_PLAYER_HEAD = "progs/h_player.mdl";
string MODEL_PLAYER_EYES = "progs/eyes.mdl";

string SOUND_HEAL15 = "items/r_item1.wav";        // pickup sound for rotten health
string SOUND_HEAL25 = "items/health1.wav";        // large health
string SOUND_HEAL100 = "items/r_item2.wav";        // mega health
string SOUND_RESPAWN = "items/itembk2.wav";        // item respawn sound
string SOUND_TALK = "misc/talk.wav";            // Activate/message sound

string SOUND_REXP3 = "weapons/r_exp3b.wav";        // Original explode sound
string SOUND_PLASMA_FIRE = "weapons/plasma_fire.wav";    // Plasma Fire
string SOUND_PLASMA_HIT = "weapons/plasma_hit.wav";        // Plasma Impact

// Resist ammo damage
string SOUND_RESIST_ROCKET = "weapons/resist_rocket.wav";

//----------------------------------------------------------------------
// Projectile models
string MODEL_PROJ_DIAM1 = "progs/proj_diam1.mdl";        // Not used atm
string MODEL_PROJ_DIAM2 = "progs/proj_diam2.mdl";
string MODEL_PROJ_SMOKE = "progs/proj_smoke.mdl";
string MODEL_PROJ_SHELLC = "progs/proj_shellcase.mdl";
string MODEL_PROJ_NG = "progs/proj_ng.mdl";
string MODEL_PROJ_NGRED = "progs/proj_ngred.mdl";
string MODEL_PROJ_SNG = "progs/proj_sng.mdl";
string MODEL_PROJ_GRENADE = "progs/proj_grenade.mdl";
string MODEL_PROJ_GRENADEGRN = "progs/proj_grenadegrn.mdl";
string MODEL_PROJ_RINGSHOCK = "progs/proj_ringshock.mdl";
string MODEL_PROJ_RINGBLAST = "progs/proj_ringblast.mdl";
string MODEL_PROJ_ROCKET = "progs/proj_rocket.mdl";
string MODEL_PROJ_ROCKETGRN = "progs/proj_rocketgrn.mdl";
string MODEL_PROJ_FLESH = "progs/proj_flesh.mdl";
string MODEL_PROJ_FLESHP = "progs/proj_fleshp.mdl";
string MODEL_PROJ_HKN = "progs/proj_dk.mdl";
string MODEL_PROJ_HKNGRN = "progs/proj_dkgrn.mdl";
string MODEL_PROJ_DGUARDQ = "progs/proj_dguardq.mdl";
string MODEL_PROJ_SERG = "progs/proj_serg.mdl";
string MODEL_PROJ_FURY1 = "progs/proj_fury1.mdl";
string MODEL_PROJ_FURY2 = "progs/proj_fury2.mdl";
string MODEL_PROJ_DLORD1 = "progs/proj_dlord1.mdl";
string MODEL_PROJ_NOUR1 = "progs/proj_nour1.mdl";
string MODEL_PROJ_NOUR2B = "progs/proj_nour2b.mdl";
string MODEL_PROJ_NOUR2P = "progs/proj_nour2p.mdl";
string MODEL_PROJ_NOUR2S = "progs/proj_nour2s.mdl";
string MODEL_PROJ_NOUR3 = "progs/proj_nour3.mdl";
string MODEL_PROJ_BLORD1B = "progs/proj_blord1b.mdl";
string MODEL_PROJ_BLORD1S = "progs/proj_blord1s.mdl";
string MODEL_PROJ_BLORDBOLT1 = "progs/proj_blordbolt1.mdl";
string MODEL_PROJ_BLORDBEAM1 = "progs/proj_blordbeam1.mdl";
string MODEL_PROJ_BLORD2B = "progs/proj_blord2b.mdl";
string MODEL_PROJ_BLORD2S = "progs/proj_blord2s.mdl";
string MODEL_PROJ_BLORDBOLT2 = "progs/proj_blordbolt2.mdl";
string MODEL_PROJ_BLORDBEAM2 = "progs/proj_blordbeam2.mdl";
string MODEL_PROJ_SHAMLIT = "progs/proj_shamlit.mdl";
string MODEL_PROJ_WIZ = "progs/proj_wiz.mdl";
string MODEL_PROJ_SPID = "progs/proj_spider.mdl";
string MODEL_PROJ_VORE = "progs/proj_voreling.mdl";
string MODEL_PROJ_SWAMP = "progs/proj_swampling.mdl";
string MODEL_PROJ_SHAL = "progs/proj_shal.mdl";
string MODEL_PROJ_SHALGRN = "progs/proj_shalgrn.mdl";
string MODEL_PROJ_SHOME = "progs/proj_shalhome.mdl";
string MODEL_PROJ_SHOMEGRN = "progs/proj_shalhomegrn.mdl";
string MODEL_PROJ_SBALL = "progs/proj_shalball.mdl";
string MODEL_PROJ_SEGG = "progs/proj_shalegg.mdl";
string MODEL_PROJ_SSHELL = "progs/proj_shalshell.mdl";
string MODEL_PROJ_LAVA = "progs/proj_balllava.mdl";
string MODEL_PROJ_SLIME = "progs/proj_ballslime.mdl";
string MODEL_PROJ_DROLE = "progs/proj_drole.mdl";
string MODEL_PROJ_DROLEGRN = "progs/proj_drolegrn.mdl";
string MODEL_PROJ_GUGBOMB = "progs/proj_gugbomb.mdl";
string MODEL_PROJ_GUGFRAG = "progs/proj_gugfrag.mdl";
string MODEL_PROJ_GUGEXPL = "progs/proj_gugexp.spr";
string MODEL_PROJ_LASER = "progs/proj_laz.mdl";
string MODEL_PROJ_LASERGRN = "progs/proj_lazgrn.mdl";
string MODEL_PROJ_LIGHTNING2 = "progs/proj_lightning2.mdl";
string MODEL_PROJ_PLASMA = "progs/proj_plasma.mdl";
string MODEL_PROJ_PLASMAGRN = "progs/proj_plasmagrn.mdl";
string MODEL_PROJ_PGPLASMA = "progs/proj_pgplasma.mdl";
string MODEL_PROJ_GAPLASMA = "progs/proj_gaplasma.mdl";
string MODEL_PROJ_GAPLASMAGRN = "progs/proj_gaplasmagrn.mdl";
string MODEL_PROJ_BOLT1 = "progs/proj_bolt1.mdl";
string MODEL_PROJ_BOLT2 = "progs/proj_bolt2.mdl";
string MODEL_PROJ_BOLT3 = "progs/proj_bolt3.mdl";
string MODEL_PROJ_SWSKULL = "progs/proj_skullwiz.mdl";
string MODEL_PROJ_SWSKULLP = "progs/proj_skullwizp.mdl";
string MODEL_PROJ_SWBALL = "progs/proj_skullball.mdl";
string MODEL_PROJ_GARGBALL = "progs/proj_gargball.mdl";
string MODEL_PROJ_GARGOYLE = "progs/proj_gargoyle.mdl";
string MODEL_PROJ_GARGBALLGRN = "progs/proj_gargballgrn.mdl";
string MODEL_PROJ_GARGOYLEGRN = "progs/proj_gargoylegrn.mdl";
string MODEL_PROJ_WTRAIL = "progs/proj_wraithtrail.mdl";
string MODEL_PROJ_WBALL = "progs/proj_wraithball.mdl";
string MODEL_PROJ_WBONE = "progs/proj_wraithbone.mdl";
string MODEL_PROJ_WPOISON = "progs/proj_wraithpoison.mdl";
string MODEL_PROJ_WEGG = "progs/proj_wraithegg.mdl";
string MODEL_PROJ_WSHELL = "progs/proj_wraithshell.mdl";
string MODEL_PROJ_SCORP = "progs/proj_scorpion.mdl";
string MODEL_PROJ_MBALL = "progs/proj_minoball.mdl";
string MODEL_PROJ_MPLASMA = "progs/proj_minoplasma.mdl";
string MODEL_PROJ_MPOISON = "progs/proj_minopoison.mdl";
string MODEL_PROJ_GROCK1 = "progs/proj_golemrock1.mdl";
string MODEL_PROJ_GROCK2 = "progs/proj_golemrock2.mdl";
string MODEL_PROJ_EIDO1 = "progs/proj_eidolonrock1.mdl";
string MODEL_PROJ_GSHARD = "progs/proj_golemshard.mdl";
string MODEL_PROJ_RAIND = "progs/xmas/proj_rdrocket.mdl";
string MODEL_PROJ_ELF = "progs/xmas/proj_elfsnow.mdl";
string MODEL_PROJ_SNOWBALL = "progs/xmas/proj_snowball.mdl";
string MODEL_PROJ_CHTHON1 = "progs/proj_chthon1.mdl";
string MODEL_PROJ_SHUB1 = "progs/proj_shub1.mdl";
string MODEL_PROJ_SHUB2 = "progs/proj_shub2.mdl";

//----------------------------------------------------------------------
// Sprites
string SEXP_SMALL = "progs/s_exp_small.spr";        // Original ID one
string SEXP_MED = "progs/s_exp_med.spr";            // From Quoth
string SEXP_BIG = "progs/s_exp_big.spr";            // From Quoth
string SEXP_PLASMA_SMALL = "progs/s_exp_plasma_small.spr";    // Kinn/Marcher
string SEXP_PLASMA_BIG = "progs/s_exp_plasma_big.spr";        // Kinn/Marcher
string SEXP_POISON_SMALL = "progs/s_exp_poison_small.spr";    // Kinn/Marcher
string SEXP_POISON_MED = "progs/s_exp_poison_med.spr";        // From Quoth
string SEXP_ELECTRIC = "progs/s_exp_electric.spr";        // From Quoth
string SEXP_ICE_BIG = "progs/s_exp_ice_big.spr";        // From Ne_ruins
string SBURST_FLAME = "progs/s_burst_flame.spr";        // From Rubicon2
string SBURST_FLAMEDP = "progs/s_burst_flamedp.spr";    // DP collision
string SBURST_POISON = "progs/s_burst_poison.spr";        // From Rubicon2
string SBURST_POISONDP = "progs/s_burst_poisondp.spr";    // DP collision
string SBURST_SMOKE = "progs/s_burst_smoke.spr";        // From Rubicon2
string SBURST_STEAM = "progs/s_burst_steam.spr";        // From Rubicon2
string SBUBBLE_DROWN = "progs/s_bubble.spr";        // Original ID one
string SWATER_DRIP = "progs/s_dripblue.spr";        // From RRP/RMQ
string SWATER_SPLASH = "progs/s_dripsplblue.spr";    // From RRP/RMQ
string SBLOOD_DRIP = "progs/s_dripred.spr";            // Re-painted
string SBLOOD_SPLASH = "progs/s_dripsplred.spr";
string SSLIME_DRIP = "progs/s_dripgreen.spr";        // Re-painted
string SSLIME_SPLASH = "progs/s_dripsplgreen.spr";

//----------------------------------------------------------------------
// Sprite types
float EXPLODE_SMALL = 1;
float EXPLODE_MED = 2;
float EXPLODE_BIG = 3;
float EXPLODE_PLASMA_SMALL = 11;
float EXPLODE_PLASMA_MED = 12;
float EXPLODE_PLASMA_BIG = 13;
float EXPLODE_POISON_SMALL = 21;
float EXPLODE_POISON_MED = 22;
float EXPLODE_POISON_BIG = 23;
float EXPLODE_ELECT_SMALL = 31;
float EXPLODE_ELECT_MED = 32;
float EXPLODE_ELECT_BIG = 33;
float EXPLODE_ICE_BIG = 38;
float EXPLODE_BURST_SMOKE = 41;
float EXPLODE_BURST_FLAME = 42;
float EXPLODE_BURST_POISON = 43;

//----------------------------------------------------------------------
// WORLD weapon models (player)
string MODEL_GWEAP_UPAXE = "progs/g_shadaxe.mdl";    // Has all 6 skins
string MODEL_GWEAP_SG = "progs/g_shot1.mdl";        // From Rubicon
string MODEL_GWEAP_SSG = "progs/g_shot2.mdl";        // New Model update
string MODEL_GWEAP_UPSSG = "progs/g_shot3.mdl";        // SSG Upgrade
string MODEL_GWEAP_NG = "progs/g_nail.mdl";            // Remake from me
string MODEL_GWEAP_SNG = "progs/g_nail2.mdl";
string MODEL_GWEAP_GL = "progs/g_rock.mdl";
string MODEL_GWEAP_RL = "progs/g_rock2.mdl";
string MODEL_GWEAP_LG = "progs/g_light.mdl";
string MODEL_GWEAP_UPLG = "progs/g_plasma.mdl";        // LG Upgrade
//string MODEL_GWEAP_ZSSG = "progs/g_zershot.mdl";

// VIEW weapon models (player)
string MODEL_VWEAP_AXE = "progs/v_shadaxe0.mdl";    // Default, no glow
string MODEL_VWEAP_UPAXE = "progs/v_shadaxe3.mdl";    // Axe Upgrade
string MODEL_VWEAP_SG = "progs/v_shot.mdl";
string MODEL_VWEAP_SSG = "progs/v_shot2.mdl";
string MODEL_VWEAP_UPSSG = "progs/v_shot3.mdl";        // SSG Upgrade
string MODEL_VWEAP_NG = "progs/v_nail.mdl";
string MODEL_VWEAP_SNG = "progs/v_nail2.mdl";
string MODEL_VWEAP_GL = "progs/v_rock.mdl";
string MODEL_VWEAP_RL = "progs/v_rock2.mdl";
string MODEL_VWEAP_LG = "progs/v_light.mdl";
string MODEL_VWEAP_UPLG = "progs/v_plasma.mdl";        // LG Upgrade
//string MODEL_VWEAP_ZSSG = "progs/v_zershot.mdl";

//----------------------------------------------------------------------
// Standard ID artifact sounds
string SOUND_ARTSUIT1 = "items/suit.wav";            // Pick up
string SOUND_ARTSUIT2 = "items/suit2.wav";            // Fading
string SOUND_ARTWETS1 = "items/wetsuit.wav";        // Pick up
string SOUND_ARTWETS2 = "items/wetsuit2.wav";        // Fading
string SOUND_ARTWETS3 = "items/wetsuit3.wav";        // In Water
string SOUND_ARTWETS3B = "items/wetsuit3out.wav";    // Out Water
string SOUND_ARTPENT1 = "items/protect.wav";        // Pick up
string SOUND_ARTPENT2 = "items/protect2.wav";        // Fading
string SOUND_ARTPENT3 = "items/protect3.wav";        // Active
string SOUND_ARTINV1 = "items/inv1.wav";            // Pick up
string SOUND_ARTINV2 = "items/inv2.wav";            // Fading
string SOUND_ARTINV3 = "items/inv3.wav";            // Active
string SOUND_ARTQUAD1 = "items/damage.wav";            // Pick up
string SOUND_ARTQUAD2 = "items/damage2.wav";        // Fading
string SOUND_ARTQUAD3 = "items/damage3.wav";        // Active
string SOUND_ARTSHARP1 = "items/sharp.wav";            // Pick up
string SOUND_ARTSHARP2 = "items/sharp2.wav";        // Fading
string SOUND_ARTSHARP3 = "items/sharp3.wav";        // Active
string SOUND_ARTNAILP1 = "items/nailp.wav";            // Pick up
string SOUND_ARTNAILP2 = "items/nailp2.wav";        // Fading
string SOUND_ARTNAILP3 = "items/nailp3.wav";        // Active

//----------------------------------------------------------------------
// DeBuff sounds
string SOUND_HEARTBEAT = "player/heartbeat.wav";    // Fast heartbeat (1s)

//----------------------------------------------------------------------
// Footstep sounds
string SOUND_FS_SLOW1 = "steps/slow1.wav";
string SOUND_FS_SLOW2 = "steps/slow2.wav";
string SOUND_FS_SLOW3 = "steps/slow3.wav";
string SOUND_FS_SLOW4 = "steps/slow4.wav";
string SOUND_FS_SLOW5 = "steps/slow5.wav";

string SOUND_FS_DRAG1 = "steps/drag1.wav";
string SOUND_FS_DRAG2 = "steps/drag2.wav";
string SOUND_FS_DRAG3 = "steps/drag3.wav";
string SOUND_FS_DRAG4 = "steps/drag4.wav";
string SOUND_FS_DRAG5 = "steps/drag5.wav";

string SOUND_FS_LIGHT1 = "steps/light1.wav";
string SOUND_FS_LIGHT2 = "steps/light2.wav";
string SOUND_FS_LIGHT3 = "steps/light3.wav";
string SOUND_FS_LIGHT4 = "steps/light4.wav";
string SOUND_FS_LIGHT5 = "steps/light5.wav";

string SOUND_FS_MEDIUM1 = "steps/medium1.wav";
string SOUND_FS_MEDIUM2 = "steps/medium2.wav";
string SOUND_FS_MEDIUM3 = "steps/medium3.wav";
string SOUND_FS_MEDIUM4 = "steps/medium4.wav";
string SOUND_FS_MEDIUM5 = "steps/medium5.wav";

string SOUND_FS_HEAVY1 = "steps/heavy1.wav";
string SOUND_FS_HEAVY2 = "steps/heavy2.wav";
string SOUND_FS_HEAVY3 = "steps/heavy3.wav";
string SOUND_FS_HEAVY4 = "steps/heavy4.wav";
string SOUND_FS_HEAVY5 = "steps/heavy5.wav";

string SOUND_FS_LARGE1 = "steps/large1.wav";
string SOUND_FS_LARGE2 = "steps/large2.wav";
string SOUND_FS_LARGE3 = "steps/large3.wav";
string SOUND_FS_LARGE4 = "steps/large4.wav";
string SOUND_FS_LARGE5 = "steps/large5.wav";

string SOUND_FS_GIANT1 = "steps/giant1.wav";
string SOUND_FS_GIANT2 = "steps/giant2.wav";
string SOUND_FS_GIANT3 = "steps/giant3.wav";
string SOUND_FS_GIANT4 = "steps/giant4.wav";
string SOUND_FS_GIANT5 = "steps/giant5.wav";

/* For reference only
float    SVC_BAD                = 0;
float    SVC_NOP                = 1;
float    SVC_DISCONNECT        = 2;
float    SVC_UPDATESTAT        = 3;
float    SVC_VERSION            = 4;
float    SVC_SETVIEW            = 5;
float    SVC_SOUND            = 6;
float    SVC_TIME            = 7;
float    SVC_PRINT            = 8;
float    SVC_STUFFTEXT        = 9;
float    SVC_SETANGLE        = 10;
float    SVC_SERVERINFO        = 11;
float    SVC_LIGHTSTYLE        = 12;
float    SVC_UPDATENAME        = 13;
float    SVC_UPDATEFRAGS        = 14;
float    SVC_CLIENTDATA        = 15;
float    SVC_STOPSOUND        = 16;
float    SVC_UPDATECOLORS    = 17;
float    SVC_PARTICLE        = 18;
float    SVC_DAMAGE            = 19;
float    SVC_SPAWNSTATIC        = 20;
float    SVC_SPAWNBINARY        = 21;
float    SVC_SPAWNBASELINE    = 22;
float    SVC_TEMPENTITY        = 23;
float    SVC_SETPAUSE        = 24;
float    SVC_SIGNONNUM        = 25;
float    SVC_CENTERPRINT        = 26;
float    SVC_KILLEDMONSTER    = 27;
float    SVC_FOUNDSECRET        = 28;
float    SVC_SPAWNSTATICSOUND= 29;
float    SVC_INTERMISSION    = 30;
float    SVC_FINALE            = 31;
float    SVC_CDTRACK            = 32;
float    SVC_SELLSCREEN        = 33;
float    SVC_CUTSCENE        = 34;
*/