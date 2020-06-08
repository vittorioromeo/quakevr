/*======================================================================
 INFO ENTITIES
======================================================================*/
void() info_null = { remove(self); };
void() info_notnull = {};
void() info_target = {};

/*======================================================================
/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 24) x x x x x x x x Not_Easy Not_Normal Not_Hard Not_DM Dyn_Easy Dyn_Normal Dyn_Hard Dyn_Night
{ model(":progs/player.mdl"); }
DEFAULT starting point for a player
-------- KEYS --------
target : triggered target(s) when used as a client destination
angle  : Starting view angle
-------- SPAWNFLAGS --------
-------- NOTES --------
DEFAULT starting point for a player

======================================================================*/
void() info_player_start = { 
    self.classtype = CT_SPAWNSP;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED info_player_start2 (0.9 0 0) (-16 -16 -24) (16 16 24) x x x x x x x x Not_Easy Not_Normal Not_Hard Not_DM Dyn_Easy Dyn_Normal Dyn_Hard Dyn_Night
{ model(":progs/player.mdl"); }
Starting point for players with runes or startspawn2 system
-------- KEYS --------
target : triggered target(s) when used as a client destination
angle  : Starting view angle
startspawn2 : Unique spawn location number matching a trigger_changelevel
-------- SPAWNFLAGS --------
-------- NOTES --------
Starting point for players with runes or using the startspawn2 system
The startspawn2 system lets players move between maps using exact location

======================================================================*/
void() info_player_start2 = { 
    self.classtype = CT_SPAWNSP;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 24) x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
{ model(":progs/player.mdl"); }
Starting position for coop games
-------- KEYS --------
target : triggered target(s) when used as a client destination
angle  : Starting view angle
-------- SPAWNFLAGS --------
STARTOFF : Starts off
-------- NOTES --------
Starting position for coop games

======================================================================*/
void() info_player_coop = { 
    self.classtype = CT_SPAWNCOOP;
    if (self.spawnflags & MON_SPAWN_DELAY) self.estate = ESTATE_OFF;
    else self.estate = ESTATE_ON;
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
};

/*======================================================================
/*QUAKED info_player_deathmatch (0 1 1) (-16 -16 -24) (16 16 24) x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
{ model(":progs/player.mdl"); }
Starting position for deathmatch games
-------- KEYS --------
target : triggered target(s) when used as a client destination
angle  : Starting view angle
-------- SPAWNFLAGS --------
STARTOFF : Starts off
-------- NOTES --------
Starting position for deathmatch games

======================================================================*/
void() info_player_deathmatch = { 
    self.classtype = CT_SPAWNDM;
    if (self.spawnflags & MON_SPAWN_DELAY) self.estate = ESTATE_OFF;
    else self.estate = ESTATE_ON;
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
};

/*======================================================================
/*QUAKED info_teleport_destination (1 0.5 0.5) (-8 -8 -8) (8 8 32) x
Destination marker for a teleporter
-------- KEYS --------
targetname : link to trigger_teleport
target : fires when the entities is used as a destination
angles : Pitch Yaw Roll (viewing angle after teleporting)
-------- SPAWNFLAGS --------
-------- NOTES --------
Destination marker for a teleporter

======================================================================*/
void() info_teleport_destination =
{
    self.classtype = CT_MISCTELEPORT;
    self.estate = ESTATE_ON;
    self.mangle = self.angles;
    self.angles = '0 0 0';
    self.origin = self.origin + '0 0 27';
};

/*======================================================================
/*QUAKED info_intermission (1 0.5 0.5) (-16 -16 -16) (16 16 16) NO_INTERMIS SOLID x x x x x x Not_Easy Not_Normal Not_Hard Not_DM
This is the camera point for the intermission
-------- KEYS --------
target : triggered target(s) when used as a viewing camera
mangle : Pitch Yaw Roll
fog_density : fog density (Must be >0 to activate feature)
fog_colour  : fog colours (def=0.1 0.1 0.1)
speed       : fog time to change (def=2s)
-------- SPAWNFLAGS --------
NO_INTERMIS : Will not display when proper intermission active
SOLID       : Intermission camera interacts with world
-------- NOTES --------
mangle = up/down, angle, tilt left/right 'pitch roll yaw'
up/left = negative value, down/right = positive value

======================================================================*/
void() info_intermission =
{
    self.classtype = CT_CAMERA;
    self.estate = ESTATE_ON;
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, MODEL_EMPTY);
    self.view_ofs = '0 0 0';
    self.owner = self;
    
    // Check for any trigger fog parameters
    if (self.fog_density > 0) {
        if (!self.speed) self.speed = 2;
        // Double check its within range
        if (self.fog_density > 1) self.fog_density = 1;
        if (CheckZeroVector(self.fog_colour)) self.fog_colour = '0.1 0.1 0.1';
    }
    // Disable fog trigger if density not setup
    else self.fog_density = 0;
    
};
