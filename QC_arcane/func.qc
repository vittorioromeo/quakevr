/*======================================================================
SIMPLE BMODELS
======================================================================*/

float FUNC_AFRAME = 2;            // Start with A frame (animated textures)
float FUNC_MODCHECK = 16;        // Will remove this entity if THIS mod is active

float FUNC_LASERSOLID = 2;        // Func bmodel blocks on all sides
float FUNC_LASERNODMG = 4;        // Func bmodel does no touch damage

float FUNC_SKILLSTARTOPEN = 1;    // Reverse setup for lighting issues

float BOB_COLLISION = 2;        // Collision for misc_bob
float BOB_NONSOLID = 4;            // Non solid for func_bob

/*======================================================================
/*QUAKED func_wall (0 .5 .8) ? x AFRAME x x MODCHECK STATIC STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
A SOLID bmodel with toggled animated texture
-------- KEYS --------
targetname : trigger entity (works with entity state system)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
AFRAME   : Start with the A frame animated texture
MODCHECK : Will remove this entity if THIS mod is active
STATIC : Turn entity into static upon spawn (frame 0)
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
A SOLID bmodel with toggled animated texture

======================================================================*/
void() func_wall_use = 
{
    // Deal with STARTOFF functionality first
    if (self.spawnflags & ENT_STARTOFF) self.estate_on();
    else {
        // Block USE functionality if state wrong
        if (self.estate & ESTATE_BLOCK) return;

        // toggle alternate textures
        else self.frame = 1 - self.frame;
    }
};

//----------------------------------------------------------------------
void() func_wall_on = 
{
    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);

    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    if (self.switchshadstyle != 0) lightstyle(self.switchshadstyle, "a");
};

//----------------------------------------------------------------------
void() func_wall_off = 
{
    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, "");

    if (self.switchshadstyle != 0) lightstyle(self.switchshadstyle, "m");
};

//----------------------------------------------------------------------
void() func_wall_aframe = { };

//----------------------------------------------------------------------
void() func_wall =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_FUNCWALL;
    self.bsporigin = TRUE;
    self.angles = '0 0 0';
    self.mdl = self.model;
    if (self.spawnflags & FUNC_AFRAME) self.frame = 1;
    if (self.spawnflags & FUNC_MODCHECK) remove(self);

    // Check for static entity option first
    if (self.spawnflags & ENT_SPNSTATIC) {
        self.movetype = MOVETYPE_PUSH;
        self.solid = SOLID_BSP;
        setmodel (self, self.mdl);
        makestatic(self);
    }
    else {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = func_wall_on;
        self.estate_off = func_wall_off;
        self.estate_use = func_wall_use;
        self.estate_aframe = func_wall_aframe;
        if (self.spawnflags & ENT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
};

/*======================================================================
/*QUAKED func_illusionary (0 .5 .8) ? x AFRAME x x x STATIC STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
A NON SOLID bmodel with texture toggle
-------- KEYS --------
targetname : trigger entity (works with entity state system)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
AFRAME : Start with the A frame animated texture
STATIC : Turn entity into static upon spawn (frame 0)
STARTOFF : Requires trigger to activate
-------- NOTES --------
A NON SOLID bmodel with texture toggle

======================================================================*/
void() func_illusionary_on = 
{
    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);

    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, self.mdl);

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;
};

//----------------------------------------------------------------------
void() func_illusionary_aframe = { };

//----------------------------------------------------------------------
void() func_illusionary =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_FUNCILLUSIONARY;
    self.bsporigin = TRUE;
    self.angles = '0 0 0';
    self.mdl = self.model;
    if (self.spawnflags & FUNC_AFRAME) self.frame = 1;

    // Check for static entity option first
    if (self.spawnflags & ENT_SPNSTATIC) {
        self.movetype = MOVETYPE_NONE;
        self.solid = SOLID_NOT;
        setmodel (self, self.mdl);
        makestatic(self);
    }
    else {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = func_illusionary_on;
        self.estate_off = func_wall_off;
        self.estate_use = func_wall_use;
        self.estate_aframe = func_illusionary_aframe;
        if (self.spawnflags & ENT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
};

/*======================================================================
/*QUAKED func_episodegate (0 .5 .8) ? E1 E2 E3 E4 x x x x Not_Easy Not_Normal Not_Hard Not_DM
SOLID bmodel when player has SELECTED rune(s)
-------- KEYS --------
targetname : trigger entity (works with entity state system)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
E1 : Episode 1
E2 : Episode 2
E3 : Episode 3
E4 : Episode 4
-------- NOTES --------
SOLID bmodel when player has SELECTED rune(s)

======================================================================*/
void() func_episodegate_use =
{
    // The RUNE condition has to be active 
    if (query_configflag(self.customkey) == self.customkey) self.estate_on();
    else self.estate_off();
};

//----------------------------------------------------------------------
void() func_episodegate_aframe = { };

//----------------------------------------------------------------------
void() func_episodegate =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_FUNCEPISODEGATE;
    self.bsporigin = TRUE;
    self.angles = '0 0 0';
    self.mdl = self.model;

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Remove any extra spawnkey stuff like skill restrictions
    self.customkey = self.spawnflags & SVR_RUNE_ALL;
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_wall_on;
    self.estate_off = func_wall_off;
    self.estate_use = func_episodegate_use;
    self.estate_aframe = func_episodegate_aframe;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else {
        // Wait 1 frame before checking for rune keys
        self.think = func_episodegate_use;
        self.nextthink = time + 0.1;
    }
};

/*======================================================================
/*QUAKED func_bossgate (0 .5 .8) ? x x x x x x x x Not_Easy Not_Normal Not_Hard Not_DM
A NON SOLID bmodel when player has ALL runes
-------- KEYS --------
targetname : trigger entity (works with entity state system)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
-------- NOTES --------
A NON SOLID bmodel when player has ALL runes

======================================================================*/
void() func_bossgate_use =
{
    // The RUNE condition has to be active 
    if (query_configflag(SVR_RUNE_ALL) == SVR_RUNE_ALL) self.estate_off();
    else self.estate_on();
};

//----------------------------------------------------------------------
void() func_bossgate_aframe = { };

//----------------------------------------------------------------------
void() func_bossgate =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_FUNCBOSSGATE;
    self.bsporigin = TRUE;
    self.angles = '0 0 0';
    self.mdl = self.model;

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_wall_on;
    self.estate_off = func_wall_off;
    self.estate_use = func_bossgate_use;
    self.estate_aframe = func_bossgate_aframe;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else {
        // Wait 1 frame before checking for rune keys
        self.think = func_bossgate_use;
        self.nextthink = time + 0.1;
    }
};

/*======================================================================
 BModel laser (originally from Rubicon2 codebase by JohnFitz)
 - Extended code to have different states and toggle function
 - Added custom/silent sounds for lasers on/off
 - Added on/off message once function
 - Added collision and nodmg spawnflags

/*======================================================================
/*QUAKED func_laser (0 .5 .8) ? x SOLID NODMG x x x STARTOFF x
A togglable laser, hurts to touch, can be used to block players
-------- KEYS --------
targetname  : trigger entity (works with entity state system)
message     : message to display when switched ON
message2    : message to display when switched OFF
wait        : -1 = will display the switching state messages only once
dmg         : damage to do on touch. default 1 per 0.1s
alpha       : alpha value will vary +/- 20% (def=0.5)
sounds      : 1=laser, 2=power, 4=silent, 5=custom
noise1      : switched ON sound
noise2      : switched OFF sound
spr_frame   : 1=Yellow, 2=Green, 4=Red, 8=Blue, 16=Purple, 32=Fire, 64=White
angle       : Direction of particles to move (def=-2 down)
part_limit  : Maximum active quantity of particles (def=25)
part_life   : Lifetime of particle (def=2s)
part_velrand: Random movement of particles (def='4 4 4')
part_vol    : Size of area to spawn particles in (def=bmodel)
wakeup_dist : Distance to wakeup particle emitter (def=768)
spawn_base  : Minimum time frame to spawn particles (def=0.1s)
spawn_rand  : Random time amount to add to spawning times (def=0.1s)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
SOLID    : Will block anything
NODMG    : Touch damage disabled
STARTOFF : Requires trigger to activate
-------- NOTES --------
A togglable laser, hurts to touch, can be used to block players

======================================================================*/
void() func_laser_touch =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.spawnflags & FUNC_LASERNODMG) return;
    if (other.takedamage == DAMAGE_NO) return;
    if (self.attack_finished < time) {
        T_Damage (other, self, self, self.dmg, DAMARMOR);
        self.attack_finished = time + 0.1;
    }
};

//----------------------------------------------------------------------
void() func_laser_think =
{
    if (self.estate == ESTATE_BLOCK) return;
    // Change the density of the alpha to make laser beam flicker
    self.alpha = self.height*0.8 + (self.alpha * (random()*0.4));
    self.nextthink = time + 0.05;
};

//----------------------------------------------------------------------
void() func_laser_on =
{
    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);

    // Switch laser to active
    self.estate = ESTATE_ON;

    // Only play the ON sound and display message when not spawning
    if (self.waitmin == TRUE) {
        // Play sound on extra emitter if sounds been defined
        if (self.sound_emitter && self.noise1 != "")
            sound (self.sound_emitter, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
        if (activator.flags & FL_CLIENT && self.message != "")
            centerprint(activator,self.message);
        if (self.wait == TRUE) self.message = "";
    }
    else self.waitmin = TRUE;
    
    // Got to set solid/movetype before model
    // Otherwise trigger state does not register properly
    if (self.spawnflags & FUNC_LASERSOLID) {
        self.solid = SOLID_BSP;
        self.movetype = MOVETYPE_PUSH;
    }
    else {
        self.solid = SOLID_TRIGGER;
        self.movetype = MOVETYPE_NONE;
    }

    // Add bmodel back to world (visually)
    setmodel (self, self.mdl);

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Switch ON any particle emitter
    if (self.part_emitter) misc_particle_on(self.part_emitter);
    
    // Reset touch and laser blink effect
    self.touch = func_laser_touch;
    self.think = func_laser_think;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() func_laser_off =
{
    // Switch laser to active
    self.estate = ESTATE_OFF;

    // Only play the OFF sound and display message when not spawning
    if (self.waitmin == TRUE) {
        // Play sound on extra emitter if sounds been defined
        if (self.sound_emitter && self.noise2 != "")
            sound (self.sound_emitter, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
        if (activator.flags & FL_CLIENT && self.message2 != "")
            centerprint(activator,self.message2);
        if (self.wait == TRUE) self.message2 = "";
    }
    else self.waitmin = TRUE;

    // Switch OFF any particle emitter
    if (self.part_emitter) misc_particle_off(self.part_emitter);
    
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, "");
};


//----------------------------------------------------------------------
void() func_laser =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_FUNCLASER;        // Classtype
    self.mdl = self.model;                // Save for later (on function)
    if (self.spr_frame > 0 && CheckZeroVector(self.angles)) self.angles = '0 -2 0';
    InitTrigger();                        // Add to work and initialize

    if (!self.alpha) self.alpha = 0.5;    // Lasers always transparent
    self.height = self.alpha;            // Used for think function
    if (!self.dmg) self.dmg = 1;        // Default tickle
    if (self.wait < 0) self.wait = TRUE;// Display message once?
    else self.wait = FALSE;
    self.waitmin = FALSE;                // Spawning no message check

    // Pre-cache sounds and setup sound emitter
    if (self.sounds > 0 && self.sounds < 3) {
        if (self.sounds == 1) {
            self.noise1 = "misc/laseron.wav";
            self.noise2 = "misc/laseroff.wav";
        }
        else if (self.sounds == 2) {
            self.noise1 = "ambience/power_on.wav";
            self.noise2 = "ambience/power_off.wav";
        }
        precache_sound(self.noise1);
        precache_sound(self.noise2);

        // Setup sound emitter where bmodel is correctly located
        self.sound_emitter = spawn();
        self.sound_emitter.origin = bmodel_origin(self);
        setorigin(self.sound_emitter, self.sound_emitter.origin);
        self.classtype = CT_SOUNDEMITTER;    
    }
    
    // Check for particles emitter type
    if (self.spr_frame > 0) {
        self.part_active = PARTICLE_STYLE_FFIELD;
        // Volume/direction need to be set early, just in case
        // the entity starts off and then it has no volume!
        self.part_vol = self.size * 0.5;
        self.part_vel = self.movedir * 4;
        if (self.spawnflags & ENT_STARTOFF) self.lip = PARTICLE_START_OFF;
        else self.lip = PARTICLE_START_ON;
        self.part_emitter = spawn_pemitter(self, self, self.part_active, self.lip);
    }
        
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_laser_on;
    self.estate_off = func_laser_off;
    
    if (self.spawnflags & ENT_STARTOFF) entity_state_off();
    else entity_state_on();
};

/*======================================================================
/*QUAKED func_skill (0 .5 .8) ? STARTOPEN x x x x START_OFF x
This is a special bmodel that changes texture based on current skill level
Will keep checking skill level until triggered (based on door QC code)
-------- KEYS --------
targetname  : trigger entity (works with entity state system)
style       : Skill Level - 0 = easy, 1 = normal, 2 = hard, 3 = nightmare
message2    : Override message to display when bmodel is touched/damaged
target      : Particle emitters to switch on/off
target2     : target(s) to fire when touched/damaged
health      : Can be damaged instead of touched
angle       : movement direction for trigger event
speed       : movement speed (100 default)
lip         : lip remaining at end of move (8 default)
noise1      : Custom sound - Stop moving (doors/drclos4.wav)
noise2      : Custom sound - Start/Loop moving (doors/stndr1.wav)
noise3      : Custom sound - Touching (plats/medplat2.wav)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
STARTOPEN : bmodel works in reverse state (moved to dest at spawn)
STARTOFF  : Starts off and waits for trigger
-------- NOTES --------
Visual skill selection/update function

======================================================================*/
void() func_skill_check =
{
    // If blocked by entity state system do nothing
    if (self.estate & ESTATE_BLOCK) return;

    // Has the skill level been changed?
    if (self.attack_finished != skill) {
        // Stop this entity constantly changing
        self.attack_finished = skill;
        
        if (self.target != "") {
            self.enemy = find(world,targetname,self.target);
            while (self.enemy) {
                // Found a particle emitter?
                if (self.enemy.classtype == CT_PARTICLEEMIT) {
                    if (self.style == skill) misc_particle_on(self.enemy);
                    else misc_particle_off(self.enemy);
                }
                    
                // Keep searching for more targets in chain
                self.enemy = find(self.enemy,targetname,self.target);
            }
        }
            
        // Is the current skill level active?
        if (self.style == skill) self.frame = 0;
        else self.frame = 1;                                                    // Blank texture (off)
    }
    self.nextthink = time + 0.1;
    self.think = func_skill_check;
};

//----------------------------------------------------------------------
void() func_skill_touch =
{
    // If blocked by entity state system do nothing
    if (self.estate & ESTATE_BLOCK) return;
    if (self.spawnflags & ENT_STARTOFF) return;
    if ( !(other.flags & FL_CLIENT) ) return;
    if ( other.health < 1 ) return;

    // If skill level is changing, say so on console
    if (skill != self.style) {
        if (self.message2 != "") sprint(other, self.message2);
        else {
            sprint(other,"Skill level ");
            if (self.style == SKILL_EASY) sprint(other,"EASY");
            else if (self.style == SKILL_NORMAL) sprint(other,"NORMAL");
            else if (self.style == SKILL_HARD) sprint(other,"HARD");
            else sprint(other,"NIGHTMARE");
            sprint(other," has been selected!");
        }
        sprint(other,"\n");
        stuffcmd (other, "bf\n");
        
        // Change skill level
        cvar_set ("skill", ftos(self.style));
        skill = self.style;
        
        // Force quick update
        func_skill_check();
        
        // Any other targets to fire when touch/damaged
        if (self.target2 != "") trigger_strs(self.target2, other);
    }    
    
    // Play touch sound regardless of skill change
    if (self.pausetime < time) {
        sound (self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
        self.pausetime = time + 1;    // No constant sound
    }    
};

//----------------------------------------------------------------------
void() func_skill_killed =
{
    other = damage_attacker;        // The attacker
    self.health = self.max_health;    // Reset health to original value

    // All other exceptions are in the touch function
    func_skill_touch();
};

//----------------------------------------------------------------------
void() func_skill_on =
{
    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);
    
    self.estate = ESTATE_ON;
    func_skill_check();
};

//----------------------------------------------------------------------
void() func_skill_stop =
{
    // Stop sound, ON and particles!
    sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
    // Need to reset local entity time, otherwise timer is broken
    // it because of the SUB_CalcMove function changes it
    self.ltime = time;
    func_skill_on();
};

//----------------------------------------------------------------------
void() func_skill_use =
{
    // Trigger once functionality
    self.estate_use = SUB_Null;
    self.estate = ESTATE_OFF;
    sound (self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
    SUB_CalcMove (self.pos2, self.speed, func_skill_stop);
};

//----------------------------------------------------------------------
void() func_skill_off =
{
    self.estate = ESTATE_OFF;
    self.frame = 1;
    
    // Switch off any particle emitters
    if (self.target != "") {
        self.enemy = find(world,targetname,self.target);
        while (self.enemy) {
            // Found a particle emitter?
            if (self.enemy.classtype == CT_PARTICLEEMIT) 
                misc_particle_off(self.enemy);
                
            // Keep searching for more targets in chain
            self.enemy = find(self.enemy,targetname,self.target);
        }
    }
};

//----------------------------------------------------------------------
void() func_skill =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // Setup default sounds if no custom sounds exist
    if (self.noise1 == "") self.noise1 = "doors/drclos4.wav";
    if (self.noise2 == "") self.noise2 = "doors/stndr1.wav";
    if (self.noise3 == "") self.noise3 = "plats/medplat2.wav";

    //Pre-cache all sounds
    precache_sound(self.noise1);    // stop sound (big clunk sound)
    precache_sound(self.noise2);    // moving sound (door moving)
    precache_sound(self.noise3);    // Touch sound
    
    self.classtype = CT_FUNCSKILL;    // Type
    self.bsporigin = TRUE;            // Bmodel (0,0,0 origin)
    self.mdl = self.model;
    SetMovedir ();
    
    if (!self.speed) self.speed = 100;    // Default movement speed
    if (!self.lip) self.lip = 8;        // End movement distance

    //----------------------------------------------------------------------
    // Is the skill level within proper range?
    if (self.style < SKILL_EASY || self.style > SKILL_NIGHTMARE)
        self.style = 0;                    // Default skill = EASY
    self.attack_finished = -1;            // Always check first time
    self.lefty = cvar("skill");            // Read current setting
    if (self.style != self.lefty) self.frame = 1;
    
    //----------------------------------------------------------------------
    // Add bmodel to world and work out movement positions
    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    setmodel (self, self.mdl);
    setorigin (self, self.origin);    
    setsize (self, self.mins , self.maxs);
    self.pos1 = self.origin;
    self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);
    
    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    //----------------------------------------------------------------------
    // FUNC_SKILL_START_OPEN is design is hide a skill pillar
    // until it is triggered (nightmare skill)
    if (self.spawnflags & FUNC_SKILLSTARTOPEN) {
        setorigin (self, self.pos2);
        self.pos2 = self.pos1;
        self.pos1 = self.origin;
    }
    
    // If the skill pillar is setup with a targetname then wait for
    // a trigger event before becoming active (updates+particles)
    if (self.targetname != "") {
        self.spawnflags = self.spawnflags | ENT_STARTOFF;
        self.estate_use = func_skill_use;
        self.frame = 1;    // Always starts off
    }
    
    // Setup Entity State functionality
    self.use = entity_state_use;
    self.estate_on = func_skill_on;
    self.estate_off = func_skill_off;
    self.estate = ESTATE_OFF;

    // Can either be touched or damaged, never both!
    if (self.health != 0) {
        self.health = self.max_health = 1;
        self.th_die = func_skill_killed;
        self.takedamage = DAMAGE_YES;
        self.touch = SUB_Null;
    }
    else {
        self.touch = func_skill_touch;
        self.takedamage = DAMAGE_NO;
    }

    // Wait random amount for all entities to spawn
    self.nextthink = time + 0.1 + random()*0.5;
    if (self.spawnflags & ENT_STARTOFF) self.think = self.estate_off;
    else self.think = self.estate_on;
};

/*======================================================================
/*QUAKED func_bob (0 .5 .8) ? x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
A SOLID bmodel that gently moves back and forth
-------- KEYS --------
targetname : trigger entity (works with entity state system)
angle    : direction movement, use "360" for angle 0
height   : direction intensity (def=4)
count    : direction cycle timer (def=2s, minimum=1s)
waitmin  : Speed up scale (def=1) 1+=non linear
waitmin2 : Slow down scale (def=0.75)
delay    : Starting time delay (def=0, -1=random)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
A SOLID bmodel that gently moves back and forth

======================================================================*/
void() func_bob_use =
{
    // Deal with STARTOFF functionality first
    if (self.spawnflags & ENT_STARTOFF) self.estate_on();
    else {
        // Toggle state (switch on or disable)
        if (self.estate & ESTATE_BLOCK) self.estate = ESTATE_ON;
        else self.estate = ESTATE_DISABLE;
    }
};

//----------------------------------------------------------------------
void() func_bob_timer =
{
    // Keep ticking in background, use local timer (faster)
    self.think = func_bob_timer;
    if (self.bsporigin) self.nextthink = self.ltime + 0.1;
    else self.nextthink = time + 0.1;
    
    // Do nothing if entity state is off
    if (self.estate & ESTATE_OFF) return;

    // Has the cycle completed?
    if (self.attack_timer < time) {
        // Don't reset bmodel if disabled
        if (self.estate & ESTATE_DISABLE) {
            self.attack_timer = LARGE_TIMER;
            self.lefty = -1;
        }
        else {
            // Setup bob cycle and half way point for slowdown
            self.attack_timer = time + self.count;
            self.distance = time + (self.count * 0.5);
            // Flip direction of bmodel bob
            self.lefty = 1 - self.lefty;
            if (self.lefty < 1) self.t_length = self.height;
            else self.t_length = -self.height;
        }
        // Always reset velocity and flags
        self.velocity = '0 0 0';
        self.flags = 0;
    }
    
    // Is the direction set?
    // This is a block condition to prevent the bmodel moving
    if (self.lefty != -1) {
        // Slow down velocity (gradually)
        if (self.distance < time) 
            self.velocity = self.velocity * self.waitmin2;
        else {
            // Speed up velocity (linear/exponentially)
            self.t_length = self.t_length * self.waitmin;
            self.velocity = self.velocity + (self.movedir * self.t_length);
        }
    }
};

//----------------------------------------------------------------------
void() func_bob_on = 
{
    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);

    self.estate = ESTATE_ON;
    if (self.bsporigin) {
        // Check for solid spawnflag
        if (self.spawnflags & BOB_NONSOLID) {
            self.movetype = MOVETYPE_PUSH;
            self.solid = SOLID_NOT;
        }
        else {
            self.movetype = MOVETYPE_PUSH;
            self.solid = SOLID_BSP;
        }
    }
    else {
        self.movetype = MOVETYPE_FLY;
        if (self.spawnflags & BOB_COLLISION) self.solid = SOLID_BBOX;
        else self.solid = SOLID_NOT;
        self.flags = 0;    // Reset any onground flags
    }
    setmodel (self, self.mdl);
    setsize (self, self.mins , self.maxs);

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;
};

//----------------------------------------------------------------------
void() func_bob_off = 
{
    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, "");
    self.velocity = '0 0 0';
};

//----------------------------------------------------------------------
void() func_bob =
{
    self.classtype = CT_FUNCBOB;
    // Using a custom model?
    if (self.mdl == "") {
        self.bsporigin = TRUE;
        self.mdl = self.model;
    }
    else {
        self.bsporigin = FALSE;
        self.modelindex = 0;
        self.model = "";
    }
    
    SetMovedir ();
    self.movedir = normalize(self.movedir);
    
    if (self.height <=0) self.height = 8;    // Direction intensity
    if (self.count <1) self.count = 2;        // Direction switch timer
    if (self.waitmin <=0) self.waitmin = 1;            // Speed up
    if (self.waitmin2 <=0) self.waitmin2 = 0.75;    // Slow down
    if (self.delay < 0) self.delay = random() + random() + random();
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_bob_on;
    self.estate_off = func_bob_off;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();

    self.think = func_bob_timer;
    self.nextthink = time + 0.1 + self.delay;
};

//----------------------------------------------------------------------
void() misc_bob =
{
    if (self.mdl == "") self.mdl = MODEL_BROKEN;
    precache_model(self.mdl);
    
    func_bob();
};