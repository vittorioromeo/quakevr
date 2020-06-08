/*======================================================================
 MISC ENTITIES
======================================================================*/
float MISC_STARTOFF = 1;        // Start OFF (use DELAY instead!)

float MISC_FBALLSLIME = 32;        // Slime green fire ball
float MISC_DRIPSILENT = 2;        // misc_drip has no sound on splash
float MISC_DRIPBLOOD = 16;        // blood red drips
float MISC_DRIPSLIME = 32;        // slime green drips
float MISC_SMOKENODPMDL = 2;    // Do not draw smoke model in DP
float MISC_SMOKENODPFX = 4;        // Do not produce any DP smoke effects
float MISC_SMOKENOQSMDL = 8;    // Do not draw smoke model in QS
float MISC_SPARKBLUE = 2;        // misc_spark produces Blue sparks
float MISC_SPARKPALE = 4;        // misc_spark produces Pale Yellow sparks
float MISC_SPARKRED = 8;        // misc_spark produces Red sparks
float MISC_COLLISION = 2;        // misc_model has collision enabled
float MISC_MOVEMENT = 4;        // misc_model can be moved around
float MISC_SHAKEVIEWONLY = 2;    // No velocity movement

float MISC_EXPLBOX_MAX = 5;        // Maximum amount of skins available

/*======================================================================
/*QUAKED misc_explobox (0 0.5 0.8) (-16 -16 0) (16 16 64) x x x x x FLOAT STARTOFF
{ model(":progs/explode_box1.mdl"); }
Large exploding box
-------- KEYS --------
target        : trigger events when box explodes
skin_override : 0=original, 1=rubicon2, 3=plasma, 4=toxic, 5-6=wood
noise         : Explosion sound (def=weapons/r_exp3.wav)
health        : Amount of health before exploding (def=15)
dmg           : Override radius damage (def=160)
-------- SPAWNFLAGS --------
FLOAT    : No drop to floor test
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Large exploding box

/*QUAKED misc_explobox2 (0 0.5 0.8) (-16 -16 0) (16 16 32) x x x x x FLOAT STARTOFF
{ model(":progs/explode_box2.mdl"); }
Small exploding box
-------- KEYS --------
target        : trigger events when box explodes
skin_override : 0=original, 1=rubicon2, 3=plasma, 4=toxic, 5-6=wood
noise         : Explosion sound (def=weapons/r_exp3.wav)
health        : Amount of health before exploding (def=15)
dmg           : Override radius damage (def=160)
-------- SPAWNFLAGS --------
FLOAT    : No drop to floor test
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Small exploding box

/*QUAKED func_explobox (0 0.5 0.8) ? x x x x x x STARTOFF x
Exploding box (bmodel)
-------- KEYS --------
target      : trigger events when box explodes
noise       : Explosion sound (def=weapons/r_exp3.wav)
health      : Amount of health before exploding (def=15)
dmg         : Override radius damage (def=160)
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Exploding box (bmodel)

======================================================================*/

// Animate model skin slowly = 0.3s
//----------------------------------------------------------------------
void()    misc_ebox_0 = [0, misc_ebox_0] {
    if (self.attack_finished < time) {
        self.lefty = 1 - self.lefty;
        self.skin = self.lefty + (self.skin_override * 2); 
        self.nextthink = time + MODEL_ANIM_SPEED;
    }
};

//----------------------------------------------------------------------
void() misc_explod_delay =
{
    // Explosive damage and sound+fx
    T_RadiusDamage (self, self, self.dmg, world, DAMAGEALL);

    // Particle explosion drifting upward
    particle_explode(self.origin, 50+random()*50, 1, self.part_style, PARTICLE_BURST_UPWARD);
    
    // Play original explosion sound
    sound (self, CHAN_WEAPON, self.noise, 1, ATTN_NORM);
    // Special type (hard-coded) of particle explosion
    // that only works if particle count=255 :)
    particle (self.origin, '0 0 0', 0, 255);

    // Check for any explosion triggers
    if (self.target != "") trigger_strs(self.target, other);

    self.origin = self.origin + '0 0 32';
    SpawnExplosion(EXPLODE_SMALL, self.origin, SOUND_REXP3);
    entity_hide(self);
};

// Blow up the box
//----------------------------------------------------------------------
void() misc_explod_fire =
{
    local entity ent_explode;
    
    if (self.attack_finished > time) return;
    self.attack_finished = time + LARGE_TIMER;

    // Save origin for later
    if (self.bsporigin) self.oldorigin = bmodel_origin(self);
    else self.oldorigin = self.origin;
    
    // Switch off everything
    self.estate_off();
    
    // Stop recursive loops with T_RadiusDamage
    // Delay each explosion so its not all at once
    // Spawn the explosion/dmg/fx as a new entity
    // func_explode does not want to set correct origin
    ent_explode = spawn();
    setorigin(ent_explode, self.oldorigin);
    ent_explode.noise = self.noise;
    ent_explode.dmg = self.dmg;
    ent_explode.think = misc_explod_delay;
    ent_explode.nextthink = 0.01 + random()*0.1;
};

//----------------------------------------------------------------------
void() misc_explobox_use =
{
    // If box is setup off, switch on first
    if (self.spawnflags & ENT_STARTOFF) {
        self.spawnflags = self.spawnflags - ENT_STARTOFF;
        self.estate_on();
    }
    // No toggle function, just blow it up!
    else misc_explod_fire();
};

//----------------------------------------------------------------------
void() misc_explobox_on =
{
    // If the box has exploded, do nothing
    if (self.attack_finished > time) return;
    // Stop re-triggering ON state
    if (self.estate == ESTATE_ON) return;

    self.estate = ESTATE_ON;
    if (self.bsporigin) {
        self.solid = SOLID_BSP;
        self.movetype = MOVETYPE_PUSH;
    }
    else {
        self.solid = SOLID_BBOX;
        self.movetype = MOVETYPE_NONE;
    }
    setmodel (self, self.mdl);
    self.skin = self.skin_override*2;
    setsize(self, self.bbmins, self.bbmaxs);
    
    self.th_die = misc_explod_fire;
    self.takedamage = DAMAGE_AIM;
    
    self.nextthink = time + 0.1;
    self.think = misc_ebox_0;
};

//----------------------------------------------------------------------
void() misc_explobox_off = 
{
    self.estate = ESTATE_OFF;
    self.solid = SOLID_NOT;
    self.modelindex = 0;            // Make sure no model
    self.model = "";                // hide model
    self.takedamage = DAMAGE_NO;
    self.th_die = SUB_Null;
    self.think = SUB_Null;
};

//----------------------------------------------------------------------
void() misc_explobox_setup =
{
    if (self.noise == "") self.noise = SOUND_REXP3;
    precache_sound (self.noise);

    self.classtype = CT_EXPLO_BOX;
    self.classgroup = CG_MISCENT;
    if (self.health < 1) self.health = 15;        // ID def = 20
    if (!self.dmg) self.dmg = 160;
    // Check and setup default box skin
    // 0=Original, 1=rubicon2, 3=plasma, 4=toxic
    if (self.skin_override < 1 || self.skin_override > MISC_EXPLBOX_MAX)
        self.skin_override = 0;
    
    // Setup particle explosion colour
    if (self.skin_override == 2) self.part_style = PARTICLE_BURST_BLUE;
    else if (self.skin_override == 3) self.part_style = PARTICLE_BURST_GREEN;
    else self.part_style = PARTICLE_BURST_WHITE;

    // Setup exploding model
    if (self.bsporigin) {
        self.mdl = self.model;        // Save model for later
        setmodel (self, self.mdl);    // set size and link into world
        self.bbmins = self.mins;    // Save bmodel bounds for later
        self.bbmaxs = self.maxs;
    }
    else {
        // Query console variable 'temp1' for model upgrade option.
        // Cannot use global vars because they don't exist at this point
        // Move the new centered exploding models to match old box origin
        // The default is to move all boxes to suit original id maps
        if (!query_configflag(SVR_ITEMOFFSET)) {
            self.oldorigin = self.origin + '16 16 0';
            setorigin(self, self.oldorigin);
        }

        // Setting the angle key in the editor to UP/DOWN = random rotation    
        if (self.angles_y <= 0) self.angles_y = rint(random()*359);

        // Temporarily enable model/bbox for collision test
        // Finalize box location (check drop to floor)
        if( !(self.spawnflags & ITEM_FLOATING) ) {
            setmodel (self, self.mdl);
            setsize(self, self.bbmins, self.bbmaxs);
            self.solid = SOLID_BBOX;
            self.movetype = MOVETYPE_TOSS;    
            self.origin_z = self.origin_z + 6;
            droptofloor();
            if (pointcontents(self.origin) == CONTENT_SOLID) {
                dprint ("\n\b[ExplBox]\b "); dprint (self.classname);
                dprint (" stuck at ("); dprint (vtos(self.origin)); dprint (")\n");
                spawn_marker(self.origin, SPNMARK_YELLOW);
                remove(self);
                return;
            }
        }
    }

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_explobox_on;
    self.estate_off = misc_explobox_off;
    self.estate_use = misc_explobox_use;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};

//----------------------------------------------------------------------
void() misc_explobox =
{
    self.mdl = "progs/explode_box1.mdl";    // maps/b_explob.bsp
    precache_model (self.mdl);
    self.bbmins = '-16 -16 0';
    self.bbmaxs = '16 16 64';
    misc_explobox_setup();
};

void() misc_explobox2 =
{
    self.mdl = "progs/explode_box2.mdl";    // maps/b_explob2.bsp
    precache_model (self.mdl);
    self.bbmins = '-16 -16 0';
    self.bbmaxs = '16 16 32';
    misc_explobox_setup();
};

//----------------------------------------------------------------------
void () func_explobox =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.mdl = self.model;                    // Brushwork version
    self.bsporigin = TRUE;                    // bmodel object
    self.angles = '0 0 0';                    // Make sure no angle twist
    // Cannot be setup floating (always remove flag)
    self.spawnflags = self.spawnflags - (self.spawnflags & ITEM_FLOATING);
    misc_explobox_setup();
};

//======================================================================
/*QUAKED misc_fireball (0 .5 .8) (-8 -8 -8) (8 8 8) x x x x x SLIME STARTOFF x
Lava Balls, with damage on impact
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
speed : vertical speed (default 1000)
dmg   : impact damage (default 5)
delay : base time between spawning fireballs (default 3)
wait  : random time default 5 (= time + self.delay + (random() x self.wait) ) 
-------- SPAWNFLAGS --------
SLIME  : Green slime version (smoke trail)
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
Lava Balls, with damage on impact
*/
//======================================================================
void() misc_lavaball_fly;
void() misc_lavaball_reset =
{
    self.touch = SUB_Null;
    self.flags = 0;
    self.modelindex = 0;            // Make sure no model
    self.model = "";                // Hide model
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    self.velocity = self.avelocity = '0 0 0';
    self.nextthink = time + self.delay + (random() * self.wait);
    self.think = misc_lavaball_fly;
};

//----------------------------------------------------------------------
void() misc_lavaball_touch =
{
    self.touch = SUB_Null;
    if (other.takedamage) T_Damage (other, self, self, self.dmg, DAMARMOR);
    misc_lavaball_reset();
};

//----------------------------------------------------------------------
void() misc_lavaball_fly =
{
    if (self.estate & ESTATE_BLOCK) return;
    
    setmodel (self, self.mdl);
    setorigin (self, self.oldorigin);
    // Remove all fly/onground flags
    self.flags = 0;

    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_TOSS;
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);

    self.velocity_x = (random() * 100) - 50;
    self.velocity_y = (random() * 100) - 50;
    self.velocity_z = self.speed + (random() * 200);

    self.nextthink = time + 5;
    self.think = misc_lavaball_reset;
    self.touch = misc_lavaball_touch;
};

//----------------------------------------------------------------------
void() misc_lavaball_on =
{
    self.estate = ESTATE_ON;
    self.nextthink = time + self.delay + (random() * self.wait);
    self.think = misc_lavaball_fly;
};

//----------------------------------------------------------------------
void() misc_fireball =
{
    if (self.spawnflags & MISC_FBALLSLIME) self.mdl = MODEL_PROJ_SLIME;
    else self.mdl = MODEL_PROJ_LAVA;
    precache_model (self.mdl);
    self.classtype = CT_FIREBALL;
    self.classgroup = CG_MISCENT;
    self.oldorigin = self.origin;
    if (self.speed <= 0) self.speed = 1000;
    if (self.dmg <= 0) self.dmg = 20;
    if (self.wait <= 0) self.wait = 5;
    if (self.delay <= 0) self.delay = 3;
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_lavaball_on;
    if (self.spawnflags & (MISC_STARTOFF | ENT_STARTOFF)) entity_state_off();
    else entity_state_on();
};

/*======================================================================
/*QUAKED air_bubbles (0 .5 .8) (-8 -8 -8) (8 8 8) x x x x x x STARTOFF x
sprite based bubble that floats upward
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
style   : 1-15 (grey,brown1,blue1,green1,red1,brown2,pinkyel,brown3,purp1,purp2,brown4,green2,yellow,blue2,red2)
-------- SPAWNFLAGS --------
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
sprite based bubble that floats upward

======================================================================*/
void() misc_bubble_bob;
void() misc_bubble_remove =
{
    if (other.classtype == self.classtype) return;
    remove(self);
};

//----------------------------------------------------------------------
void() misc_bubble_split =
{
    local entity    bubble;
    
    bubble = spawn();
    bubble.classname = self.classname;
    bubble.classtype = CT_BUBBLE;
    bubble.classgroup = CG_TEMPENT;
    setmodel (bubble, self.mdl);
    setorigin (bubble, self.origin);
    bubble.movetype = MOVETYPE_NOCLIP;
    bubble.solid = SOLID_NOT;
    bubble.velocity = self.velocity;
    bubble.nextthink = time + 0.5;
    bubble.think = misc_bubble_bob;
    bubble.touch = misc_bubble_remove;

    bubble.frame = 1;
    bubble.cnt = 10;
    setsize (bubble, '-8 -8 -8', '8 8 8');

    self.frame = 1;        // Smaller bubble
    self.cnt = 10;
    if (self.waterlevel != 3) remove (self);
};

//----------------------------------------------------------------------
void() misc_bubble_bob =
{
    local float        rnd1, rnd2, rnd3;

    self.cnt = self.cnt + 1;
    if (self.cnt == 4) misc_bubble_split();
    if (self.cnt == 20) { remove(self); return; }

    rnd1 = self.velocity_x + (-10 + (random() * 20));
    rnd2 = self.velocity_y + (-10 + (random() * 20));
    rnd3 = self.velocity_z + 10 + random() * 10;

    if (rnd1 > 10) rnd1 = 5;
    if (rnd1 < -10) rnd1 = -5;
        
    if (rnd2 > 10) rnd2 = 5;
    if (rnd2 < -10) rnd2 = -5;
        
    if (rnd3 < 10) rnd3 = 15;
    if (rnd3 > 30) rnd3 = 25;
    
    self.velocity_x = rnd1;
    self.velocity_y = rnd2;
    self.velocity_z = rnd3;
        
    self.nextthink = time + 0.5;
    self.think = misc_bubble_bob;
};

//----------------------------------------------------------------------
void() misc_bubble_spawn =
{
    local entity bubble;
    if (self.estate & ESTATE_BLOCK) return;
    
    bubble = spawn();
    bubble.classname = self.classname;
    bubble.classtype = CT_BUBBLE;
    bubble.classgroup = CG_TEMPENT;
    bubble.owner = self;
    setmodel (bubble, self.mdl);
    setorigin (bubble, self.origin);
    bubble.movetype = MOVETYPE_NOCLIP;
    bubble.solid = SOLID_NOT;
    bubble.velocity = '0 0 15';
    bubble.nextthink = time + 0.5;
    bubble.think = misc_bubble_bob;
    bubble.touch = misc_bubble_remove;

    bubble.frame = bubble.cnt = 0;
    setsize (bubble, '-8 -8 -8', '8 8 8');
    self.nextthink = time + random() + 0.5;
    self.think = misc_bubble_spawn;
};

// Map hack entry point
void() make_bubbles = { 
    self.mdl = SBUBBLE_DROWN;
    misc_bubble_spawn(); 
};

//----------------------------------------------------------------------
void() misc_bubble_on =
{
    self.estate = ESTATE_ON;
    self.nextthink = time + 1 + random();
    self.think = misc_bubble_spawn;
};

//----------------------------------------------------------------------
void() air_bubbles =
{
    if (deathmatch) { remove(self); return; }
    
    self.mdl = SBUBBLE_DROWN;
    precache_model (self.mdl);
    
    // Allow for custom bubble sprites
    if (self.style > 0) trigger_setup_bubbles();

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_bubble_on;
    if (self.spawnflags & (MISC_STARTOFF | ENT_STARTOFF)) entity_state_off();
    else entity_state_on();
};

/*=============================================================================
 FX drips (based on code from RRP by ijed)
 - Rewritten to not keep spawning endless entities
 - Modified to have on/off/toggle state

/*QUAKED misc_drip (0 .5 .8) (-8 -8 -8) (8 8 8) x SILENT x x BLOOD SLIME STARTOFF x
Falling water drip with splash and sound
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
wait       : random time between drips (=random() + self.wait)
-------- SPAWNFLAGS --------
BLOOD    : Blood red drips
SLIME    : Slime green drips
SILENT   : Don't make any drip sound (good for multiple drips)
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
Falling water drip with splash and sound

=============================================================================*/
void() misc_drip_spawn;
void() misc_drip_reset =
{
    self.touch = SUB_Null;
    self.frame = self.flags = 0;
    setmodel (self, "");
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    self.velocity = self.avelocity = '0 0 0';
    self.nextthink = time + random() + self.wait;
    self.think = misc_drip_spawn;
};

//----------------------------------------------------------------------
// splash animation (runs at 20fps)
void() s_splash1 = [0, s_splash2] {self.nextthink = time+0.05;};
void() s_splash2 = [1, s_splash3] {self.nextthink = time+0.05;};
void() s_splash3 = [2, s_splash4] {self.nextthink = time+0.05;};
void() s_splash4 = [3, s_splash5] {self.nextthink = time+0.05;};
void() s_splash5 = [4, s_splash6] {self.nextthink = time+0.05;};
void() s_splash6 = [5, misc_drip_reset] {self.nextthink = time+0.05;};

//----------------------------------------------------------------------
void() misc_drip_touch =
{
    self.touch = SUB_Null;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    // If water below, shift origin to water surface
    if (self.cnt) self.origin = self.pos1;
    setorigin(self, self.origin + '0 0 12');
    
    // play a random drip sound
    if (!(self.spawnflags & MISC_DRIPSILENT)) {
        self.lip = random() * 3;
        if (self.lip < 1) sound (self, CHAN_AUTO, self.noise1, 1, ATTN_STATIC);
        else if (self.lip < 2) sound (self, CHAN_AUTO, self.noise2, 1, ATTN_STATIC);
        else sound (self, CHAN_AUTO, self.noise3, 1, ATTN_STATIC);
    }    
    
    // small particle effect when hitting something
    particle (self.origin+'0 0 1', '0 0 0.5', self.aflag+random()*4, 5+random()*5);
    // Switch to splash sprite and animate for 5 frames
    setmodel (self, self.headmdl);
    s_splash1();
};

//----------------------------------------------------------------------
// Keep checking while falling for liquid impact
//----------------------------------------------------------------------
void() misc_drip_water =
{
    if (self.attack_finished < time) misc_drip_reset();
    // Extremely simplified water surface check (pre-calculated)
    if (self.origin_z < self.pos1_z) misc_drip_touch();
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
// Setup new drip and wait for touch/death/water
//----------------------------------------------------------------------
void() misc_drip_spawn =
{
    // Is the entity OFF or BLOCKED?
    if (self.estate & ESTATE_BLOCK) return;

    // make sure player distance check is available
    if (!self.enemy) self.enemy = find(world, classname, "player");
    if ( !(self.enemy.flags & FL_CLIENT) ) {
        self.nextthink = time + 0.1;
        self.think = misc_drip_spawn;
        return;
    }
    
    // Is the misc_drip close to the player?
    if (range_distance(self.enemy, TRUE) > self.wakeup_dist) {
        self.nextthink = time + random() + self.wait;
        self.think = misc_drip_spawn;
        return;
    }    
    
    // Move drip to start position and setup sprite    
    setorigin (self, self.oldorigin);
    setmodel (self, self.mdl);
    self.solid = SOLID_TRIGGER;
    if (self.cnt) self.movetype = MOVETYPE_NOCLIP;
    else self.movetype = MOVETYPE_FLY;
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    self.velocity_z = -map_gravity;

    // Drip only lasts 3s, don't want to travel forever
    self.think = misc_drip_reset;
    self.nextthink = time + 3;
    self.touch = misc_drip_touch;

    // Is there any water underneath drip?
    if (self.cnt) {
        self.attack_finished = self.nextthink;
        self.nextthink = time + 0.1;
        self.think = misc_drip_water;
    }
};

//----------------------------------------------------------------------
void() misc_drip_on =
{
    self.estate = ESTATE_ON;
    self.nextthink = time + random() + self.wait;
    self.think = misc_drip_spawn;
};

//----------------------------------------------------------------------
void() misc_drip =
{    
    // Pick type of drip sprite based on spawnflags
    if (self.spawnflags & MISC_DRIPBLOOD) {
        self.mdl = SBLOOD_DRIP;
        self.headmdl = SBLOOD_SPLASH;
        self.aflag = 64;
    }
    else if (self.spawnflags & MISC_DRIPSLIME) {
        self.mdl = SSLIME_DRIP;
        self.headmdl = SSLIME_SPLASH;
        self.aflag = 48;
    }
    else {
        self.mdl = SWATER_DRIP;
        self.headmdl = SWATER_SPLASH;
        self.aflag = 0;
    }
    // Default cache - water
    precache_model (self.mdl);
    precache_model (self.headmdl);
    
    self.noise1 = "misc/drip1.wav";
    self.noise2 = "misc/drip2.wav";
    self.noise3 = "misc/drip3.wav";
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    precache_sound (self.noise3);

    // default frequency to 3 seconds
    if (self.wait <= 0) self.wait = 3;
    // No point dripping if no drippy player around!
    if (self.wakeup_dist <= 0) self.wakeup_dist = 1024;

    self.classtype = CT_MISCDRIP;
    self.classgroup = CG_MISCENT;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    setsize(self, VEC_ORIGIN, VEC_ORIGIN);

    // Check point content for wierd setups (inside liquids/solids)
    self.height = pointcontents(self.origin);
    if (self.height < CONTENT_SOLID) {
        dprint ("\b[MISCDRIP]\b Spawned inside liquid!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }
    // If flush against a ceiling, move slightly down
    else if (self.height == CONTENT_SOLID) {
        self.origin_z = self.origin_z - 2;
    }
    
    // Setup drip particle in the correct start location
    self.oldorigin = self.origin;
    setorigin(self, self.origin);

    // Find out bottom (world) position first
    self.pos1 = self.origin;
    traceline (self.pos1, self.pos1 + '0 0 -4096', TRUE, self);
    self.pos2 = trace_endpos;

    // Only do loop test if water exists below
    self.count = 8;
    if (trace_inwater) self.cnt = TRUE;
    else self.count = 0;

    // Binary divide the distance to find water surface
    while (self.count > 0) {
        // Break out early from loop if <8 from water surface
        if (fabs(self.pos2_z-self.pos1_z) < 8) self.count = 0;
        // Calculate midway point between origin and endtrace
        self.pos3 = self.pos1;
        self.pos3_z = self.pos1_z + ((self.pos2_z - self.pos1_z)*0.5);

        // Test which half has water and shift top/bottom positions
        traceline (self.pos1, self.pos3, TRUE, self);
        if (trace_inwater) self.pos2 = self.pos3;
        else self.pos1 = self.pos3;
        // Only loop a limited amount of times
        self.count = self.count - 1;
    }
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_drip_on;
    if (self.spawnflags & (MISC_STARTOFF | ENT_STARTOFF)) entity_state_off();
    else entity_state_on();
};

// Re-direct to correct entity name
void() fx_drip = { misc_drip(); };

//======================================================================
/*QUAKED misc_steam (.5 .5 .75) (-8 -8 -8) (8 8 8) x x x x x x STARTOFF x
Steam/Smoke particles
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
target     : targeting entity used for custom direction
angles     : 'pitch roll yaw' up/down, angle, tilt left/right 
wait       : time between generation of smoke particles (def=0.1, min=0.01)
delay      : random amount of time delay ( time = wait + delay x random() )
height     : Percentage of velocity distance travelled (def=1, range=0-1+)
-------- SPAWNFLAGS --------
STARTOFF   : Always Starts off and waits for trigger
-------- NOTES --------
Steam/Smoke particles

======================================================================*/



//======================================================================
/*QUAKED misc_smoke (.5 .5 .75) (-8 -8 -8) (8 8 192) x NODPMDL NODPFX NOQSMDL x x STARTOFF x
Smoke model (+DP only smoke effect)
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
target     : targeting entity used for custom direction
angles     : 'pitch roll yaw' up/down, angle, tilt left/right 
exactskin  : 0=Gunsmoke, 1=Soot, 2=Steam, 3=Toxin, 4=Plague, 5=Incense, 6=Lithium, 7=Flames
alpha      : alpha value for model (def=0.65)
wait       : time between generation of smoke particles (def=0.1, min=0.01)
delay      : random amount of time delay ( time = wait + delay x random() )
height     : Percentage of velocity distance travelled (def=1, range=0-1+)
-------- SPAWNFLAGS --------
NODPMDL    : Do not draw smoke model in DP engine
NODPFX     : Do not draw DP smoke particle effect
NOQSMDL    : Do not draw smoke model in QS/Fitz engines
STARTOFF   : Always Starts off and waits for trigger
-------- NOTES --------
Smoke model, +DP only smoke effect (wait/delay/height DP only)

======================================================================*/
void() misc_smoke_model = 
{
    self.count = self.count + 1;
    if (self.count > 59) self.count = 0;
    self.frame = self.count;
    self.think = misc_smoke_model;
    self.nextthink = time + 0.1;
};

void() misc_smoke_on =
{
    self.estate = ESTATE_ON;

    // Switch on particle emitter if was setup
    if (self.part_emitter) misc_particle_on(self.part_emitter);

    // Restore model/size/skin
    if (self.mdl != "") {
        setmodel (self, self.mdl);
        setsize (self, VEC_ORIGIN, VEC_ORIGIN);
        self.skin = self.exactskin;
        self.count = rint(random()*59);
        misc_smoke_model();
    }
};

//----------------------------------------------------------------------
void() misc_smoke_off =
{
    self.estate = ESTATE_OFF;
    // Turn off model if setup
    if (self.mdl != "") {
        self.modelindex = 0;    // Make sure no model
        self.model = "";        // Hide model
    }
};

//----------------------------------------------------------------------
void() misc_smoke_setup =
{
    // If target is setup, calculate new facing angle
    if (self.target != "") {
        if (!self.movetarget)
            self.movetarget = find(world, targetname, self.target);
        
        if (self.movetarget) {
            // Check for a Bmodel object (special origin)
            if (self.movetarget.bsporigin) self.dest1 = bmodel_origin(self.movetarget);
            else self.dest1 = self.movetarget.origin;
            // Calculate facing angle towards target
            self.movedir = normalize(self.dest1 - self.origin);
            self.angles = vectoangles(self.movedir);
            self.angles_y = self.angles_y + 180;
            // Update velocity direction for DP effect
            if (self.part_emitter) self.part_emitter.dpp_vel = self.movedir*self.height;
        }
    }

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_smoke_on;
    self.estate_off = misc_smoke_off;
    if (self.spawnflags & (MISC_STARTOFF | ENT_STARTOFF)) entity_state_off();
    else entity_state_on();
};

//----------------------------------------------------------------------
void() misc_smoke =
{
    self.mdl = "progs/misc_smoke.mdl";
    precache_model (self.mdl);

    self.classtype = CT_MISCSMOKE;
    self.classgroup = CG_MISCENT;
    self.solid = SOLID_NOT;                // No world interaction
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (!self.exactskin) self.exactskin = 0;    // Default = 0
    if (self.wait < 0.1) self.wait = 0.1;
    if (self.delay <= 0) self.delay = 0.1;
    if (self.height <= 0) self.height = 0.5 + random()*0.5;
    // If DP engine active remove particle shadow
    if (engine == ENG_DPEXT) {
        // Originally had ef_additive but produced sorting errors
        self.effects = self.effects + EF_NOSHADOW;
    }
    else {
        // Setup alpha for non DP engines
        if (!self.alpha) self.alpha = 0.5+random()*0.25;
    }

    // Calculate smoke particle movedir from angles
    makevectors(self.angles);
    self.movedir = v_up;
    
    // Setup some random Y axis rotation if nothing set
    if (CheckZeroVector(self.angles)) self.angles_y = rint(random()*360);

    // DP particle effects active?
    if (ext_dppart) {
        // Remove the model if spawnflag set
        if (self.spawnflags & MISC_SMOKENODPMDL) self.mdl = "";
        
        // Spawn particle emitter if particles active and not blocked
        if (query_configflag(SVR_PARTICLES) && !(self.spawnflags & MISC_SMOKENODPFX) ) {
            self.part_active = PARTICLE_STYLE_SMOKE;
            if (self.spawnflags & (MISC_STARTOFF | ENT_STARTOFF)) 
                self.part_emitter = spawn_pemitter(self, self, self.part_active, PARTICLE_START_OFF);
            else self.part_emitter = spawn_pemitter(self, self, self.part_active, PARTICLE_START_ON);
        }
    }
    else {
        // Check for QS model exception
        if (self.spawnflags & MISC_SMOKENOQSMDL) self.mdl = "";
    }
    
    // Setup target and delay starting model animation
    self.nextthink = time + 0.1 + (rint(random()*10) * 0.1);
    self.think = misc_smoke_setup;
};

/*======================================================================
 spark effect (based on code from Rubicon2 by JohnFitz)
 - Modified to have on/off/toggle state via triggers
 - extended parameters for angle/speed/custom sounds

/*QUAKED misc_spark (.5 .75 .5) (-8 -8 -8) (8 8 8) x BLUE PALE RED x x STARTOFF x
Produces a burst of sparks at random intervals
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
target : If target is a light, will be switched on/off in sync
wait   : time delay between bursts Def=2, spark once=-1
cnt    : number of sparks in burst (0.5 + random() x 0.5) Def=16
angle  : direction of sparks to follow, use "360" for 0
fixangle: 1 = Random Y axis direction of sparks
speed  : velocity speed of sparks (def=40)
height : random velocity modifier (def=+/-20)
sounds : 1=sparks, 4=silent, 5=custom
noise  : custom sound for sparks
-------- SPAWNFLAGS --------
BLUE   : sparks are blue in colour (def=yellow)
PALE   : sparks are pale yellow in colour (def=yellow)
RED    : sparks are red in colour (def=yellow)
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
Produces a burst of sparks at random intervals
If targeting a light, it must start switched off (lights spawnflag=1)

======================================================================*/

void()    misc_sparks_fade1 = [0, misc_sparks_fade2] {self.alpha = 0.8; self.nextthink = time + 0.05;};
void()    misc_sparks_fade2 = [0, misc_sparks_fade3] {self.alpha = 0.6; self.nextthink = time + 0.05;};
void()    misc_sparks_fade3 = [0, misc_sparks_fade4] {self.alpha = 0.4; self.nextthink = time + 0.05;};
void()    misc_sparks_fade4 = [0, SUB_Remove] {self.alpha = 0.2; self.nextthink = time + 0.05;};

//----------------------------------------------------------------------
void() misc_spark_spawn;
void() misc_spark_switchoff = 
{
    // Always switch off trigger (lights)
    SUB_UseTargets();

    // Is the entity OFF or BLOCKED?
    if (self.estate & ESTATE_BLOCK) return;
    // Only spark once (wait for trigger)
    if (self.wait > 0) {
        self.think = misc_spark_spawn;
        self.nextthink = time + self.wait + (random()*self.wait);
    }
}

//----------------------------------------------------------------------
void() misc_spark_spawn =
{
    local float loopvar;
    local entity spark;

    // Is the entity OFF or BLOCKED?
    if (self.estate & ESTATE_BLOCK) return;

    // Check for random rotation, most be set for whole batch
    // otherwise the sparks will all go in different directions
    if (self.fixangle > 0) {
        self.angles = '0 0 0';
        self.angles_y = rint(random()*360);
        makevectors (self.angles);
        self.movedir = v_forward;
    }
    
    // Work out how many sparks to spawn
    loopvar = (0.5 + random()*0.5)*self.cnt;
    while (loopvar > 0) {
        spark = spawn();
        spark.classtype = CT_TEMPSPARK;
        spark.classgroup = CG_TEMPENT;
        spark.owner = self;
        spark.movetype = MOVETYPE_BOUNCE;
        spark.solid = SOLID_TRIGGER;
        setmodel (spark, self.mdl);
        setorigin (spark, self.origin);
        setsize (spark, VEC_ORIGIN, VEC_ORIGIN);
        spark.gravity = 0.3;
        spark.velocity = vecrand(0,self.height,TRUE);
        spark.velocity = spark.velocity + (self.movedir * self.speed);
        spark.avelocity = '300 300 300';
        spark.nextthink = time + 0.5 + 1.5*random();
        spark.think = misc_sparks_fade1;
        
        // If DP engine active remove particle shadow
        if (engine == ENG_DPEXT) spark.effects = spark.effects + EF_NOSHADOW;

        // Some brightness variety
        if (random() < 0.33) spark.skin = 0;
        else if (random() < 0.5) spark.skin = 1;
        else spark.skin = 2;

        // Alternative colours (blue, pale yellow & red)
        if (self.spawnflags & MISC_SPARKBLUE) spark.skin = spark.skin + 3;
        else if (self.spawnflags & MISC_SPARKPALE) spark.skin = spark.skin + 6;
        else if (self.spawnflags & MISC_SPARKRED) spark.skin = spark.skin + 9;
            
        loopvar = loopvar - 1;
    }
    // Play any spark sound and switch ON any target lights
    if (self.noise != "") sound (self, CHAN_VOICE, self.noise, self.volume, self.distance);
    
    // Is there any target(s) to switch on
    if (self.target) {
        SUB_UseTargets();
        // Setup timer to switch off
        self.nextthink = time + 0.1 + random() * 0.2;
        self.think = misc_spark_switchoff;
    }
    else {
        // Only spark once (wait for trigger)
        if (self.wait > 0) {
            self.nextthink = time + 0.2 + self.wait + (random()*self.wait);
            self.think = misc_spark_spawn;
        }
    }
};

//----------------------------------------------------------------------
void() misc_spark_on =
{
    self.estate = ESTATE_ON;
    self.nextthink = time + 0.1 + random();
    self.think = misc_spark_spawn;
};

//----------------------------------------------------------------------
void() misc_spark =
{
    self.mdl = "progs/misc_spark.mdl";
    precache_model (self.mdl);
    if (self.sounds == 1) self.noise = "misc/spark.wav";
    if (self.noise != "") precache_sound (self.noise);
    
    // Allow for volume/atten to be customized
    if (self.distance < 0.1 || self.distance > ATTN_STATIC) self.distance = ATTN_STATIC;
    if (self.volume < 0.1 || self.volume > 1) self.volume = 1;
    
    self.classtype = CT_MISCSPARK;
    self.classgroup = CG_MISCENT;
    self.solid = SOLID_NOT;                // No world interaction
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (!self.wait) self.wait = 2;        // -1 = spark once and turn off
    if (!self.cnt) self.cnt = 16;
    if (!self.speed) self.speed = 40;
    if (!self.height) self.height = 20;
    self.estate = ESTATE_OFF;

    // Always convert 0 angle to 360 for setmovedir function
    if (CheckZeroVector(self.angles)) self.angles = '0 360 0';
    self.mangle = self.angles;
    SetMovedir();

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_spark_on;
    if (self.spawnflags & (MISC_STARTOFF | ENT_STARTOFF)) entity_state_off();
    else entity_state_on();
};

/*======================================================================
Screen Shake (based on code from RRP by ijed/supa)
 - Modified to have on/off/toggle state
 - added extra sound options

/*QUAKED misc_shake (.5 .5 .9) (-16 -16 -8) (16 16 8) x VIEWONLY x x x x x x
Shake players view and/or velocity around center of entity
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
count  : radius of shake (def = 200)
wait   : duration of shake (def = 2s)
dmg    : strength at center (def = 200)
sounds : 1=loud rumble (no default)
noise1 : noise to play when starting to shake
noise2 : noise to play when stopping
-------- SPAWNFLAGS --------
VIEWONLY : Shakes the view, but player movement is not affected
-------- NOTES --------
Shake players view and/or velocity around center of entity.
Always starts off, requires triggers to activate

======================================================================*/
void() misc_shake_think =
{
    local entity plyr;
    local float    d;

    // Is the shaking over?
    if (self.attack_finished < time || self.estate & ESTATE_BLOCK) {
        if (self.noise2) sound (self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
        // Check state before changing it, entity may be disabled
        if (self.estate == ESTATE_ON) self.estate = ESTATE_OFF;
        return;
    }
    
    // Create a list of entities to check for players
    plyr = findradius(self.origin, self.count);
    while(plyr) {
        // Only shake players (clients)
        if (plyr.flags & FL_CLIENT) {    
            // Scale effect by distance
            d = vlen(self.origin - plyr.origin);
            d = (self.count - d)/self.count;
            
            if (d > 0) {
                // shake up the view
                plyr.punchangle_x = -1 * (random() + (0.025*self.dmg*d));
                
                // push the player around
                if (plyr.flags & FL_ONGROUND && !(self.spawnflags & MISC_SHAKEVIEWONLY)) {
                    d = self.dmg*d;
                    plyr.velocity_x = plyr.velocity_x + (random()*d*2 - d);
                    plyr.velocity_y = plyr.velocity_y + (random()*d*2 - d);
                    plyr.velocity_z = plyr.velocity_z + (random()*d);
                }    
            }
        }
        // Find next entity in chain
        plyr = plyr.chain;
    }
    
    // keep shaking!
    self.nextthink = time + 0.1;
    self.think = misc_shake_think;
};

//----------------------------------------------------------------------
void() misc_shake_on = 
{
    self.estate = ESTATE_ON;

    // Play earthquake LOOP sound
    if (self.noise1) sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
    self.attack_finished = time + self.wait;

    // keep checking for players to shake!
    self.nextthink = time + 0.1;
    self.think = misc_shake_think;
};

//----------------------------------------------------------------------
void() misc_shake =
{    
    if (self.sounds == 1) {
        self.noise1 = "misc/rumbleloop.wav";
        self.noise2 = "misc/rumbleoff.wav";
    }
    else {
        if (self.noise1 == "") self.noise1 = SOUND_EMPTY;
        if (self.noise2 == "") self.noise2 = SOUND_EMPTY;
    }
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    
    self.classtype = CT_MISCSHAKE;
    self.classgroup = CG_MISCENT;
    if (!self.dmg) self.dmg = 120;
    if (self.count <= 0) self.count = 200;
    if (self.wait <= 0) self.wait = 2;
    self.attack_finished = 0;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_shake_on;
    self.estate = ESTATE_OFF;
};

/*======================================================================
/*QUAKED misc_model (1 .5 .25) (-16 -16 -16) (16 16 16) x COLLISION MOVEMENT x x STATIC STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
MDL files that can be setup with specific frame/skin and animate groups
-------- KEYS --------
mdl : specify model to load, full path (progs/candle.mdl)
targetname : toggle state (use trigger ent for exact state)
angle  : facing angle (-1 = random position)
angles : 'pitch yaw roll' up/down, angle, tilt left/right 
ideal_yaw : = 1 Setup model with random Y axis rotation
pos1 : used for selection of frame(s) has several setups
  X=0, Y=0, Z=exact frame number
  X->Y, Z=0 Randomly pick a frame from the X,Y range
  X->Y, Z=-1 Animate between the X,Y range, can forward or backward setup
pos2 : used for the selection of skin(s) has several setups
  X=0, Y=0, Z=exact skin number
  X->Y, Z=0 Randomly pick a skin from the X,Y range
-------- SPAWNFLAGS --------
COLLISION : model bbox collision enabled
MOVEMENT : model can be moved around like an item
STATIC : Turn entity into static upon spawn (frame 0)
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
MDL files that can be setup with specific frame/skin and animate groups

======================================================================*/
void() misc_model_loop =
{
    if (self.estate & ESTATE_BLOCK) return;
    
    self.height = self.height + self.lip;
    if (self.lip > 0 && self.height > self.pos1_y) self.height = self.pos1_x;
    else if (self.lip < 0 && self.height < self.pos1_y) self.height = self.pos1_x;
    
    self.frame = self.height;
    self.think = misc_model_loop;
    self.nextthink = time + self.speed;
};

//----------------------------------------------------------------------
void() misc_model_on =
{
    // Check if there is any spawn/on delay setup
    // Useful for delay spawning models on to platform/doors
    if (self.delay > 0) {
        // Feed back into this function
        self.think = misc_model_on;
        self.nextthink = time + self.delay;
        // The delay only works once
        self.delay = 0;
        return;
    }
    
    self.estate = ESTATE_ON;

    setmodel (self, self.mdl);
    // Restore model/size and check if collision needed
    if (self.spawnflags & MISC_COLLISION) {
        self.solid = SOLID_BBOX;
        setsize (self, self.mins , self.maxs);
    }
    else setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(self, self.oldorigin + self.view_ofs);

    if (self.spawnflags & MISC_MOVEMENT) {
        // Turn misc model into an item (can work with plats/doors)
        self.movetype = MOVETYPE_TOSS;
        self.solid = SOLID_TRIGGER;
        setsize (self, VEC_ORIGIN, VEC_ORIGIN);
        self.velocity = '0 0 0';
        // Make sure the misc model starts on the ground
        self.origin_z = self.origin_z + 6;
        if (!droptofloor()) {
            dprint ("\n\b[Model]\b "); dprint (self.mdl);
            dprint (" stuck at ("); dprint (vtos(self.origin)); dprint (")\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            remove(self);
            return;
        } 
        // Make sure no touch function is active
        self.touch = SUB_Null;
    }
    
    // Setup skin number (range or exact)    
    self.skin = self.pos2_z;
    // Exact frame number
    self.frame = self.pos1_z;

    // Check for static entity option first
    if (self.spawnflags & ENT_SPNSTATIC) makestatic(self);
    else {
        // Check for manual animation loops    
        if (self.pos1_z == -1) {
            self.height = self.pos1_x;
            if (self.pos1_x < self.pos1_y) self.lip = 1;
            else self.lip = -1;
            // Manually animate model
            misc_model_loop();
        }
    }
};

//----------------------------------------------------------------------
void() misc_model_off =
{
    // Turn off model/world interaction
    self.estate = ESTATE_OFF;
    self.model = string_null;                // hide bmodel surface
    self.movetype = MOVETYPE_NONE;            // Create baseline
    self.solid = SOLID_NOT;
    self.nextthink = 0;
};

//----------------------------------------------------------------------
void() misc_model =
{
    // Is the model defined using the noise key?
    if (!self.mdl) {
        dprint("\b[MISCMDL]\b Missing model to load\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }

    precache_model (self.mdl);
    self.classtype = CT_MISCMODEL;
    self.classgroup = CG_MISCENT;
    self.solid = SOLID_NOT;                // No world interaction
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    self.oldorigin = self.origin;        // Store for later
        
    // Random Rotation : UP/DOWN angle or use the ideal_yaw key
    if (self.angles_y < 0 || self.ideal_yaw > 0) 
        self.angles_y = rint(random()*360);
    self.mangle = self.angles;            // Save for later
    if (!self.speed) self.speed = 0.1;    // Manual animation tick speed
        
    // Has a frame range been defined?
    if (self.pos1_x != self.pos1_y) {
        // Make sure the range is the right way around
        // X has to be the lowest number of the two (X/Y)
        if (self.pos1_x > self.pos1_y) {
            self.frame_box = self.pos1_x;
            self.pos1_x = self.pos1_y;
            self.pos1_y = self.frame_box;
        }
        // Randomly pick frame number from a range? 
        // Work out random different and add to X base
        if (self.pos1_z == 0) {
            // Double check lower limit is not negative
            if (self.pos1_x < 0) self.pos1_x = 0;
            // Work out random range first and then add to base
            self.frame_box = fabs(self.pos1_y - self.pos1_x);
            self.pos1_z = self.pos1_x + rint(random()*self.frame_box);
            // Double check the frame is within the specified range
            if (self.pos1_z < self.pos1_x) self.pos1_z = self.pos1_x;
            if (self.pos1_z > self.pos1_y) self.pos1_z = self.pos1_y;
        }
        // Manual frame animation required
        else self.pos1_z = -1;
    }
    else {
        // If no exact frame bas specified, reset frame to default = 0
        if (self.pos1_z < 1) self.pos1 = '0 0 0';
    }
        
    // Has a skin range been defined?
    if (self.pos2_x != self.pos2_y) {
        self.pos2_z = rint( random() * fabs(self.pos2_y - self.pos2_x) );
    }
    else {
        // If no exact frame bas specified, reset frame to default = 0
        if (self.pos2_z < 1) self.pos2 = '0 0 0';
    }

    // Cannot have static and movement at the same time!
    if (self.spawnflags & MISC_MOVEMENT) {
        self.spawnflags = self.spawnflags - (self.spawnflags & ENT_SPNSTATIC);
    }
    
    // Check for static entity option first
    if (self.spawnflags & ENT_SPNSTATIC) misc_model_on();
    else {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = misc_model_on;
        self.estate_off = misc_model_off;
        if (self.spawnflags & (MISC_STARTOFF | ENT_STARTOFF)) entity_state_off();
        else entity_state_on();
    }
};

//----------------------------------------------------------------------
// Allows GTK editors to work with Q1 assets easier
void() misc_gtkmodel = { misc_model(); };

//======================================================================
// All dead bodies use the same on/off states
//======================================================================
void() misc_deadbody_on =
{
    if (self.gibbed == TRUE) return;

    self.estate = ESTATE_ON;            // Show entity
    self.solid = SOLID_NOT;                // No world interaction
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    setmodel(self,self.mdl);            // Show model
    setsize (self, self.bbmins, self.bbmaxs);
    self.bodyonflr = MON_ONFLR;            // Let Shadow Axe interact
};

//----------------------------------------------------------------------
void() misc_deadbody_off =
{
    if (self.gibbed == TRUE) return;

    self.estate = ESTATE_OFF;        // Hide entity
    self.solid = SOLID_NOT;            // No world interaction
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    setmodel(self,"");                // Show model
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    self.bodyonflr = "";            // No Shadows Axe interaction
};

//----------------------------------------------------------------------
void() misc_deadbody_setup =
{
    self.classtype = CT_MISCMODEL;
    self.classgroup = CG_MISCENT;
    self.oldorigin = self.origin;        // Store for later

    self.takedamage = DAMAGE_NO;        // No projectile interaction
    self.deadflag = DEAD_DEAD;            // Body is really dead!
    self.health = self.max_health = -1;
    self.blockudeath = TRUE;            // Body is dead, no human death noise
    self.gibbed = FALSE;                // Still in one piece!
    
    // Random Rotation : UP/DOWN angle or use the ideal_yaw key
    if (self.angles_y < 0 || self.ideal_yaw > 0) 
        self.angles_y = rint(random()*360);
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_deadbody_on;
    self.estate_off = misc_deadbody_off;
    if (self.spawnflags & ENT_STARTOFF) entity_state_off();
    else entity_state_on();
};

/*======================================================================
/*QUAKED misc_player (1 .5 .25) (-16 -16 -24) (16 16 32) BACK DOWN1 WALL DOWN2 DOWN3 SIDE STARTOFF x
Dead Player MDL for poses
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
angle  : facing angle (-1 = random position)
ideal_yaw : = 1 Setup model with random Y axis rotation
fixangle : 1 = Use misc_player.mdl instead (less hassle)
frame  : body pose (49=on back, 67-69=against wall, 60/84/93=face down, 102=on side)
exactskin : -1= Random, 0-1 Original, 2-3 Green, 4-5 Yellow, 6-7 Red
-------- SPAWNFLAGS --------
BACK  : 49=On back
DOWN1 : 60=Face Down
WALL  : 69=Against wall
DOWN2 : 84=Face Down
DOWN3 : 93=Face Down
SIDE  : 102=On Side
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
Dead Player MDL for poses
*/
/*======================================================================
/*QUAKED misc_demon (1 .5 .25) (-32 -32 -24) (32 32 64) x x x x x x STARTOFF x
Dead demon/fiend for poses
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
angle  : facing angle (-1 = random position)
ideal_yaw : = 1 Setup model with random Y axis rotation
frame : body pose (53=on back - def)
exactskin : -1= Random, 0= Original, 1= Green
-------- SPAWNFLAGS --------
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
Dead demon/fiend for poses
*/
/*======================================================================
/*QUAKED misc_dknight (0.75 0.25 0) (-16 -16 -24) (16 16 40) x x x x x x STARTOFF x
Dead Death Knight for poses
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
angle  : facing angle (-1 = random position)
ideal_yaw : = 1 Setup model with random Y axis rotation
frame : body pose (223=on front, 237-243=on back - def)
-------- SPAWNFLAGS --------
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
Dead Death Knight for poses
*/

//----------------------------------------------------------------------
void() misc_player =
{
    self.mdl = "progs/player.mdl";                // Standard player model
    self.headmdl = "progs/h_soldier.mdl";        // Ugly monster mug
    self.gib1mdl = "progs/gib_soldfoot1.mdl";    // Upright foot
    self.gib2mdl = "progs/gib_soldfoot2.mdl";    // Fallen down foot

    // Special player model?
    if (self.fixangle) {
        self.mdl = "progs/misc_player.mdl";        // Custom model
        self.gib3mdl = "progs/gib_5.mdl";        // Gib for precache
        // using special model, simple frame setup
        if (self.spawnflags & 2) self.frame = 1;
        else if (self.spawnflags & 4) self.frame = 2;
        else if (self.spawnflags & 8) self.frame = 3;
        else if (self.spawnflags & 16) self.frame = 4;
        else if (self.spawnflags & 32) self.frame = 5;
        // Must always reset frame
        else self.frame = 0;
    }
    // Normal player model
    else {
        self.mdl = "progs/player.mdl";            // Standard player model
        self.gib3mdl = "progs/w_soldiergun.mdl";// Unique weapon
        self.gib3sound = GIB_IMPACT_WOOD;
        // Using original model, skip to correct frames
        if (self.spawnflags & 1) self.frame = 49;
        else if (self.spawnflags & 2) self.frame = 60;
        else if (self.spawnflags & 4) self.frame = 69;
        else if (self.spawnflags & 8) self.frame = 84;
        else if (self.spawnflags & 16) self.frame = 93;
        else if (self.spawnflags & 32) self.frame = 102;
    }
    
    // Make sure stuff is cached
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    
    // uses short red knight bounding box
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    monster_bbox();

    // Setup random/exact skin choices
    if (self.exactskin < 0) self.exactskin = rint(0.5 + random()*7);
    if (self.exactskin > 7) self.exactskin = 7;
    self.skin = self.exactskin;

    misc_deadbody_setup();
};

//----------------------------------------------------------------------
void() misc_demon =
{
    self.mdl = "progs/mon_demon.mdl";
    self.headmdl = "progs/h_demon.mdl";
    self.gib1mdl = "progs/gib_dmleg1.mdl";        // Left leg
    self.gib2mdl = "progs/gib_dmleg2.mdl";        // Right leg
    self.gib3mdl = "progs/gib_dmtail.mdl";        // Tail
    self.gib4mdl = "progs/gib_dmclaw1.mdl";        // Claw 1
    self.gib5mdl = "progs/gib_dmclaw2.mdl";        // Claw 2

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);        // Always precache extra models
    precache_model (self.gib5mdl);        // regardless if picked or not
    
    // Randomly swap in demon claws instead of legs
    if (random() < 0.5) self.gib1mdl = self.gib4mdl;
    if (random() < 0.5) self.gib2mdl = self.gib5mdl;
    self.gib4mdl = ""; self.gib5mdl = "";

    // Setup bounding box based on presets
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    monster_bbox();
    
    // Default pose for dead demons
    if (self.frame < 0) self.frame = 0;
    if (self.frame == 0) self.frame = 53;
    
    // Setup random/exact skin choices
    if (self.exactskin < 0) {
        if (random() < 0.5) self.exactskin = 0;
        else self.exactskin = 1;
    }
    if (self.exactskin > 1) self.exactskin = 1;
    self.skin = self.exactskin;
    // Sync gib model skin to demon skin
    self.gib1skin = self.gib2skin = self.gib3skin = self.exactskin;

    misc_deadbody_setup();    
};

//----------------------------------------------------------------------
void() misc_dknight =
{
    self.mdl = "progs/mon_dknight.mdl";            // New Hell Knight
    self.headmdl = "progs/h_dknight.mdl";
    self.gib1mdl = "progs/w_dknightsword.mdl";    // Unique sword
    self.gib2mdl = "progs/gib_knfoot_l.mdl";    // left foot
    self.gib3mdl = "progs/gib_knfoot_r.mdl";    // right foot

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib1mdl);
    
    self.gib1sound = GIB_IMPACT_METALA;
    if (random() < 0.5) self.gib2mdl = string_null;
    if (random() < 0.5) self.gib3mdl = string_null;

    // Setup bounding box based on presets
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    monster_bbox();
    
    // Default pose for dead death knights
    if (self.frame < 0) self.frame = 0;
    if (self.frame == 0) self.frame = 243;
    if (self.exactskin > 0) self.skin = self.exactskin;

    misc_deadbody_setup();    
};

/*======================================================================
/*QUAKED misc_builtineffect (0 .5 .8) (-8 -8 -8) (8 8 8) x
Spawns a builtin particle effect, will toggle if active
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
target  : destination of effect (self -> target)
wait    : time between firing of effect (def=0)
delay   : random time between firing of effect (def=0s)
sounds  : = 4 silent (no default additional sounds)
count   : type of effect to fire
          0=TE_SPIKE (def), 1=TE_SUPERSPIKE, 2=TE_GUNSHOT, 
          3=TE_EXPLOSION (sprites), 4=TE_TAREXPLOSION (purple ver)
          5=TE_LIGHTNING1 (Shambler ver), 6=TE_LIGHTNING2 (Player ver)
          7=TE_WIZSPIKE, 8=TE_KNIGHTSPIKE, 9=TE_LIGHTNING3 (boss ver)
          10=TE_LAVASPLASH (boss wakeup), 11=TE_TELEPORT (sparkles)
-------- SPAWNFLAGS --------
-------- NOTES --------
Spawns a builtin particle effect, will toggle if active
Always starts off, requires triggers to activate

======================================================================*/
void() misc_builtineffects_fire =
{
    // Check if disabled/off first
    if (self.estate & ESTATE_BLOCK) self.state = STATE_OFF;
    if (self.state == STATE_OFF) return;

    // Play builtin effects at target/self location
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, self.count);
    
    self.pos2 = self.origin;

    // If lightning effect (self -> target)
    if (self.count == TE_LIGHTNING1 || self.count == TE_LIGHTNING2 || 
        self.count == TE_LIGHTNING3) {

        // Check if target is a Bmodel? (different origin location)
        if (self.movetarget.bsporigin) self.pos1 = bmodel_origin(self.movetarget);
        else self.pos1 = self.movetarget.origin;
        
        // The lightning model is made from 30 unit sections stitched together
        // reduce the vector length down to 30 unit chunks so it does not
        // poke through the destination object
        self.pos2 = self.pos1 - self.origin;
        self.t_width = vlen(self.pos2);
        if (self.t_width > 30) self.t_length = floor(self.t_width / 30) * 30;
        else self.t_length = self.t_width;
        self.pos2 = normalize(self.pos2);
        self.pos2 = self.origin + (self.pos2 * self.t_length);
    
        WriteEntity (MSG_BROADCAST, self);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }

    WriteCoord (MSG_BROADCAST, self.pos2_x);
    WriteCoord (MSG_BROADCAST, self.pos2_y);
    WriteCoord (MSG_BROADCAST, self.pos2_z);

    // Using the standard for triggers (4 == no extra sound)
    if (self.sounds != 4) {
        // The effect sounds have to come after the write commands
        // otherwise the engine will get confused and think the protocol
        // has changed, there is an exact format for effects
        if (self.count == TE_TELEPORT) {
            self.lip = rint(random()*5);
            if (self.lip == 0) sound (self.movetarget, CHAN_VOICE, "misc/r_tele1.wav", 1, ATTN_NORM);
            else if (self.lip == 1) sound (self.movetarget, CHAN_VOICE, "misc/r_tele2.wav", 1, ATTN_NORM);
            else if (self.lip == 2) sound (self.movetarget, CHAN_VOICE, "misc/r_tele3.wav", 1, ATTN_NORM);
            else if (self.lip == 3) sound (self.movetarget, CHAN_VOICE, "misc/r_tele4.wav", 1, ATTN_NORM);
            else sound (self.movetarget, CHAN_VOICE, "misc/r_tele5.wav", 1, ATTN_NORM);
        }    
        else if (self.count == TE_LIGHTNING1 || self.count == TE_LIGHTNING2 || 
            self.count == TE_LIGHTNING3) {
            // Play lightning sound (LG weapon hit)
            // Stop the sound constantly playing
            if (self.waitmin < time) {
                sound (self.movetarget, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
                self.waitmin = time + 0.6;
            }
        }
        else if (self.count == TE_LAVASPLASH) 
            sound (self.movetarget, CHAN_BODY, "boss1/out1.wav", 1, ATTN_NORM);
        else if (self.count == TE_EXPLOSION || self.count == TE_TAREXPLOSION)
            // Play original explosion sound
            sound(self.movetarget, CHAN_WEAPON, SOUND_REXP3, 1, ATTN_NORM);
    }

    // Continuous mode?
    if (self.wait > 0) {
        self.think = misc_builtineffects_fire;
        self.nextthink = time + self.wait + random()*self.delay;
    }
    // Fire once and switch off
    else self.state = STATE_OFF;
};

//----------------------------------------------------------------------
void() misc_builtineffects_use =
{
    // Check if disabled/off first
    if (self.estate & ESTATE_BLOCK) return;
    
    // Toggle shooter on/off
    if (self.state == STATE_OFF) self.state = STATE_ON;
    else self.state = STATE_OFF;
    
    misc_builtineffects_fire();
};

//----------------------------------------------------------------------
void() misc_builtineffects_reset =
{
    self.state = STATE_OFF;
};

//----------------------------------------------------------------------
void() misc_builtineffects_setup =
{
    // Find any target destinations
    if (self.target != "") self.movetarget = find(world, targetname, self.target);
    else self.movetarget = self;
    
    // Lightning effects need source and target to work
    // Check target is valid before trying to setup this effect
    if (self.count == TE_LIGHTNING1 || self.count == TE_LIGHTNING2 || 
        self.count == TE_LIGHTNING3 && !self.movetarget) {
            dprint("\b[MISC_EFFECTS]\b Missing target for lightning\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            remove(self);
    }

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = misc_builtineffects_use;
    self.estate_reset = misc_builtineffects_reset;
    self.estate = ESTATE_ON;
    self.state = STATE_OFF;
};

//----------------------------------------------------------------------
void() misc_builtineffects =
{
    // Precache sounds for effects
    if (self.count == TE_LAVASPLASH) precache_sound ("boss1/out1.wav");

    self.classtype = CT_PARTICLEEMIT;
    if (self.wait < 0) self.wait = 0;
    if (self.delay < 0) self.delay = 0;
    if (!self.count) self.count = 0;    // def=TE_SPIKE

    self.think = misc_builtineffects_setup;
    self.nextthink = time + 0.1 + random();
};
