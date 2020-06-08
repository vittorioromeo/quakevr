/*======================================================================
 LIGHT ENTITIES (used by compiler)
======================================================================*/
float LIGHT_STARTOFF = 1;
float LIGHT_NOSOUND = 2;
float LIGHT_NOSTATIC = 4;
float LIGHT_FLAMEOFF = 2;
float LIGHT_CANDLESHADOW = 8;
float LIGHT_SWITCHFADE = 16;
float LIGHT_NOEFFECTS = 128;

// To help reduce signon buffer size, allow for light entities
// to be delay spawned (especially light models like candles)
// Cannot use target key because compiler uses it for light styles
.string    lighttarget;

//----------------------------------------------------------------------
void() play_light_sound = 
{
    if (!self.owner) return;
    if (self.owner.estate & ESTATE_BLOCK) return;
    
    self.think = play_light_sound;
    self.nextthink = time + self.owner.waitmin;
    sound (self, CHAN_VOICE, self.owner.noise, self.owner.volume, ATTN_STATIC);
};

//----------------------------------------------------------------------
void() setup_light_sound = 
{
    if (!self.volume) self.volume = 0.5;
    if ( !(self.spawnflags & LIGHT_NOSOUND) && self.noise != "" ) {
        if (self.targetname) {
            self.attachment = spawn();
            self.attachment.owner = self;
            setorigin(self.attachment, self.origin);
            self.attachment.nextthink = time + 1;
            self.attachment.think = play_light_sound;
        }
        else {
            ambientsound (self.origin, self.noise, self.volume, ATTN_QUIET);
        }
    }
};

//----------------------------------------------------------------------
void() setup_light_static =
{
    // Cannot have static entities and particle emitters!
    if (query_configflag(SVR_PARTICLES) == SVR_PARTICLES) return;
    if ( !(self.spawnflags & LIGHT_NOSTATIC) ) makestatic (self);
};

//----------------------------------------------------------------------
void(float pe_style) setup_light_particle =
{
    if (self.spawnflags & LIGHT_NOEFFECTS) return;
    // Spawn particle emitter if particles active and not blocked
    if (query_configflag(SVR_PARTICLES) == SVR_PARTICLES ) {
        self.part_active = pe_style;
        if (self.spawnflags & LIGHT_STARTOFF) self.lip = PARTICLE_START_OFF;
        else self.lip = PARTICLE_START_ON;
        self.part_emitter = spawn_pemitter(self, self, self.part_active, self.lip);
    }
};

//----------------------------------------------------------------------
void() light_part_emitter_on =
{
    if (self.part_emitter) {
        if (ext_dppart && self.part_emitter.state == STATE_OFF)
            pointparticles(particleeffectnum(DPP_BURSTFLAME), self.origin, '0 0 0', 1);
        misc_particle_on(self.part_emitter);
    }
    if (self.attachment) {
        self.attachment.think = play_light_sound;
        self.attachment.nextthink = time + 0.1;
    }
};

//----------------------------------------------------------------------
void() light_on =
{
    self.estate = ESTATE_ON;
    if (self.mdl) setmodel (self, self.mdl);
    lightstyle(self.style, "m");
    light_part_emitter_on();
};

//----------------------------------------------------------------------
void() light_off =
{
    self.estate = ESTATE_OFF;
    if (self.mdl) self.model = "";
    lightstyle(self.style, "a");
    if (self.attachment) 
        sound (self.attachment, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_STATIC);
};

/*======================================================================
/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) LIGHTOFF x x x x x x x
Non-displayed light
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
style : 0 = normal
1 = flicker (first variety)
2 = slow strong pulse
3 = candle (first variety)
4 = fast strobe
5 = gentle pulse
6 = flicker (second variety)
7 = candle (second variety)
8 = candle (third variety)
9 = slow strobe
10 = flourescent flicker
11 = slow pulse, not fading to black
styles 32-62 are assigned by the light program for switchable lights

_color : Light color - R/G/B (def='1 1 1')
wait : Fade distance multiplier (def=1)
delay : Attenuation 0=Linear, 1=inverse, 2=Squared, 3=NoFalloff, 4=Minlight, 5=Good Ambient

mangle : Spotlight direction
angle : Spotlight cone angle (def=40)

_dirt : Dirt map (override)
_dirtscale : Dirt scale (override)
_deviance : Deviance
_samples : No. of Deviance lights (def=16)
_surface : Surface light texture name
_surface_offset : Surface light offset (def=2)
_softangle : Spotlight soft angle (def=0)
_anglescale : Light angle scale (def=0.5)
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
-------- NOTES --------
Non-displayed light. If targeted, it will toggle between on or off

======================================================================*/
void() light =
{
    // If no targetname then inert light, safe to remove
    if (self.targetname == "") {
        remove(self);
        return;
    }
    
    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
};

/*======================================================================
/*QUAKED light_fluoro (0 1 0) (-8 -8 -8) (8 8 8) LIGHTOFF NO_SOUND x x x x x x
Non-displayed light. Makes steady fluorescent humming sound
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
NO_SOUND : No ambient sound
-------- NOTES --------
Non-displayed light. Makes steady fluorescent humming sound

======================================================================*/
void() light_fluoro =
{
    self.noise = "ambience/fl_hum1.wav";
    precache_sound (self.noise);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    
    setup_light_sound();
};

/*======================================================================
/*QUAKED light_fluorospark (0 1 0) (-8 -8 -8) (8 8 8) x NO_SOUND x x x x x x
Non-displayed light. Makes sparking, broken fluorescent sound
-------- KEYS --------
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
NO_SOUND  : No ambient sound
-------- NOTES --------
Non-displayed light. Makes sparking, broken fluorescent sound

======================================================================*/
void() light_fluorospark =
{
    self.noise = "ambience/buzz1.wav";
    precache_sound (self.noise);

    if (!self.style) self.style = 10;
    setup_light_sound();
};

/*======================================================================
/*QUAKED light_globe (0 1 0) (-8 -8 -8) (8 8 8) LIGHTOFF x NO_STATIC x x x X x
Sphere globe light (sprite)
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
NO_STATIC : Will not turn into static entity
-------- NOTES --------
Sphere globe light (sprite)

======================================================================*/
void() light_globe =
{
    self.mdl = "progs/s_light.spr";
    precache_model (self.mdl);
    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};

/*======================================================================
/*QUAKED light_torch_small_walltorch (0 .5 0) (-10 -10 -20) (10 10 20) LIGHTOFF NO_SOUND NO_STATIC x x x x x
Short wall torch
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
NO_SOUND  : No ambient sound
NO_STATIC : Will not turn into static entity
-------- NOTES --------
Short wall torch

======================================================================*/
void() light_torch_on =
{
    self.frame = 0;
    self.estate = ESTATE_ON;
    lightstyle(self.style, "m");
    light_part_emitter_on();
};

//----------------------------------------------------------------------
void() light_torch_off =
{
    self.frame = 1;
    self.estate = ESTATE_OFF;
    lightstyle(self.style, "a");
    if (self.attachment) 
        sound (self.attachment, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_STATIC);
};

//----------------------------------------------------------------------
void() light_torch_small_walltorch =
{
    self.mdl = "progs/flame.mdl";
    precache_model (self.mdl);
    if (self.noise == "") self.noise = "ambience/fire1.wav";
    precache_sound (self.noise);
    if (!self.waitmin) self.waitmin = 2.165;

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y < 0 || (self.angles_y == 0 && !query_configflag(SVR_ITEMROTATE)) ) 
        self.angles_y = rint(random()*359);

    setup_light_sound();
    setup_light_particle(PARTICLE_STYLE_FLAMET);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_torch_on;
        self.estate_off = light_torch_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};

/*======================================================================
/*QUAKED light_flame_large_yellow (0 1 0) (-8 -8 -8) (8 8 8) LIGHTOFF NO_SOUND NO_STATIC x x x x x
Large yellow flame
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
NO_SOUND  : No ambient sound
NO_STATIC : Will not turn into static entity
-------- NOTES --------
Large yellow flame

======================================================================*/
void() light_flame_large_yellow =
{
    self.mdl = "progs/flame2.mdl";
    precache_model (self.mdl);
    if (self.noise == "") self.noise = "ambience/fire1.wav";
    precache_sound (self.noise);
    if (!self.waitmin) self.waitmin = 2.165;
    
    // If DP engine active remove candle model shadow
    if (engine == ENG_DPEXT) self.effects = self.effects + EF_NOSHADOW;

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    self.frame = 1;            // Switch to frame group 2 (larger flame)
    if (!self.alpha) self.alpha = 0.8;
    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y < 0 || (self.angles_y == 0 && !query_configflag(SVR_ITEMROTATE)) ) 
        self.angles_y = rint(random()*359);

    setup_light_sound();
    setup_light_particle(PARTICLE_STYLE_FLAMEL);
    
    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};

/*======================================================================
/*QUAKED light_flame_small_yellow (0 1 0) (-8 -8 -8) (8 8 8) LIGHTOFF NO_SOUND NO_STATIC x x x X x
Small yellow flames
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
NO_SOUND  : No ambient sound
NO_STATIC : Will not turn into static entity
-------- NOTES --------
Small yellow flames.

======================================================================*/
void() light_flame_small_yellow =
{
    self.mdl = "progs/flame2.mdl";
    precache_model (self.mdl);
    if (self.noise == "") self.noise = "ambience/fire1.wav";
    precache_sound (self.noise);
    self.part_active = PARTICLE_STYLE_FLAMES;
    if (!self.waitmin) self.waitmin = 2.165;

    // If DP engine active remove candle model shadow
    if (engine == ENG_DPEXT) self.effects = self.effects + EF_NOSHADOW;

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    if (!self.alpha) self.alpha = 0.8;
    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y < 0 || (self.angles_y == 0 && !query_configflag(SVR_ITEMROTATE)) ) 
        self.angles_y = rint(random()*359);

    setup_light_sound();
    setup_light_particle(PARTICLE_STYLE_FLAMES);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};
//Duplicate (small yellow flame)
void() light_flame_small_white = { light_flame_small_yellow(); };

/*======================================================================
/*QUAKED light_postlight (0 .5 0) (-10 -10 -20) (10 10 20) LIGHTOFF x x x x x x x
light post (from Quoth)
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
-------- NOTES --------
light post (from Quoth)
======================================================================*/
void() light_postlight =
{
    self.mdl = "progs/misc_lightpost.mdl";
    precache_model (self.mdl);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setmodel (self, self.mdl);
};

/*======================================================================
/*QUAKED light_tubelight (0 .5 0) (-10 -10 -20) (10 10 20) LIGHTOFF x x x x x x x
light Tube (from Quoth)
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
-------- NOTES --------
light Tube (from Quoth)
======================================================================*/
void() light_tubelight =
{
    self.mdl = "progs/misc_lighttube.mdl";
    precache_model (self.mdl);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setmodel (self, self.mdl);
};

/*======================================================================
/*QUAKED light_fixture1 (0 .5 0) (-10 -10 -20) (10 10 20) LIGHTOFF x x x x x x x
light Fixture (from Rubicon)
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
light : sets brightness, 300 is default
fixangle : angle the model should be facing; set it to face away from the wall
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
-------- NOTES --------
light Fixture (from Rubicon)
======================================================================*/
void() light_fixture1 =
{
    self.mdl = "progs/misc_fixture1.mdl";
    precache_model (self.mdl);
    self.angles_y = self.fixangle;

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setmodel (self, self.mdl);
};

/*======================================================================
/*QUAKED light_candle (0 1 0) (-4 -4 -16) (4 4 16) LIGHTOFF FLAMEOFF x SHADOW x x x x
Wide variety of Candles (from Rogue Software)
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
angle      : 0=random angle, 1-360=specific
height     : size of candle 1=short, 2=medium, 3=regular, -1=random
t_width    : width of candle 1=regular, 2=fat, 3=thin, -1=random
lip        : Moves model down to ground plane (default = 16)
light      : sets brightness, 300 is default
-------- SPAWNFLAGS --------
LIGHTOFF : Starts off and waits for trigger
FLAMEOFF : The candle is displayed off (no light/flame)
SHADOW   : Turn on shadows in DP engine
-------- NOTES --------
Wide variety of Candles (from Rogue Software)

======================================================================*/
void() light_candle_on =
{
    self.estate = ESTATE_ON;
    self.frame = 1 + (self.t_width-1)*2;
    lightstyle(self.style, "m");
};

//----------------------------------------------------------------------
void() light_candle_off =
{
    self.estate = ESTATE_OFF;
    self.frame = (self.t_width-1)*2;
    lightstyle(self.style, "a");
};
//----------------------------------------------------------------------
void() light_candle_setup =
{
    // Setup model and no world interaction
    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);

    // If DP engine active remove candle model shadow
    if (engine == ENG_DPEXT && !(self.spawnflags & LIGHT_CANDLESHADOW)) 
        self.effects = self.effects + EF_NOSHADOW;

    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y <= 0) self.angles_y = rint(random()*359);
    
    // Setup light
    if (self.spawnflags & LIGHT_FLAMEOFF) {
        self.estate = ESTATE_OFF;
        self.frame = (self.t_width-1)*2;
    }
    
    // Is switchable light style defined?
    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_candle_on;
        self.estate_off = light_candle_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    // Check for static option, after switchable lights
    else if (self.spawnflags & ENT_SPNSTATIC) makestatic(self);
};

//----------------------------------------------------------------------
void() light_candle =
{
    // Cannot have randomly selected models for prechache
    // Quickload will re-run all spawn functions and try to select
    // a different candle model which is not precached
    precache_model ("progs/misc_candle1.mdl");
    precache_model ("progs/misc_candle2.mdl");
    precache_model ("progs/misc_candle3.mdl");

    // Check for size 1=small, 2=medium, 3=regular (def), -1=random
    if (self.height < 0) self.height = 1 + rint(random()*2);
    if (self.height == 1) self.mdl = "progs/misc_candle1.mdl";
    else if (self.height == 2) self.mdl = "progs/misc_candle2.mdl";
    else self.mdl = "progs/misc_candle3.mdl";
    
    // Check for width 1=regular (def), 2=fat, 3=thin, -1=random
    // The models are split up into three groups of off/on
    if (self.t_width < 0) self.t_width = 1 + rint(random()*2);
    else if (self.t_width == 0 || self.t_width > 3) self.t_width = 1;

    self.modelindex = 0;
    self.model = string_null;                // hide bmodel surface
    self.frame = 1 + (self.t_width-1)*2;    // Def = on state
    self.movetype = MOVETYPE_NONE;            // Create baseline
    self.solid = SOLID_NOT;
    self.nextthink = 0;

    // The model should not be setup flush to floor because it is a light
    // source and will look really odd, setup entity 16 units above floor
    // and the "lip" key will move it back down again
    if (!self.lip) self.lip = 16;
    self.origin_z = self.origin_z - self.lip;

    // Check for static entity option first
    if (self.spawnflags & ENT_SPNSTATIC) light_candle_setup();
    else {
        // Check for spawn delay on model (signon buffer issues)
        // Code left so light_candles can be switched on/off
        if (self.lighttarget != "" && !self.style) {
            self.targetname = self.lighttarget;
            // Use the entity state system for max compatibility
            self.estate = ESTATE_OFF;
            self.use = entity_state_use;
            self.estate_on = light_candle_setup;
        }
        else light_candle_setup();
    }
};


//----------------------------------------------------------------------
void() light_flame_brazier_short = 
{
    self.mdl = "progs/brazshrt.mdl";
    precache_model (self.mdl);
    if (self.noise == "") self.noise = "ambience/fire1.wav";
    precache_sound (self.noise);
    if (!self.waitmin) self.waitmin = 2.165;

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y < 0 || (self.angles_y == 0 && !query_configflag(SVR_ITEMROTATE)) ) 
        self.angles_y = rint(random()*359);

    setup_light_sound();
    setup_light_particle(PARTICLE_STYLE_FLAMET);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_torch_on;
        self.estate_off = light_torch_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};

//----------------------------------------------------------------------
void() light_flame_brazier_tall = 
{
    self.mdl = "progs/braztall.mdl";
    precache_model (self.mdl);
    if (self.noise == "") self.noise = "ambience/fire1.wav";
    precache_sound (self.noise);
    if (!self.waitmin) self.waitmin = 2.165;

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y < 0 || (self.angles_y == 0 && !query_configflag(SVR_ITEMROTATE)) ) 
        self.angles_y = rint(random()*359);

    setup_light_sound();
    setup_light_particle(PARTICLE_STYLE_FLAMET);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_torch_on;
        self.estate_off = light_torch_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};

//----------------------------------------------------------------------
void() light_torch_long_walltorch = 
{
    self.mdl = "progs/longtrch.mdl";
    precache_model (self.mdl);
    if (self.noise == "") self.noise = "ambience/fire1.wav";
    precache_sound (self.noise);
    if (!self.waitmin) self.waitmin = 2.165;

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y < 0 || (self.angles_y == 0 && !query_configflag(SVR_ITEMROTATE)) ) 
        self.angles_y = rint(random()*359);

    setup_light_sound();
    setup_light_particle(PARTICLE_STYLE_FLAMET);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_torch_on;
        self.estate_off = light_torch_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};

void () light_lantern = {
    self.mdl = "progs/lantern.mdl";
    precache_model (self.mdl);
    self.angles_y = self.fixangle;

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setmodel (self, self.mdl);
};

void () light_lantern2 = {
    self.mdl = "progs/misc_lantern.mdl";
    precache_model(self.mdl);
    if (self.noise == "") self.noise = "ambience/fire1.wav";
    precache_sound (self.noise);
    if (!self.waitmin) self.waitmin = 2.165;

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    // Setting the angle key in the editor to UP/DOWN = random rotation    
    if (self.angles_y < 0 || (self.angles_y == 0 && !query_configflag(SVR_ITEMROTATE)) ) 
        self.angles_y = rint(random()*359);

    setup_light_sound();
    setup_light_particle(PARTICLE_STYLE_FLAMET);

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_torch_on;
        self.estate_off = light_torch_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setup_light_static();
};


//===============================================================================
//light_beacon
//===============================================================================
/*QUAKED light_beacon (0 1 0) (-8 -8 -36) (8 8 8) BLINKING
floor-mounted flashing red beacon
Default light value is 300
Default style is 0
Flags:
1 : "Blinking" if you want the beacon to blink (Set style to "16" to match the skin animation.) */

void() light_beacon = {
    if (self.spawnflags & 1) self.skin = 1;

    self.mdl = "progs/beacon.mdl";
    precache_model (self.mdl);
    self.angles_y = self.fixangle;

    if (self.style >= 32) {
        // Setup Entity State functionality
        if (self.targetname != "") self.use = entity_state_use;
        self.estate_on = light_on;
        self.estate_off = light_off;
        if (self.spawnflags & LIGHT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
    else setmodel (self, self.mdl);
};

void() bug_bob = {
    self.nextthink = time + 0.1;
    self.think = bug_bob;
    if (self.multiplier > 1) self.multiplier = self.multiplier - 0.15;
    if (self.multiplier < 1) self.multiplier = 1;
    self.velocity = self.velocity * 0.8 + (self.oldorigin - self.origin) * 0.2;
    if (random() < 0.030000 && self.ltime < time) {
        self.ltime = time + 6;
        self.multiplier = 4.5;
    }
    self.velocity = normalize(self.velocity);
    self.velocity = self.velocity * ((20 + random() * 8) * self.multiplier);
};

void() make_bugs = {
    local entity bug;
    local vector svec;
    bug = spawn();
    bug.oldorigin = self.origin;
    bug.movetype = MOVETYPE_NOCLIP;
    bug.solid = SOLID_NOT;
    bug.velocity_x = 20 + random() * -40;
    bug.velocity_y = 20 + random() * -40;
    bug.velocity_z = 20 + random() * -40;
    bug.nextthink = time + 0.1;
    bug.think = bug_bob;
    bug.classname = "lightchunk";
    bug.frame = 0;
    bug.cnt = 0;
    if (self.count > 1)
        setmodel(bug, "progs/marshsm.spr");
    else
        setmodel(bug, "progs/marsh.spr");

    bug.frame = self.col;
    svec_x = 20 + random() * -40;
    svec_y = 20 + random() * -40;
    svec_z = 20 + random() * -40;
    svec = self.origin + svec;
    setorigin(bug, svec);
    setsize(bug, VEC_ORIGIN, VEC_ORIGIN);
    if (self.count < 5) {
        self.nextthink = time + 0.1;
        self.think = make_bugs;
    } else {
        remove(self);
    }
    self.count = self.count + 1;
};

void() light_marsh = {
    precache_model("progs/marsh.spr");
    precache_model("progs/marshsm.spr");
    make_bugs();
};